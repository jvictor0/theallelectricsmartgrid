#pragma once

#include "WaveTable.hpp"

struct QuadFloat
{
    float m_values[4];

    QuadFloat(float x, float y, float z, float w)
    {
        m_values[0] = x;
        m_values[1] = y;
        m_values[2] = z;
        m_values[3] = w;
    }

    QuadFloat()
    {
        m_values[0] = 0.0f;
        m_values[1] = 0.0f;
        m_values[2] = 0.0f;
        m_values[3] = 0.0f;
    }

    float& operator[](int index)
    {
        return m_values[index];
    }

    const float& operator[](int index) const
    {
        return m_values[index];
    }

    QuadFloat operator+(const QuadFloat& other) const
    {
        return QuadFloat(m_values[0] + other.m_values[0], m_values[1] + other.m_values[1], m_values[2] + other.m_values[2], m_values[3] + other.m_values[3]);
    }

    QuadFloat operator-(const QuadFloat& other) const
    {
        return QuadFloat(m_values[0] - other.m_values[0], m_values[1] - other.m_values[1], m_values[2] - other.m_values[2], m_values[3] - other.m_values[3]);
    }

    QuadFloat operator*(const QuadFloat& other) const
    {
        return QuadFloat(m_values[0] * other.m_values[0], m_values[1] * other.m_values[1], m_values[2] * other.m_values[2], m_values[3] * other.m_values[3]);
    }

    QuadFloat operator/(const QuadFloat& other) const
    {
        return QuadFloat(m_values[0] / other.m_values[0], m_values[1] / other.m_values[1], m_values[2] / other.m_values[2], m_values[3] / other.m_values[3]);
    }

    QuadFloat operator*(float scalar) const
    {
        return QuadFloat(m_values[0] * scalar, m_values[1] * scalar, m_values[2] * scalar, m_values[3] * scalar);
    }

    QuadFloat operator/(float scalar) const
    {
        return QuadFloat(m_values[0] / scalar, m_values[1] / scalar, m_values[2] / scalar, m_values[3] / scalar);
    }

    QuadFloat operator-() const
    {
        return QuadFloat(-m_values[0], -m_values[1], -m_values[2], -m_values[3]);
    }

    QuadFloat& operator+=(const QuadFloat& other)
    {
        m_values[0] += other.m_values[0];
        m_values[1] += other.m_values[1];
        m_values[2] += other.m_values[2];
        m_values[3] += other.m_values[3];
        return *this;
    }

    QuadFloat& operator-=(const QuadFloat& other)
    {
        m_values[0] -= other.m_values[0];
        m_values[1] -= other.m_values[1];
        m_values[2] -= other.m_values[2];
        m_values[3] -= other.m_values[3];
        return *this;
    }

    QuadFloat& operator*=(const QuadFloat& other)
    {
        m_values[0] *= other.m_values[0];
        m_values[1] *= other.m_values[1];
        m_values[2] *= other.m_values[2];
        m_values[3] *= other.m_values[3];
        return *this;
    }

    QuadFloat& operator/=(const QuadFloat& other)
    {
        m_values[0] /= other.m_values[0];
        m_values[1] /= other.m_values[1];
        m_values[2] /= other.m_values[2];
        m_values[3] /= other.m_values[3];
        return *this;
    }

    QuadFloat& operator*=(float scalar)
    {
        m_values[0] *= scalar;
        m_values[1] *= scalar;
        m_values[2] *= scalar;
        m_values[3] *= scalar;
        return *this;
    }

    QuadFloat& operator/=(float scalar)
    {
        m_values[0] /= scalar;
        m_values[1] /= scalar;
        m_values[2] /= scalar;
        m_values[3] /= scalar;
        return *this;
    }

    QuadFloat ModOne() const
    {
        QuadFloat result;
        for (int i = 0; i < 4; ++i)
        {
            result[i] = m_values[i] - std::floor(m_values[i]);

            if (result[i] < 0 || result[i] >= 1)
            {
                result[i] = result[i] - std::floor(result[i]);
            }
            
            assert(result[i] >= 0);
            assert(result[i] < 1);
        }

        return result;
    }
    
    bool operator==(const QuadFloat& other) const
    {
        return m_values[0] == other.m_values[0] && m_values[1] == other.m_values[1] && m_values[2] == other.m_values[2] && m_values[3] == other.m_values[3];
    }

    bool operator!=(const QuadFloat& other) const
    {
        return m_values[0] != other.m_values[0] || m_values[1] != other.m_values[1] || m_values[2] != other.m_values[2] || m_values[3] != other.m_values[3];
    }

    QuadFloat Rotate(float angle)
    {
        QuadFloat values = *this;

        angle *= 4;
        size_t index = static_cast<size_t>(angle) % 4;
        size_t nextIndex = (index + 1) % 4;
        float lerp = angle - static_cast<float>(index);

        float s = std::sinf(lerp * M_PI / 2);
        float c = std::cosf(lerp * M_PI / 2);

        QuadFloat result;
        for (size_t i = 0; i < 4; i++)
        {
            result[i] = (values[(i + index) % 4] * c + values[(i + nextIndex) % 4] * s) / (c + s);
        }

        return result;
    }

    QuadFloat RotateLinear(QuadFloat angle)
    {
        QuadFloat result;
        for (int i = 0; i < 4; ++i)
        {
            float angleI = angle[i] * 4;
            size_t index = static_cast<size_t>(angleI);
            float lerp = angleI - static_cast<float>(index);

            result[i] = m_values[(i + index) % 4] * (1 - lerp) + m_values[(i + index + 1) % 4] * lerp;
        }

        return result;
    }

    QuadFloat Rotate90()
    {
        return QuadFloat(m_values[3], m_values[0], m_values[1], m_values[2]);
    }

    QuadFloat Hadamard()
    {
        return QuadFloat(
            (m_values[0] + m_values[1] + m_values[2] + m_values[3]) / 2,
            (m_values[0] - m_values[1] + m_values[2] - m_values[3]) / 2,
            (m_values[0] + m_values[1] - m_values[2] - m_values[3]) / 2,
            (m_values[0] - m_values[1] - m_values[2] + m_values[3]) / 2);
    }

    float Sum() const
    {
        return m_values[0] + m_values[1] + m_values[2] + m_values[3];
    }

    float Average() const
    {
        return (m_values[0] + m_values[1] + m_values[2] + m_values[3]) / 4;
    }
    
    QuadFloat Widen(float amount) const
    {
        float sum = Sum() / 4;
        return QuadFloat(m_values[0] + (sum - m_values[0]) * amount, m_values[1] + (sum - m_values[1]) * amount, m_values[2] + (sum - m_values[2]) * amount, m_values[3] + (sum - m_values[3]) * amount);
    }

    static QuadFloat Pan(float x, float y, float sample, const WaveTable* sin)
    {
        return QuadFloat(
            sin->Evaluate((1 - x) * y / 4),
            sin->Evaluate(x * y / 4),
            sin->Evaluate(x * (1 - y) / 4),
            sin->Evaluate((1 - x) * (1 - y) / 4)) * sample;
    }
};
        
