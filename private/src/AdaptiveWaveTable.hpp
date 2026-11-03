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

// Immutable band limits are shared by all channel counts and sample types.
//
struct AdaptiveWaveTableLevels
{
    static constexpr size_t x_maxLevels = 25;
    static constexpr size_t x_maxComponents = DiscreteFourierTransform::x_maxComponents;
    static constexpr size_t x_lookupSize = x_maxComponents + 1;
    size_t m_components[x_maxLevels]{};
    size_t m_lookup[x_lookupSize]{};

    constexpr AdaptiveWaveTableLevels()
    {
        float components = 3;
        m_components[0] = 1;
        for (size_t i = 1; i < x_maxLevels; ++i)
        {
            m_components[i] = std::min(static_cast<size_t>(components), x_maxComponents);
            components = std::max(components * 1.3f, static_cast<float>(static_cast<size_t>(components) + 1));
        }

        for (size_t count = 0; count < x_lookupSize; ++count)
        {
            size_t level = 0;
            while (level + 1 < x_maxLevels && m_components[level + 1] < count)
            {
                ++level;
            }

            m_lookup[count] = level;
        }
    }
};

inline constexpr AdaptiveWaveTableLevels x_adaptiveWaveTableLevels;

template<typename Number, size_t Size>
struct AdaptiveWaveTableEvalSiteGeneric
{
    int m_lower[Size]{};
    int m_upper[Size]{};
    MultiChannelNumber<Number, Size> m_wayThrough;
};

template<typename Number, size_t Size>
struct AdaptiveWaveTableGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    using WaveTable = BasicWaveTableGeneric<Number, Size, 10>;
    using DFT = DiscreteFourierTransformGeneric<Number, Size, 10>;
    using EvalSite = AdaptiveWaveTableEvalSiteGeneric<Number, Size>;
    using Lookup = typename WaveTable::Lookup;
    using MonoTable = AdaptiveWaveTableGeneric<Number, 1>;
    static constexpr size_t x_maxLevels = AdaptiveWaveTableLevels::x_maxLevels;
    static constexpr size_t x_lookupSize = AdaptiveWaveTableLevels::x_lookupSize;
    inline static constexpr auto& s_levelComponents = x_adaptiveWaveTableLevels.m_components;
    inline static constexpr auto& s_componentLookup = x_adaptiveWaveTableLevels.m_lookup;

    WaveTable m_waveTable;
    DFT m_dft;
    WaveTable m_levels[x_maxLevels];
    bool m_waveTableReady = false;
    bool m_dftReady = false;
    size_t m_levelsReady = 0;

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

    WaveTable& GetWaveTable()
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

    static void EvaluateSite(const Vector& freq, const Vector& maxFreq, EvalSite& site)
    {
        Vector positive = Vector::IfLess(Vector(), freq, freq, Vector::Splat(1));
        Vector maxComponents = Vector::Min(maxFreq / positive, Vector::Splat(DFT::x_maxComponents));
        maxComponents = Vector::IfLess(Vector(), freq, maxComponents, Vector::Splat(DFT::x_maxComponents));
        maxComponents = Vector::Max(Vector(), maxComponents);
        Vector lowerComponents;
        Vector spans = Vector::Splat(1);
        Vector blend;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            site.m_lower[lane] = -1;
            site.m_upper[lane] = -1;
            if (maxFreq[lane] < freq[lane])
            {
                continue;
            }

            size_t level = s_componentLookup[static_cast<size_t>(maxComponents[lane])];
            if (level + 1 < x_maxLevels && s_levelComponents[level + 1] <= maxComponents[lane])
            {
                ++level;
            }

            site.m_lower[lane] = static_cast<int>(level);
            if (level + 1 < x_maxLevels && maxComponents[lane] >= s_levelComponents[level]
                && maxComponents[lane] < s_levelComponents[level + 1])
            {
                site.m_upper[lane] = static_cast<int>(level + 1);
                lowerComponents[lane] = static_cast<Number>(s_levelComponents[level]);
                spans[lane] = static_cast<Number>(s_levelComponents[level + 1] - s_levelComponents[level]);
                blend[lane] = 1;
            }
        }

        site.m_wayThrough = (maxComponents - lowerComponents) / spans * blend;
    }

    // Only address-dependent loads are scalar; phase and level interpolation
    // run across every lane together. The reader supports owned vector tables
    // and independently allocated mono tables without copying table storage.
    //
    template<typename Read>
    static Vector EvaluateWithRead(const Lookup& lookup, const EvalSite& site, const Read& read)
    {
        Vector lower0, lower1, upper0, upper1;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            size_t index = lookup.m_index[lane];
            size_t next = (index + 1) % WaveTable::x_tableSize;
            if (site.m_lower[lane] >= 0)
            {
                lower0[lane] = read(site.m_lower[lane], index, lane);
                lower1[lane] = read(site.m_lower[lane], next, lane);
            }

            if (site.m_upper[lane] >= 0)
            {
                upper0[lane] = read(site.m_upper[lane], index, lane);
                upper1[lane] = read(site.m_upper[lane], next, lane);
            }
        }

        Vector left = lower0 * (Vector::Splat(1) - lookup.m_fraction) + lower1 * lookup.m_fraction;
        Vector right = upper0 * (Vector::Splat(1) - lookup.m_fraction) + upper1 * lookup.m_fraction;
        return left * (Vector::Splat(1) - site.m_wayThrough) + right * site.m_wayThrough;
    }

    Vector Evaluate(const Vector& phase, const EvalSite& site) const
    {
        return EvaluateWithRead(Lookup(phase), site, [this](int level, size_t index, size_t lane)
        {
            return m_levels[level].m_table[index][lane];
        });
    }

    static Vector Evaluate(const Lookup& lookup, const EvalSite& site, MonoTable* const* tables)
    {
        return EvaluateWithRead(lookup, site, [tables](int level, size_t index, size_t lane)
        {
            return tables[lane]->m_levels[level].m_table[index][0];
        });
    }

    Vector StartValue() const
    {
        return m_waveTable.StartValue();
    }

    Vector CenterValue() const
    {
        return m_waveTable.CenterValue();
    }
};

using AdaptiveWaveTable = AdaptiveWaveTableGeneric<float, 1>;
