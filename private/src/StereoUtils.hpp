#pragma once

#include "Math.hpp"
#include "QuadUtils.hpp"

struct StereoFloat
{
    static constexpr size_t x_numChannels = 2;
    float m_values[x_numChannels];

    StereoFloat(float left, float right)
        : m_values{left, right}
    {
    }

    StereoFloat(const float* values)
        : m_values{}
    {
        m_values[0] = values[0];
        m_values[1] = values[1];
    }

    StereoFloat()
        : m_values{0.0f, 0.0f}
    {
    }

    float& operator[](int index)
    {
        return m_values[index];
    }

    const float& operator[](int index) const
    {
        return m_values[index];
    }

    StereoFloat operator+(const StereoFloat& other) const
    {
        return StereoFloat(m_values[0] + other.m_values[0], m_values[1] + other.m_values[1]);
    }

    StereoFloat operator-(const StereoFloat& other) const
    {
        return StereoFloat(m_values[0] - other.m_values[0], m_values[1] - other.m_values[1]);
    }

    StereoFloat operator*(const StereoFloat& other) const
    {
        return StereoFloat(m_values[0] * other.m_values[0], m_values[1] * other.m_values[1]);
    }

    StereoFloat operator/(const StereoFloat& other) const
    {
        return StereoFloat(m_values[0] / other.m_values[0], m_values[1] / other.m_values[1]);
    }

    StereoFloat operator*(float scalar) const
    {
        return StereoFloat(m_values[0] * scalar, m_values[1] * scalar);
    }

    StereoFloat operator/(float scalar) const
    {
        return StereoFloat(m_values[0] / scalar, m_values[1] / scalar);
    }

    StereoFloat operator-() const
    {
        return StereoFloat(-m_values[0], -m_values[1]);
    }

    StereoFloat& operator+=(const StereoFloat& other)
    {
        m_values[0] += other.m_values[0];
        m_values[1] += other.m_values[1];
        return *this;
    }

    StereoFloat& operator-=(const StereoFloat& other)
    {
        m_values[0] -= other.m_values[0];
        m_values[1] -= other.m_values[1];
        return *this;
    }

    StereoFloat& operator*=(const StereoFloat& other)
    {
        m_values[0] *= other.m_values[0];
        m_values[1] *= other.m_values[1];
        return *this;
    }

    StereoFloat& operator/=(const StereoFloat& other)
    {
        m_values[0] /= other.m_values[0];
        m_values[1] /= other.m_values[1];
        return *this;
    }

    StereoFloat& operator*=(float scalar)
    {
        m_values[0] *= scalar;
        m_values[1] *= scalar;
        return *this;
    }

    StereoFloat& operator/=(float scalar)
    {
        m_values[0] /= scalar;
        m_values[1] /= scalar;
        return *this;
    }

    StereoFloat ModOne() const
    {
        StereoFloat result;
        for (int i = 0; i < 2; ++i)
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
    
    bool operator==(const StereoFloat& other) const
    {
        return m_values[0] == other.m_values[0] && m_values[1] == other.m_values[1];
    }

    bool operator!=(const StereoFloat& other) const
    {
        return m_values[0] != other.m_values[0] || m_values[1] != other.m_values[1];
    }

    StereoFloat& operator=(const StereoFloat& other)
    {
        m_values[0] = other.m_values[0];
        m_values[1] = other.m_values[1];
        return *this;
    }

    float Sum() const
    {
        return m_values[0] + m_values[1];
    }

    float Average() const
    {
        return (m_values[0] + m_values[1]) / 2;
    }

    static StereoFloat Pan(float x, float sample)
    {
        return StereoFloat(
            Math::Sin2pi((1 - x) / 4),
            Math::Sin2pi(x / 4)) * sample;
    }

    QuadFloat EmbedQuadEmpty() const
    {
        return QuadFloat(m_values[0], m_values[1], 0.0f, 0.0f);
    }

    QuadFloat CombineToQuad(const StereoFloat& other) const
    {
        return QuadFloat(m_values[0], m_values[1], other.m_values[0], other.m_values[1]);
    }
};

template <size_t Size>
struct MultiChannelFloatTypeHolder;

template <>
struct MultiChannelFloatTypeHolder<2> 
{
    using type = StereoFloat;
};

template <>
struct MultiChannelFloatTypeHolder<4> 
{
    using type = QuadFloat;
};

template <size_t Size>
using MultiChannelFloat = typename MultiChannelFloatTypeHolder<Size>::type;
