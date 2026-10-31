#pragma once

#include "MultiChannelNumber.hpp"
#include <algorithm>
#include <cmath>

template <typename Number, size_t Size, bool Normalize>
struct TanhSaturatorGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    Number m_inputGain;
    Number m_tanhGain;

    TanhSaturatorGeneric()
        : m_inputGain(1)
        , m_tanhGain(static_cast<Number>(0.7615941559557649))
    {
    }

    TanhSaturatorGeneric(Number gain)
        : m_inputGain(1)
        , m_tanhGain(static_cast<Number>(0.7615941559557649))
    {
        SetInputGain(gain);
    }

    void SetInputGain(Number gain)
    {
        if (gain != m_inputGain)
        {
            m_inputGain = gain;
            m_tanhGain = Tanh(m_inputGain);
        }
    }

    static Number Tanh(Number x)
    {
        Number y = x * (27 + x * x) / (27 + 9 * x * x);
        return std::min<Number>(1, std::max<Number>(-1, y));
    }

    static Vector Tanh(const Vector& x)
    {
        Vector x2 = x * x;
        Vector y = x * (x2 + 27) / (x * 9 * x + 27);
        return Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), y));
    }

    // Derivative of Process w.r.t. input
    // Non-normalized: d/dx[Tanh(gain*x)] = gain * sech²(gain*x)
    // Normalized: d/dx[Tanh(gain*x)/tanhGain] = gain * sech²(gain*x) / tanhGain
    //
    Vector Derivative(const Vector& input)
    {
        // Rational tanh and its derivative sech^2 as a rational function
        // y = x * (27 + x^2) / (27 + 9*x^2)
        // dy/dx = [ (27 + x^2) + 2x^2 ] / (27 + 9*x^2) - x*(27 + x^2)*18x / (27 + 9*x^2)^2
        //        = (27 + 3x^2) / (27 + 9x^2) - [18x^2 (27 + x^2)] / (27 + 9x^2)^2
        //
        Vector x = input * m_inputGain;
        Vector x2 = x * x;
        Vector denom = x2 * 9 + 27;

        // Derivative of the numerator
        //
        Vector num1 = x2 * 3 + 27;
        Vector num2 = x2 * 18 * (x2 + 27);

        Vector numerator = num1 * denom - num2;
        Vector denominator = denom * denom;

        Vector sech2 = numerator / denominator;
        Vector result = Normalize ? sech2 * m_inputGain / m_tanhGain : sech2 * m_inputGain;

        // The clamped region of Tanh is flat.
        //
        Vector saturated = Tanh(x);
        for (size_t i = 0; i < Size; ++i)
        {
            if (1 <= std::fabs(saturated[i]))
            {
                result[i] = 0;
            }
        }

        return result;
    }

    Number DerivativeZero()
    {
        if (Normalize)
        {
            return m_inputGain / m_tanhGain;
        }
        else
        {
            return m_inputGain;
        }
    }

    Vector Process(const Vector& input)
    {
        Vector output = Tanh(input * m_inputGain);
        return Normalize ? output / m_tanhGain : output;
    }
};

template <bool Normalize>
using TanhSaturator = TanhSaturatorGeneric<float, 1, Normalize>;

template <bool Normalize>
using QuadTanhSaturator = TanhSaturatorGeneric<float, 4, Normalize>;

template <typename Number, size_t Size, bool Normalize>
struct ATanSaturatorGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    Number m_inputGain;
    Number m_atanGain;

    ATanSaturatorGeneric()
        : m_inputGain(1)
        , m_atanGain(0)
    {
    }

    ATanSaturatorGeneric(Number gain)
        : m_inputGain(1)
        , m_atanGain(0)
    {
        SetInputGain(gain);
    }

    static Number Saturate(Number input)
    {
        return static_cast<Number>(std::atan(input * M_PI / 2) / (M_PI / 2));
    }

    static Vector Saturate(const Vector& input)
    {
        Vector output;
        for (size_t i = 0; i < Size; ++i)
        {
            output[i] = Saturate(input[i]);
        }

        return output;
    }

    void SetInputGain(Number gain)
    {
        if (gain != m_inputGain)
        {
            m_inputGain = gain;
            m_atanGain = Saturate(m_inputGain);
        }
    }

    Vector Process(const Vector& input)
    {
        Vector output = Saturate(input * m_inputGain);
        return Normalize ? output / m_atanGain : output;
    }
};

template <bool Normalize>
using ATanSaturator = ATanSaturatorGeneric<float, 1, Normalize>;

template <bool Normalize>
using QuadATanSaturator = ATanSaturatorGeneric<float, 4, Normalize>;
