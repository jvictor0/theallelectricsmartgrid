#pragma once

#include "MultiChannelNumber.hpp"
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
struct SampleRateReducerGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    Vector m_freq;
    Vector m_phase;
    Vector m_output;

    SampleRateReducerGeneric()
        : m_freq(Vector::Splat(0))
        , m_phase(Vector::Splat(0))
        , m_output(Vector::Splat(0))
    {
    }

    void SetFreq(const Vector& freq)
    {
        m_freq = freq;
    }

    // Channels at or above one cycle per sample bypass and hold their phase.
    //
    Vector Process(const Vector& input)
    {
        Vector one = Vector::Splat(1);
        m_phase += Vector::IfLess(m_freq, one, m_freq, Vector::Splat(0));
        Vector wrapped = m_phase - m_phase.Floor();
        m_output = Vector::IfLess(m_phase, one, m_output, input);
        m_phase = Vector::IfLess(m_phase, one, m_phase, wrapped);
        return Vector::IfLess(m_freq, one, m_output, input);
    }
};

using SampleRateReducer = SampleRateReducerGeneric<1>;
using TrioSampleRateReducer = SampleRateReducerGeneric<3>;
