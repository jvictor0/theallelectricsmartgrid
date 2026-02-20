#pragma once

#include <complex>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <algorithm>

#include "ios_stubs.hpp"
#include "BasicWaveTable.hpp"
#include "Math.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"

template<size_t Bits>
struct DiscreteFourierTransformGeneric
{
    static constexpr size_t x_maxComponents = BasicWaveTableGeneric<Bits>::x_tableSize / 2;
    std::complex<float> m_components[x_maxComponents];

    DiscreteFourierTransformGeneric()
    {
        Init();
    }

    void Init()
    {
        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_components[i] = std::complex<float>(0.0f, 0.0f);
        }
    }

    void Transform(const BasicWaveTableGeneric<Bits>& waveTable)
    {
        // Clear existing components
        //
        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_components[i] = std::complex<float>(0.0f, 0.0f);
        }

        // Pre-allocated workspace to avoid dynamic allocation
        //
        std::complex<float> workspace[BasicWaveTableGeneric<Bits>::x_tableSize];

        // Convert input to complex numbers
        //
        const size_t x_N = BasicWaveTableGeneric<Bits>::x_tableSize;
        for (size_t i = 0; i < x_N; ++i)
        {
            workspace[i] = std::complex<float>(waveTable.m_table[i], 0.0f);
        }

        // Perform in-place FFT
        //
        FFT(workspace, x_N);

        // Store FFT results directly as complex numbers
        //
        for (size_t k = 0; k < x_maxComponents; ++k)
        {
            m_components[k] = workspace[k] / static_cast<float>(x_N);
        }
    }

    void FFT(std::complex<float>* data, size_t N)
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
                    size_t rootOfUnityIndex = (BasicWaveTableGeneric<Bits>::x_tableSize - j * BasicWaveTableGeneric<Bits>::x_tableSize / len) % BasicWaveTableGeneric<Bits>::x_tableSize;
                    std::complex<float> t = MathGeneric<Bits>::RootOfUnityByIndex(rootOfUnityIndex) * data[i + j + halfLen];
                    std::complex<float> u = data[i + j];
                    data[i + j] = u + t;
                    data[i + j + halfLen] = u - t;
                }
            }
        }
    }

    void InverseTransform(BasicWaveTableGeneric<Bits>& waveTable, size_t maxComponents)
    {
        // Limit components to available range
        //
        size_t componentsToUse = ((maxComponents + 1) > x_maxComponents) ? x_maxComponents : maxComponents + 1;
        
        // Clear the wave table
        //
        for (size_t i = 0; i < BasicWaveTableGeneric<Bits>::x_tableSize; ++i)
        {
            waveTable.m_table[i] = 0.0f;
        }
        
        // Reconstruct from frequency components using inverse FFT
        //
        std::complex<float> workspace[BasicWaveTableGeneric<Bits>::x_tableSize];
                
        // Zero out unused components for band limiting
        //
        for (size_t k = 0; k < BasicWaveTableGeneric<Bits>::x_tableSize; ++k)
        {
            workspace[k] = std::complex<float>(0.0f, 0.0f);
        }
        
        // Copy complex components directly
        //
        for (size_t k = 0; k < componentsToUse; ++k)
        {
            workspace[k] = m_components[k];

            if (0 < k && k < BasicWaveTableGeneric<Bits>::x_tableSize / 2)
            {
                workspace[BasicWaveTableGeneric<Bits>::x_tableSize - k] = std::conj(workspace[k]);
            }
        }        

        // Perform inverse FFT
        //
        IFFT(workspace, BasicWaveTableGeneric<Bits>::x_tableSize);
        
        // Extract real part and normalize
        //
        for (size_t i = 0; i < BasicWaveTableGeneric<Bits>::x_tableSize; ++i)
        {
            waveTable.m_table[i] = workspace[i].real();
        }
    }

    void IFFT(std::complex<float>* data, size_t N)
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

        // Iterative butterfly operations (IFFT uses positive exponent)
        //
        for (size_t len = 2; len <= N; len <<= 1)
        {
            size_t halfLen = len >> 1;
            for (size_t i = 0; i < N; i += len)
            {
                for (size_t j = 0; j < halfLen; ++j)
                {
                    size_t rootOfUnityIndex = j * BasicWaveTableGeneric<Bits>::x_tableSize / len;
                    std::complex<float> t = MathGeneric<Bits>::RootOfUnityByIndex(rootOfUnityIndex) * data[i + j + halfLen];
                    std::complex<float> u = data[i + j];
                    data[i + j] = u + t;
                    data[i + j + halfLen] = u - t;
                }
            }
        }
    }
};

typedef DiscreteFourierTransformGeneric<10> DiscreteFourierTransform;
typedef DiscreteFourierTransformGeneric<12> DiscreteFourierTransform4096;

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
