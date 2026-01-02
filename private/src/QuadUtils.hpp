#pragma once

#include "Math.hpp"

template<class Number>
struct QuadNumber
{
    static constexpr size_t x_numChannels = 4;
    Number m_values[x_numChannels];

    QuadNumber(Number x, Number y, Number z, Number w)
    {
        m_values[0] = x;
        m_values[1] = y;
        m_values[2] = z;
        m_values[3] = w;
    }

    QuadNumber(const Number* values)
    {
        m_values[0] = values[0];
        m_values[1] = values[1];
        m_values[2] = values[2];
        m_values[3] = values[3];
    }

    QuadNumber()
    {
        m_values[0] = static_cast<Number>(0);
        m_values[1] = static_cast<Number>(0);
        m_values[2] = static_cast<Number>(0);
        m_values[3] = static_cast<Number>(0);
    }

    Number& operator[](int index)
    {
        return m_values[index];
    }

    const Number& operator[](int index) const
    {
        return m_values[index];
    }

    QuadNumber operator+(const QuadNumber& other) const
    {
        return QuadNumber(m_values[0] + other.m_values[0], m_values[1] + other.m_values[1], m_values[2] + other.m_values[2], m_values[3] + other.m_values[3]);
    }

    QuadNumber operator-(const QuadNumber& other) const
    {
        return QuadNumber(m_values[0] - other.m_values[0], m_values[1] - other.m_values[1], m_values[2] - other.m_values[2], m_values[3] - other.m_values[3]);
    }

    QuadNumber operator*(const QuadNumber& other) const
    {
        return QuadNumber(m_values[0] * other.m_values[0], m_values[1] * other.m_values[1], m_values[2] * other.m_values[2], m_values[3] * other.m_values[3]);
    }

    QuadNumber operator/(const QuadNumber& other) const
    {
        return QuadNumber(m_values[0] / other.m_values[0], m_values[1] / other.m_values[1], m_values[2] / other.m_values[2], m_values[3] / other.m_values[3]);
    }

    QuadNumber operator*(Number scalar) const
    {
        return QuadNumber(m_values[0] * scalar, m_values[1] * scalar, m_values[2] * scalar, m_values[3] * scalar);
    }

    QuadNumber operator/(Number scalar) const
    {
        return QuadNumber(m_values[0] / scalar, m_values[1] / scalar, m_values[2] / scalar, m_values[3] / scalar);
    }

    QuadNumber operator-() const
    {
        return QuadNumber(-m_values[0], -m_values[1], -m_values[2], -m_values[3]);
    }

    QuadNumber& operator+=(const QuadNumber& other)
    {
        m_values[0] += other.m_values[0];
        m_values[1] += other.m_values[1];
        m_values[2] += other.m_values[2];
        m_values[3] += other.m_values[3];
        return *this;
    }

    QuadNumber& operator-=(const QuadNumber& other)
    {
        m_values[0] -= other.m_values[0];
        m_values[1] -= other.m_values[1];
        m_values[2] -= other.m_values[2];
        m_values[3] -= other.m_values[3];
        return *this;
    }

    QuadNumber& operator*=(const QuadNumber& other)
    {
        m_values[0] *= other.m_values[0];
        m_values[1] *= other.m_values[1];
        m_values[2] *= other.m_values[2];
        m_values[3] *= other.m_values[3];
        return *this;
    }

    QuadNumber& operator/=(const QuadNumber& other)
    {
        m_values[0] /= other.m_values[0];
        m_values[1] /= other.m_values[1];
        m_values[2] /= other.m_values[2];
        m_values[3] /= other.m_values[3];
        return *this;
    }

    QuadNumber& operator*=(Number scalar)
    {
        m_values[0] *= scalar;
        m_values[1] *= scalar;
        m_values[2] *= scalar;
        m_values[3] *= scalar;
        return *this;
    }

    QuadNumber& operator/=(Number scalar)
    {
        m_values[0] /= scalar;
        m_values[1] /= scalar;
        m_values[2] /= scalar;
        m_values[3] /= scalar;
        return *this;
    }

    QuadNumber ModOne() const
    {
        QuadNumber result;
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
    
    bool operator==(const QuadNumber& other) const
    {
        return m_values[0] == other.m_values[0] && m_values[1] == other.m_values[1] && m_values[2] == other.m_values[2] && m_values[3] == other.m_values[3];
    }

    bool operator!=(const QuadNumber& other) const
    {
        return m_values[0] != other.m_values[0] || m_values[1] != other.m_values[1] || m_values[2] != other.m_values[2] || m_values[3] != other.m_values[3];
    }

    QuadNumber Rotate(Number angle)
    {
        QuadNumber values = *this;

        angle *= 4;
        size_t index = static_cast<size_t>(angle) % 4;
        size_t nextIndex = (index + 1) % 4;
        Number lerp = angle - static_cast<Number>(index);

        Number s = std::sin(lerp * M_PI / 2);
        Number c = std::cos(lerp * M_PI / 2);

        QuadNumber result;
        for (size_t i = 0; i < 4; i++)
        {
            result[i] = (values[(i + index) % 4] * c + values[(i + nextIndex) % 4] * s) / (c + s);
        }

        return result;
    }

    QuadNumber RotateLinear(QuadNumber angle)
    {
        QuadNumber result;
        for (int i = 0; i < 4; ++i)
        {
            Number angleI = angle[i] * 4;
            size_t index = static_cast<size_t>(angleI);
            Number lerp = angleI - static_cast<Number>(index);

            result[i] = m_values[(i + index) % 4] * (1 - lerp) + m_values[(i + index + 1) % 4] * lerp;
        }

        return result;
    }

    QuadNumber Rotate90()
    {
        return QuadNumber(m_values[3], m_values[0], m_values[1], m_values[2]);
    }

    QuadNumber Hadamard()
    {
        return QuadNumber(
            (m_values[0] + m_values[1] + m_values[2] + m_values[3]) / 2,
            (m_values[0] - m_values[1] + m_values[2] - m_values[3]) / 2,
            (m_values[0] + m_values[1] - m_values[2] - m_values[3]) / 2,
            (m_values[0] - m_values[1] - m_values[2] + m_values[3]) / 2);
    }

    Number Sum() const
    {
        return m_values[0] + m_values[1] + m_values[2] + m_values[3];
    }

    Number Average() const
    {
        return (m_values[0] + m_values[1] + m_values[2] + m_values[3]) / 4;
    }
    
    QuadNumber Widen(Number amount) const
    {
        Number sum = Sum() / 4;
        return QuadNumber(m_values[0] + (sum - m_values[0]) * amount, m_values[1] + (sum - m_values[1]) * amount, m_values[2] + (sum - m_values[2]) * amount, m_values[3] + (sum - m_values[3]) * amount);
    }

    static QuadNumber Pan(Number x, Number y, Number sample)
    {
        return QuadNumber(
            Math::Sin2pi((1 - x) * y / 4),
            Math::Sin2pi(x * y / 4),
            Math::Sin2pi(x * (1 - y) / 4),
            Math::Sin2pi((1 - x) * (1 - y) / 4)) * sample;
    }
};

using QuadFloat = QuadNumber<float>;
using QuadDouble = QuadNumber<double>;
        
