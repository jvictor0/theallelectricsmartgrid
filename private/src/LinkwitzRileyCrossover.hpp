#pragma once

#include <cmath>
#include <algorithm>
#include <complex>
#include "ButterworthFilter.hpp"

struct LinkwitzRileyCrossover
{
    static constexpr float x_minCyclesPerSample = 0.0001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    // Low-pass filter: two biquad sections
    //
    BiquadSection m_lowBiquad1;
    BiquadSection m_lowBiquad2;

    // High-pass filter: two biquad sections
    //
    BiquadSection m_highBiquad1;
    BiquadSection m_highBiquad2;

    float m_cyclesPerSample;

    LinkwitzRileyCrossover()
        : m_cyclesPerSample(0.1f)
    {
    }

    struct CrossoverOutput
    {
        float m_lowPass;
        float m_highPass;
    };

    struct ComplexCrossoverOutput
    {
        std::complex<float> m_lowPass;
        std::complex<float> m_highPass;
    };

    CrossoverOutput Process(float input)
    {
        // Process through low-pass filter (two cascaded biquad sections)
        //
        float lowStage1Out = m_lowBiquad1.Process(input);
        float lowPassOut = m_lowBiquad2.Process(lowStage1Out);

        // Process through high-pass filter (two cascaded biquad sections)
        //
        float highStage1Out = m_highBiquad1.Process(input);
        float highPassOut = m_highBiquad2.Process(highStage1Out);

        return {lowPassOut, highPassOut};
    }

    void SetCyclesPerSample(float cyclesPerSample)
    {
        m_cyclesPerSample = (cyclesPerSample < x_minCyclesPerSample) ? x_minCyclesPerSample : (cyclesPerSample > x_maxCyclesPerSample) ? x_maxCyclesPerSample : cyclesPerSample;

        // Calculate normalized frequency
        //
        float omega = 2.0f * M_PI * m_cyclesPerSample;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);

        // Linkwitz-Riley 4th order crossover coefficients
        // Each filter uses two 2nd-order Butterworth sections
        //
        float q1 = 1.0f / (2.0f * std::cos(M_PI / 8.0f));
        float q2 = 1.0f / (2.0f * std::cos(3.0f * M_PI / 8.0f));

        // Set coefficients for all biquad sections
        //
        m_lowBiquad1.SetCoefficients(cosw, sinw, q1, false);
        m_lowBiquad2.SetCoefficients(cosw, sinw, q2, false);
        m_highBiquad1.SetCoefficients(cosw, sinw, q1, true);
        m_highBiquad2.SetCoefficients(cosw, sinw, q2, true);
    }

    static ComplexCrossoverOutput TransferFunction(float cutoffFreq, float freq)
    {
        cutoffFreq = std::min(x_maxCyclesPerSample, cutoffFreq);

        float omega = 2.0f * M_PI * cutoffFreq;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);
        float q1 = 1.0f / (2.0f * std::cos(M_PI / 8.0f));
        float q2 = 1.0f / (2.0f * std::cos(3.0f * M_PI / 8.0f));

        std::complex<float> lowPassResponse1 = BiquadSection::TransferFunction(cosw, sinw, q1, freq);
        std::complex<float> lowPassResponse2 = BiquadSection::TransferFunction(cosw, sinw, q2, freq);
        std::complex<float> highPassResponse1 = BiquadSection::TransferFunction(cosw, sinw, q1, freq, true);
        std::complex<float> highPassResponse2 = BiquadSection::TransferFunction(cosw, sinw, q2, freq, true);

        return {lowPassResponse1 * lowPassResponse2, highPassResponse1 * highPassResponse2};
    }

    static CrossoverOutput FrequencyResponse(float cutoffFreq, float freq)
    {
        ComplexCrossoverOutput complex = TransferFunction(cutoffFreq, freq);
        return {std::abs(complex.m_lowPass), std::abs(complex.m_highPass)};
    }

    void Reset()
    {
        m_lowBiquad1.Reset();
        m_lowBiquad2.Reset();
        m_highBiquad1.Reset();
        m_highBiquad2.Reset();
    }
};
