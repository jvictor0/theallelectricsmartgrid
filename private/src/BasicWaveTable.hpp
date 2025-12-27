#pragma once

#include <cassert>
#include <cstddef>
#include <cmath>

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

#pragma clang diagnostic pop

