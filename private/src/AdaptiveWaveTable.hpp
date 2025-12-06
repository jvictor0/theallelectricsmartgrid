#pragma once

#include <complex>
#include <cmath>
#include <cassert>
#include <cstddef>

#include "ios_stubs.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"

struct BasicWaveTable
{
    static constexpr size_t x_tableSize = 1024;
    float m_table[x_tableSize];

    BasicWaveTable()
    {
        Init();
    }

    void Init()
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = 0;
        }
    }

    float Evaluate(float phase) const
    {
        return EvaluateLinear(phase);
    }

    float EvaluateLinear(float phase) const
    {
        float pos = phase * x_tableSize;
        int index = static_cast<int>(std::floor(pos)) % x_tableSize;
        float frac = pos - static_cast<float>(index);
        return m_table[index] * (1.0f - frac) + m_table[index + 1] * frac;
    }

    float EvaluateCubic(float phase) const
    {
        float pos = phase * x_tableSize;
        int index = static_cast<int>(std::floor(pos)) % x_tableSize;
        float frac = pos - static_cast<float>(index);

        // Get four sample points for cubic interpolation
        //
        int i0 = (index - 1 + x_tableSize) % x_tableSize;
        int i1 = index;
        int i2 = (index + 1) % x_tableSize;
        int i3 = (index + 2) % x_tableSize;

        float y0 = m_table[i0];
        float y1 = m_table[i1];
        float y2 = m_table[i2];
        float y3 = m_table[i3];

        // Cubic interpolation (Catmull-Rom)
        //
        float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float a2 = -0.5f * y0 + 0.5f * y2;
        float a3 = y1;

        return ((a0 * frac + a1) * frac + a2) * frac + a3;
    }

    void Write(size_t index, float value)
    {
        assert(index >= 0 && index < x_tableSize);
        m_table[index] = value;
    }

    float Amplitude() const
    {
        float max = 0;
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            max = std::max(max, std::abs(m_table[i]));
        }

        return max;
    }

    float RMSAmplitude() const
    {
        float sum = 0;
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            sum += m_table[i] * m_table[i];
        }

        return std::sqrt(sum / x_tableSize);
    }

    void NormalizeAmplitude()
    {
        float rms = RMSAmplitude();

        if (rms < 0.00000001)
        {
            for (size_t i = 0; i < x_tableSize; ++i)
            {
                m_table[i] = 0;
            }

            return;
        }

        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] /= (rms * sqrt(2));
        }
    }

    void MakeSaw()
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = - 2 * (i / static_cast<float>(x_tableSize) - 0.5f);
        }
    }

    void MakeSquare()
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] = (i / static_cast<float>(x_tableSize) < 0.5f) ? 1.0f : -1.0f;
        }
    }

    void WriteCosHarmonic(size_t harmonic)
    {
        for (size_t i = 0; i < x_tableSize; ++i)
        {
            m_table[i] += cosf(2.0f * M_PI * harmonic * i / x_tableSize);
        }
    }

    float StartValue() const
    {
        return m_table[0];
    }

    float CenterValue() const
    {
        return m_table[x_tableSize / 2];
    }
};

struct DiscreteFourierTransform
{
    static constexpr size_t x_maxComponents = BasicWaveTable::x_tableSize / 2;
    std::complex<float> m_components[x_maxComponents];

    DiscreteFourierTransform()
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

    void Transform(const BasicWaveTable& waveTable)
    {
        // Clear existing components
        //
        for (size_t i = 0; i < x_maxComponents; ++i)
        {
            m_components[i] = std::complex<float>(0.0f, 0.0f);
        }

        // Pre-allocated workspace to avoid dynamic allocation
        //
        std::complex<float> workspace[BasicWaveTable::x_tableSize];

        // Convert input to complex numbers
        //
        const size_t x_N = BasicWaveTable::x_tableSize;
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

        // Create temporary storage for even and odd elements
        //
        std::complex<float> even[N / 2];
        std::complex<float> odd[N / 2];
        
        // Separate even and odd indices
        //
        for (size_t i = 0; i < N / 2; ++i)
        {
            even[i] = data[i * 2];
            odd[i] = data[i * 2 + 1];
        }

        // Recursive calls
        //
        FFT(even, N / 2);
        FFT(odd, N / 2);

        // Combine results
        //
        for (size_t i = 0; i < N / 2; ++i)
        {
            float angle = -2.0f * M_PI * i / N;
            std::complex<float> t = std::complex<float>(std::cos(angle), std::sin(angle)) * odd[i];
            data[i] = even[i] + t;
            data[i + N / 2] = even[i] - t;
        }
    }

    void InverseTransform(BasicWaveTable& waveTable, size_t maxComponents)
    {
        // Limit components to available range
        //
        size_t componentsToUse = ((maxComponents + 1) > x_maxComponents) ? x_maxComponents : maxComponents + 1;
        
        // Clear the wave table
        //
        for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
        {
            waveTable.m_table[i] = 0.0f;
        }
        
        // Reconstruct from frequency components using inverse FFT
        //
        std::complex<float> workspace[BasicWaveTable::x_tableSize];
                
        // Zero out unused components for band limiting
        //
        for (size_t k = 0; k < BasicWaveTable::x_tableSize; ++k)
        {
            workspace[k] = std::complex<float>(0.0f, 0.0f);
        }
        
        // Copy complex components directly
        //
        for (size_t k = 0; k < componentsToUse; ++k)
        {
            workspace[k] = m_components[k];

            if (0 < k && k < BasicWaveTable::x_tableSize / 2)
            {
                workspace[BasicWaveTable::x_tableSize - k] = std::conj(workspace[k]);
            }
        }        

        // Perform inverse FFT
        //
        IFFT(workspace, BasicWaveTable::x_tableSize);
        
        // Extract real part and normalize
        //
        for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
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

        // Create temporary storage for even and odd elements
        //
        std::complex<float> even[N / 2];
        std::complex<float> odd[N / 2];
        
        // Separate even and odd indices
        //
        for (size_t i = 0; i < N / 2; ++i)
        {
            even[i] = data[i * 2];
            odd[i] = data[i * 2 + 1];
        }

        // Recursive calls
        //
        IFFT(even, N / 2);
        IFFT(odd, N / 2);

        // Combine results (inverse FFT uses positive exponent)
        //
        for (size_t i = 0; i < N / 2; ++i)
        {
            float angle = 2.0f * M_PI * i / N;  // Positive for inverse
            std::complex<float> t = std::complex<float>(std::cos(angle), std::sin(angle)) * odd[i];
            data[i] = even[i] + t;
            data[i + N / 2] = even[i] - t;
        }
    }
};

struct AdaptiveWaveTable
{
    static constexpr size_t x_maxLevels = 25;
    static constexpr float m_levelsBase = 1.3f;
    BasicWaveTable m_waveTable;
    DiscreteFourierTransform m_dft;
    BasicWaveTable m_levels[x_maxLevels];
    size_t m_levelComponents[x_maxLevels];

    bool m_waveTableReady;
    bool m_dftReady;
    size_t m_levelsReady;
    
    AdaptiveWaveTable()
    {
        float components = 3;
        m_levelComponents[0] = 1;
        for (size_t i = 1; i < x_maxLevels; ++i)
        {
            m_levelComponents[i] = static_cast<size_t>(components);
            components = std::max(components * m_levelsBase, std::floor(components) + 1);
            assert(m_levelComponents[i] < BasicWaveTable::x_tableSize);            
        }

        m_waveTableReady = false;
        m_dftReady = false;
        m_levelsReady = 0;
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
            m_dft.InverseTransform(m_levels[i], m_levelComponents[i]);
            m_levels[i].NormalizeAmplitude();
        }

        m_levelsReady = x_maxLevels;
    }

    size_t m_lastLevel;

    float Evaluate(float phase, float freq, float maxFreq)
    {
        if (maxFreq < freq)
        {
            return 0;
        }

        float maxComponents = maxFreq / freq;
        auto itr = std::lower_bound(m_levelComponents, m_levelComponents + x_maxLevels, maxComponents);
        size_t level = itr - m_levelComponents;
        if (level != m_lastLevel)
        {
            m_lastLevel = level;            
        }

        if (itr == m_levelComponents + x_maxLevels)
        {
            return m_levels[x_maxLevels - 1].Evaluate(phase);
        }
        else if (itr == m_levelComponents)
        {
            return m_levels[0].Evaluate(phase);
        }
        else
        {
            float lower = m_levels[itr - m_levelComponents - 1].Evaluate(phase);
            float upper = m_levels[itr - m_levelComponents].Evaluate(phase);
            float wayThrough = static_cast<float>(maxComponents - *(itr - 1)) / static_cast<float>(*itr - *(itr - 1));
            assert(wayThrough >= 0);
            assert(wayThrough <= 1);
            return lower * (1 - wayThrough) + upper * wayThrough;
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
