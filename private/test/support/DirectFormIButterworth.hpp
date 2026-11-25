#pragma once

#include "MultiChannelNumber.hpp"
#include <cmath>

// Reference for regression tests and the iPad A/B benchmark. This preserves
// the original low-pass arithmetic and state layout.
//
// One second-order section; coefficients are shared by every channel.
//
template<typename Number, size_t Size>
struct DirectFormIBiquad
{
    using Vector = MultiChannelNumber<Number, Size>;

    float m_b0, m_b1, m_b2;
    float m_a1, m_a2;
    Vector m_x1, m_x2;
    Vector m_y1, m_y2;

    DirectFormIBiquad()
        : m_b0(1.0f)
        , m_b1(0.0f)
        , m_b2(0.0f)
        , m_a1(0.0f)
        , m_a2(0.0f)
    {
    }

    Vector Process(const Vector& input)
    {
        typename Vector::Vector x = input.Load();
        typename Vector::Vector x1 = m_x1.Load();
        typename Vector::Vector x2 = m_x2.Load();
        typename Vector::Vector y1 = m_y1.Load();
        typename Vector::Vector y2 = m_y2.Load();
        Vector output = Vector::FromVector(m_b0 * x + m_b1 * x1 + m_b2 * x2 - m_a1 * y1 - m_a2 * y2);

        m_x2 = m_x1;
        m_x1 = input;
        m_y2 = m_y1;
        m_y1 = output;

        return output;
    }

    void Reset()
    {
        m_x1 = Vector();
        m_x2 = Vector();
        m_y1 = Vector();
        m_y2 = Vector();
    }

    void SetCoefficients(float cosw, float sinw, float q)
    {
        float alpha = sinw / (2.0f * q);
        float a0 = 1.0f + alpha;
        float a2 = 1.0f - alpha;
        float b0 = (1.0f - cosw) / 2.0f;
        float b1 = 1.0f - cosw;
        float b2 = (1.0f - cosw) / 2.0f;
        m_b0 = b0 / a0;
        m_b1 = b1 / a0;
        m_b2 = b2 / a0;
        m_a1 = -2.0f * cosw / a0;
        m_a2 = a2 / a0;
    }
};

// Butterworth low-pass of even Order, built from Order / 2 cascaded biquads.
//
template<typename Number, size_t Size, size_t Order>
struct DirectFormIButterworth
{
    static_assert(Order >= 2 && Order % 2 == 0, "Butterworth order must be even");

    using Vector = MultiChannelNumber<Number, Size>;

    static constexpr size_t x_numSections = Order / 2;
    static constexpr float x_minCyclesPerSample = 0.0001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    DirectFormIBiquad<Number, Size> m_sections[x_numSections];

    float m_cyclesPerSample;

    DirectFormIButterworth()
        : m_cyclesPerSample(0.1f)
    {
    }

    Vector Process(const Vector& input)
    {
        Vector output = input;
        for (size_t i = 0; i < x_numSections; ++i)
        {
            output = m_sections[i].Process(output);
        }

        return output;
    }

    // Q of section i: poles at angles (2i + 1) pi / (2 Order).
    //
    static float SectionQ(size_t i)
    {
        return 1.0f / (2.0f * std::cos(static_cast<double>(2 * i + 1) * M_PI / static_cast<double>(2 * Order)));
    }

    void SetCyclesPerSample(float cyclesPerSample)
    {
        m_cyclesPerSample = (cyclesPerSample < x_minCyclesPerSample) ? x_minCyclesPerSample : (cyclesPerSample > x_maxCyclesPerSample) ? x_maxCyclesPerSample : cyclesPerSample;

        // Calculate normalized frequency
        //
        float omega = 2.0f * M_PI * m_cyclesPerSample;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);

        for (size_t i = 0; i < x_numSections; ++i)
        {
            m_sections[i].SetCoefficients(cosw, sinw, SectionQ(i));
        }
    }

    void Reset()
    {
        for (size_t i = 0; i < x_numSections; ++i)
        {
            m_sections[i].Reset();
        }
    }
};
