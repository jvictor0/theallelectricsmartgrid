#pragma once

#include <cmath>
#include <algorithm>

struct BiquadSection
{
    float m_b0, m_b1, m_b2;
    float m_a1, m_a2;
    float m_x1, m_x2;
    float m_y1, m_y2;

    BiquadSection()
        : m_b0(1.0f)
        , m_b1(0.0f)
        , m_b2(0.0f)
        , m_a1(0.0f)
        , m_a2(0.0f)
        , m_x1(0.0f)
        , m_x2(0.0f)
        , m_y1(0.0f)
        , m_y2(0.0f)
    {
    }

    float Process(float input)
    {
        float output = m_b0 * input + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;

        m_x2 = m_x1;
        m_x1 = input;
        m_y2 = m_y1;
        m_y1 = output;

        return output;
    }

    void Reset()
    {
        m_x1 = m_x2 = m_y1 = m_y2 = 0.0f;
    }

    void SetCoefficients(float cosw, float sinw, float q, bool isHighPass = false)
    {
        float alpha = sinw / (2.0f * q);
        float a0 = 1.0f + alpha;
        float a2 = 1.0f - alpha;
        
        float b0, b1, b2;
        if (isHighPass)
        {
            b0 = (1.0f + cosw) / 2.0f;
            b1 = -(1.0f + cosw);
            b2 = (1.0f + cosw) / 2.0f;
        }
        else
        {
            b0 = (1.0f - cosw) / 2.0f;
            b1 = 1.0f - cosw;
            b2 = (1.0f - cosw) / 2.0f;
        }

        m_b0 = b0 / a0;
        m_b1 = b1 / a0;
        m_b2 = b2 / a0;
        m_a1 = -2.0f * cosw / a0;
        m_a2 = a2 / a0;
    }
};

struct ButterworthFilter
{
    static constexpr float x_minCyclesPerSample = 0.001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    BiquadSection m_biquad1;
    BiquadSection m_biquad2;
    BiquadSection m_biquad3;
    BiquadSection m_biquad4;

    float m_cyclesPerSample;

    ButterworthFilter()
        : m_cyclesPerSample(0.1f)
    {
    }

    float Process(float input)
    {
        // Process through four cascaded biquad sections
        // Each biquad implements a 2nd-order Butterworth section
        //
        float stage1Out = m_biquad1.Process(input);
        float stage2Out = m_biquad2.Process(stage1Out);
        float stage3Out = m_biquad3.Process(stage2Out);
        float stage4Out = m_biquad4.Process(stage3Out);

        return stage4Out;
    }

    void SetCyclesPerSample(float cyclesPerSample)
    {
        m_cyclesPerSample = (cyclesPerSample < x_minCyclesPerSample) ? x_minCyclesPerSample : (cyclesPerSample > x_maxCyclesPerSample) ? x_maxCyclesPerSample : cyclesPerSample;

        // Calculate normalized frequency
        //
        float omega = 2.0f * M_PI * m_cyclesPerSample;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);

        // Butterworth 8th order coefficients
        // Four 2nd-order sections with different pole frequencies
        //
        float q1 = 1.0f / (2.0f * std::cos(M_PI / 16.0f));
        float q2 = 1.0f / (2.0f * std::cos(3.0f * M_PI / 16.0f));
        float q3 = 1.0f / (2.0f * std::cos(5.0f * M_PI / 16.0f));
        float q4 = 1.0f / (2.0f * std::cos(7.0f * M_PI / 16.0f));

        // Set coefficients for all biquad sections
        //
        m_biquad1.SetCoefficients(cosw, sinw, q1, false);
        m_biquad2.SetCoefficients(cosw, sinw, q2, false);
        m_biquad3.SetCoefficients(cosw, sinw, q3, false);
        m_biquad4.SetCoefficients(cosw, sinw, q4, false);
    }

    void Reset()
    {
        m_biquad1.Reset();
        m_biquad2.Reset();
        m_biquad3.Reset();
        m_biquad4.Reset();
    }
}; 