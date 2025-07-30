#pragma once

#include <cmath>

struct BitRateReducer
{
    float m_amount;
    float m_steps;

    BitRateReducer()
        : m_amount(0.0f)
    {
    }

    void SetAmount(float amount)
    {
        if (amount != m_amount)
        {
            static constexpr float maxBits = 16;

            float ebits = (1 + maxBits) * std::powf(1.0 - amount, 3.5) - 1;
            m_steps = std::powf(2.0, -ebits);
            m_amount = amount;
        }
    }

    float Process(float input)
    {
        if (m_amount == 0.0f)
        {
            return input;
        }

        return std::roundf((input + 1) / m_steps) * m_steps - 1;
    }
};

struct SampleRateReducer
{
    float m_freq;
    float m_phase;
    float m_output;

    SampleRateReducer()
        : m_freq(0.0f)
        , m_phase(0.0f)
    {
    }

    void SetFreq(float freq)
    {
        m_freq = freq;
    }

    float Process(float input)
    {
        if (m_freq >= 1.0)
        {
            return input;
        }

        m_phase += m_freq;
        if (m_phase >= 1.0)
        {
            m_phase = m_phase - std::floor(m_phase);
            m_output = input;
        }

        return m_output;
    }
};