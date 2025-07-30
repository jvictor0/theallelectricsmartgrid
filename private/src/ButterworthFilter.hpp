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

        // First biquad section
        //
        float alpha1 = sinw / (2.0f * q1);
        float a0_1 = 1.0f + alpha1;
        float a2_1 = 1.0f - alpha1;
        float b0_1 = (1.0f - cosw) / 2.0f;
        float b1_1 = 1.0f - cosw;
        float b2_1 = (1.0f - cosw) / 2.0f;

        m_biquad1.m_b0 = b0_1 / a0_1;
        m_biquad1.m_b1 = b1_1 / a0_1;
        m_biquad1.m_b2 = b2_1 / a0_1;
        m_biquad1.m_a1 = -2.0f * cosw / a0_1;
        m_biquad1.m_a2 = a2_1 / a0_1;

        // Second biquad section
        //
        float alpha2 = sinw / (2.0f * q2);
        float a0_2 = 1.0f + alpha2;
        float a2_2 = 1.0f - alpha2;
        float b0_2 = (1.0f - cosw) / 2.0f;
        float b1_2 = 1.0f - cosw;
        float b2_2 = (1.0f - cosw) / 2.0f;

        m_biquad2.m_b0 = b0_2 / a0_2;
        m_biquad2.m_b1 = b1_2 / a0_2;
        m_biquad2.m_b2 = b2_2 / a0_2;
        m_biquad2.m_a1 = -2.0f * cosw / a0_2;
        m_biquad2.m_a2 = a2_2 / a0_2;

        // Third biquad section
        //
        float alpha3 = sinw / (2.0f * q3);
        float a0_3 = 1.0f + alpha3;
        float a2_3 = 1.0f - alpha3;
        float b0_3 = (1.0f - cosw) / 2.0f;
        float b1_3 = 1.0f - cosw;
        float b2_3 = (1.0f - cosw) / 2.0f;

        m_biquad3.m_b0 = b0_3 / a0_3;
        m_biquad3.m_b1 = b1_3 / a0_3;
        m_biquad3.m_b2 = b2_3 / a0_3;
        m_biquad3.m_a1 = -2.0f * cosw / a0_3;
        m_biquad3.m_a2 = a2_3 / a0_3;

        // Fourth biquad section
        //
        float alpha4 = sinw / (2.0f * q4);
        float a0_4 = 1.0f + alpha4;
        float a2_4 = 1.0f - alpha4;
        float b0_4 = (1.0f - cosw) / 2.0f;
        float b1_4 = 1.0f - cosw;
        float b2_4 = (1.0f - cosw) / 2.0f;

        m_biquad4.m_b0 = b0_4 / a0_4;
        m_biquad4.m_b1 = b1_4 / a0_4;
        m_biquad4.m_b2 = b2_4 / a0_4;
        m_biquad4.m_a1 = -2.0f * cosw / a0_4;
        m_biquad4.m_a2 = a2_4 / a0_4;
    }

    void Reset()
    {
        m_biquad1.Reset();
        m_biquad2.Reset();
        m_biquad3.Reset();
        m_biquad4.Reset();
    }
}; 