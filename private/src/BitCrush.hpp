#pragma once

#include "MultiChannelNumber.hpp"
#include "SampleRateReducerCoreGeneric.hpp"
#include <cmath>

template <size_t Size>
struct BitRateReducerGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    Vector m_amount;
    Vector m_steps;

    BitRateReducerGeneric()
        : m_amount(Vector::Splat(0))
        , m_steps(Vector::Splat(0))
    {
    }

    // Effective bits fall from 16 to -1 as (1 - amount)^3.5; the step size is
    // 2^-bits.
    //
    void SetAmount(const Vector& amount)
    {
        if (amount != m_amount)
        {
            static constexpr float x_maxBits = 16;

            Vector remaining = Vector::Splat(1) - amount;
            Vector curve = remaining * remaining * remaining * remaining.Sqrt();
            Vector ebits = curve * (1 + x_maxBits) - 1;
            m_steps = (-ebits).Exp2();
            m_amount = amount;
        }
    }

    // A zero amount passes the input through unchanged.
    //
    Vector Process(const Vector& input)
    {
        Vector crushed = ((input + 1) / m_steps).Round() * m_steps - 1;
        return Vector::IfLess(Vector::Splat(0), m_amount, crushed, input);
    }
};

using BitRateReducer = BitRateReducerGeneric<1>;
using TrioBitRateReducer = BitRateReducerGeneric<3>;

template <size_t Size>
struct SampleRateReducerGeneric : SampleRateReducerCoreGeneric<float, Size>::State
{
    using Core = SampleRateReducerCoreGeneric<float, Size>;
    using Vector = typename Core::Vector;

    Vector m_freq;

    void SetFreq(const Vector& freq)
    {
        m_freq = freq;
    }

    Vector Process(const Vector& input)
    {
        return Core::Process(*this, typename Core::Input{m_freq, input});
    }
};

using SampleRateReducer = SampleRateReducerGeneric<1>;
using TrioSampleRateReducer = SampleRateReducerGeneric<3>;
