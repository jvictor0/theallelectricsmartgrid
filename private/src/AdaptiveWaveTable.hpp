#pragma once

#include <complex>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <algorithm>

#include "HostStubs.hpp"
#include "BasicWaveTable.hpp"
#include "Math.hpp"
#include "MultiChannelComplex.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"

// Transforms each channel of a wave table independently, using MultiChannelNumber
// samples and MultiChannelComplex frequency components.
//
template<typename Number, size_t Size, size_t Bits>
struct DiscreteFourierTransformGeneric
{
    using Table = BasicWaveTableGeneric<Number, Size, Bits>;
    using Vector = MultiChannelNumber<Number, Size>;
    using Complex = std::complex<Number>;
    using ComplexVector = MultiChannelComplex<Number, Size>;

    static constexpr size_t x_tableSize = Table::x_tableSize;
    static constexpr size_t x_maxComponents = x_tableSize / 2;
    static constexpr int x_partialKernelRadius = 8;
    ComplexVector m_components[x_maxComponents];

    DiscreteFourierTransformGeneric()
    {
        Init();
    }

    void Init()
    {
        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_components[i] = ComplexVector();
        }
    }

    void Transform(const Table& waveTable)
    {
        // Pre-allocated workspace to avoid dynamic allocation
        //
        ComplexVector workspace[x_tableSize];
        Transform(waveTable, workspace);
    }

    // The workspace holds x_tableSize values and is overwritten.
    //
    void Transform(const Table& waveTable, ComplexVector* workspace)
    {
        // Clear existing components
        //
        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_components[i] = ComplexVector();
        }

        // Convert input to complex numbers
        //
        const size_t x_N = x_tableSize;
        Vector zero = Vector::Splat(0);
        for (size_t i = 0; i < x_N; ++i)
        {
            workspace[i] = ComplexVector(waveTable.m_table[i], zero);
        }

        // Perform in-place FFT
        //
        FFT(workspace, x_N);

        // Store FFT results directly as complex numbers
        //
        for (size_t k = 0; k < x_maxComponents; ++k)
        {
            m_components[k] = workspace[k] / static_cast<Number>(x_N);
        }
    }

    void FFT(ComplexVector* data, size_t N)
    {
        if (N <= 1)
        {
            return;
        }

        // Bit-reversal permutation
        //
        for (size_t i = 1, j = 0; i < N; ++i)
        {
            size_t bit = N >> 1;
            for (; j & bit; bit >>= 1)
            {
                j ^= bit;
            }
            j ^= bit;
            if (i < j)
            {
                std::swap(data[i], data[j]);
            }
        }

        // Iterative butterfly operations
        //
        for (size_t len = 2; len <= N; len <<= 1)
        {
            size_t halfLen = len >> 1;
            for (size_t i = 0; i < N; i += len)
            {
                for (size_t j = 0; j < halfLen; ++j)
                {
                    size_t rootOfUnityIndex = (x_tableSize - j * x_tableSize / len) % x_tableSize;
                    ComplexVector twiddle = ComplexVector::Splat(MathGeneric<Bits>::RootOfUnityByIndex(rootOfUnityIndex));
                    ComplexVector t = data[i + j + halfLen] * twiddle;
                    ComplexVector u = data[i + j];
                    data[i + j] = u + t;
                    data[i + j + halfLen] = u - t;
                }
            }
        }
    }

    template<size_t OutputBits>
    void InverseTransform(BasicWaveTableGeneric<Number, Size, OutputBits>& output, size_t maxComponents) const
    {
        ComplexVector workspace[BasicWaveTableGeneric<Number, Size, OutputBits>::x_tableSize];
        InverseTransform(output, maxComponents, workspace);
    }

    // Evaluates the band-limited spectrum at 1 << OutputBits points. A larger
    // output interpolates the same spectrum: bins keep their indices, added
    // bins stay zero, and the forward 1 / x_tableSize normalization is kept.
    // The source Nyquist bin is not stored and stays omitted. The workspace
    // holds 1 << OutputBits values and is overwritten.
    //
    template<size_t OutputBits>
    void InverseTransform(BasicWaveTableGeneric<Number, Size, OutputBits>& output, size_t maxComponents, ComplexVector* workspace) const
    {
        static_assert(Bits <= OutputBits, "InverseTransform output must be at least the source size");
        constexpr size_t x_outputSize = BasicWaveTableGeneric<Number, Size, OutputBits>::x_tableSize;

        // Limit components to available range
        //
        size_t componentsToUse = ((maxComponents + 1) > x_maxComponents) ? x_maxComponents : maxComponents + 1;

        for (size_t k = 0; k < x_outputSize; ++k)
        {
            workspace[k] = ComplexVector();
        }

        for (size_t k = 0; k < componentsToUse; ++k)
        {
            workspace[k] = m_components[k];
            if (0 < k)
            {
                workspace[x_outputSize - k] = m_components[k].Conj();
            }
        }

        IFFT<OutputBits>(workspace);

        for (size_t i = 0; i < x_outputSize; ++i)
        {
            output.m_table[i] = workspace[i].real();
        }
    }

    // Unnormalized inverse FFT of 1 << OutputBits values, with twiddles from
    // MathGeneric<OutputBits>.
    //
    template<size_t OutputBits>
    static void IFFT(ComplexVector* data)
    {
        constexpr size_t x_N = static_cast<size_t>(1) << OutputBits;

        // Bit-reversal permutation
        //
        for (size_t i = 1, j = 0; i < x_N; ++i)
        {
            size_t bit = x_N >> 1;
            for (; j & bit; bit >>= 1)
            {
                j ^= bit;
            }
            j ^= bit;
            if (i < j)
            {
                std::swap(data[i], data[j]);
            }
        }

        // Iterative butterfly operations (IFFT uses positive exponent)
        //
        for (size_t len = 2; len <= x_N; len <<= 1)
        {
            size_t halfLen = len >> 1;
            for (size_t i = 0; i < x_N; i += len)
            {
                for (size_t j = 0; j < halfLen; ++j)
                {
                    size_t rootOfUnityIndex = j * x_N / len;
                    ComplexVector twiddle = ComplexVector::Splat(MathGeneric<OutputBits>::RootOfUnityByIndex(rootOfUnityIndex));
                    ComplexVector t = data[i + j + halfLen] * twiddle;
                    ComplexVector u = data[i + j];
                    data[i + j] = u + t;
                    data[i + j + halfLen] = u - t;
                }
            }
        }
    }

    // One partial at a shared frequency, with per-channel phase and magnitude.
    //
    void WriteWindowedPartial(const Vector& phase, const Vector& magnitude, Number exactFrequency)
    {
        ComplexVector phaseFactor(magnitude * MathGeneric<Bits>::Cos2pi(phase), magnitude * MathGeneric<Bits>::Sin2pi(phase));
        WriteHannKernel(phaseFactor, exactFrequency);
    }

    // One partial at a shared frequency and phase, with per-channel magnitude,
    // such as a panned partial. The phase is evaluated once for all channels.
    //
    void WriteWindowedPartial(Number phase, const Vector& magnitude, Number exactFrequency)
    {
        ComplexVector phaseFactor(magnitude * MathGeneric<Bits>::Cos2pi(phase), magnitude * MathGeneric<Bits>::Sin2pi(phase));
        WriteHannKernel(phaseFactor, exactFrequency);
    }

    void WriteHannKernel(const ComplexVector& phaseFactor, Number exactFrequency)
    {
        Number exactBin = exactFrequency * static_cast<Number>(x_tableSize);
        int centerBin = static_cast<int>(std::floor(exactBin));
        int firstBin = std::max(1, centerBin - x_partialKernelRadius);
        int lastBin = std::min(static_cast<int>(x_maxComponents) - 1, centerBin + x_partialKernelRadius);

        for (int k = firstBin; k <= lastBin; ++k)
        {
            Number offset = exactBin - static_cast<Number>(k);
            m_components[k] += phaseFactor * ComplexVector::Splat(MathGeneric<Bits>::HannKernel(offset));
        }
    }

    void WriteBinCenteredWindowedPartial(size_t bin, const ComplexVector& value)
    {
        if (bin == 0 || x_maxComponents <= bin)
        {
            return;
        }

        m_components[bin] += value * 0.5f;
        if (1 < bin)
        {
            m_components[bin - 1] += value * -0.25f;
        }

        if (bin + 1 < x_maxComponents)
        {
            m_components[bin + 1] += value * -0.25f;
        }
    }
};

using DiscreteFourierTransform = DiscreteFourierTransformGeneric<float, 1, 10>;
using DiscreteFourierTransform4096 = DiscreteFourierTransformGeneric<float, 1, 12>;

struct AdaptiveWaveTable
{
    static constexpr size_t x_maxLevels = 25;
    static constexpr float m_levelsBase = 1.3f;
    static constexpr size_t x_lookupSize = DiscreteFourierTransform::x_maxComponents + 1;
    
    BasicWaveTable m_waveTable;
    DiscreteFourierTransform m_dft;
    BasicWaveTable m_levels[x_maxLevels];
    
    static size_t s_levelComponents[x_maxLevels];
    static size_t s_componentLookup[x_lookupSize];
    static bool s_isInitialized;

    bool m_waveTableReady;
    bool m_dftReady;
    size_t m_levelsReady;
    size_t m_lastLevel;
    
    AdaptiveWaveTable()
        : m_waveTableReady(false)
        , m_dftReady(false)
        , m_levelsReady(0)
        , m_lastLevel(0)
    {
        InitializeStatic();
    }
    
    static void InitializeStatic()
    {
        if (s_isInitialized)
        {
            return;
        }
        
        // Initialize level components
        //
        float components = 3;
        s_levelComponents[0] = 1;
        for (size_t i = 1; i < x_maxLevels; ++i)
        {
            s_levelComponents[i] = static_cast<size_t>(components);
            components = std::max(components * m_levelsBase, std::floor(components) + 1);
            s_levelComponents[i] = std::min(s_levelComponents[i], DiscreteFourierTransform::x_maxComponents);
            assert(s_levelComponents[i] < BasicWaveTable::x_tableSize);
        }
        
        // Initialize lookup table
        //
        for (size_t maxComp = 0; maxComp < x_lookupSize; ++maxComp)
        {
            size_t level = 0;
            while (level < x_maxLevels - 1 && s_levelComponents[level + 1] < maxComp)
            {
                ++level;
            }
            
            s_componentLookup[maxComp] = level;
        }
        
        s_isInitialized = true;
    }

    bool IsReady() const
    {
        return m_waveTableReady && m_dftReady && m_levelsReady == x_maxLevels;
    }

    void Init()
    {
        m_waveTable.Init();
        m_dft.Init();
        for (size_t i = 0; i < x_maxLevels; ++i)
        {
            m_levels[i].Init();
        }

        m_waveTableReady = false;
        m_dftReady = false;
        m_levelsReady = 0;
    }

    BasicWaveTable& GetWaveTable()
    {
        return m_waveTable;
    }

    void Generate()
    {
        m_dft.Transform(m_waveTable);
        m_dftReady = true;
        GenerateLevels();
    }

    void GenerateLevels()
    {
        for (size_t i = 0; i < x_maxLevels; ++i)
        {
            m_dft.InverseTransform(m_levels[i], s_levelComponents[i]);
            m_levels[i].NormalizeAmplitude();
        }

        m_levelsReady = x_maxLevels;
    }

    struct EvalSite
    {
        int m_lower;
        int m_upper;
        float m_wayThrough;

        void Set(int lower, int upper, float wayThrough)
        {
            m_lower = lower;
            m_upper = upper;
            m_wayThrough = wayThrough;
        }
    };

    void EvaluateSite(float freq, float maxFreq, EvalSite& site)
    {
        if (maxFreq < freq)
        {
            site.Set(-1, -1, 0);
            return;
        }

        float maxComponents = maxFreq / freq;
        size_t maxCompInt = static_cast<size_t>(maxComponents);
        maxCompInt = std::min(maxCompInt, DiscreteFourierTransform::x_maxComponents);
        
        // Use lookup table, check if fractional part pushes us to next level
        //
        size_t level = s_componentLookup[maxCompInt];
        if (level < x_maxLevels - 1 && s_levelComponents[level + 1] <= maxComponents)
        {
            ++level;
        }
        
        if (level != m_lastLevel)
        {
            m_lastLevel = level;
        }

        if (level == x_maxLevels - 1)
        {
            site.Set(x_maxLevels - 1, -1, 0);
        }
        else if (maxComponents >= s_levelComponents[level] && maxComponents < s_levelComponents[level + 1])
        {
            // Interpolate between level and level + 1
            //
            float wayThrough = static_cast<float>(maxComponents - s_levelComponents[level]) / static_cast<float>(s_levelComponents[level + 1] - s_levelComponents[level]);
            site.Set(static_cast<int>(level), static_cast<int>(level + 1), wayThrough);
        }
        else
        {
            site.Set(static_cast<int>(level), -1, 0);
        }
    }
    
    float Evaluate(float phase, EvalSite& site)
    {
        if (site.m_lower == -1)
        {
            return 0;
        }
        else if (site.m_upper == -1)
        {
            return m_levels[site.m_lower].Evaluate(phase);
        }
        else
        {
            return m_levels[site.m_lower].Evaluate(phase) * (1 - site.m_wayThrough) + m_levels[site.m_upper].Evaluate(phase) * site.m_wayThrough;
        }
    }

    float StartValue() const
    {
        return m_waveTable.StartValue();
    }
    
    float CenterValue() const
    {
        return m_waveTable.CenterValue();
    }
};

inline size_t AdaptiveWaveTable::s_levelComponents[AdaptiveWaveTable::x_maxLevels];
inline size_t AdaptiveWaveTable::s_componentLookup[AdaptiveWaveTable::x_lookupSize];
inline bool AdaptiveWaveTable::s_isInitialized = false;
