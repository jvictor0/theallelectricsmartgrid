#pragma once

#include <cmath>
#include <algorithm>
#include <complex>
#include "ButterworthFilter.hpp"
#include "MultiChannelNumber.hpp"

// Width-independent parts of the 4th order Linkwitz-Riley crossover: cutoff
// limits, section Qs and the scalar transfer functions the UI evaluates.
//
struct LinkwitzRileyCrossoverBase
{
    static constexpr float x_minCyclesPerSample = 0.0001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    struct ComplexCrossoverOutput
    {
        std::complex<float> m_lowPass;
        std::complex<float> m_highPass;
    };

    // Each filter is two 2nd-order Butterworth sections.
    //
    static float Q1()
    {
        return 1.0f / (2.0f * std::cos(M_PI / 8.0f));
    }

    static float Q2()
    {
        return 1.0f / (2.0f * std::cos(3.0f * M_PI / 8.0f));
    }

    static ComplexCrossoverOutput TransferFunction(float cutoffFreq, float freq)
    {
        cutoffFreq = std::min(x_maxCyclesPerSample, cutoffFreq);

        float omega = 2.0f * M_PI * cutoffFreq;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);
        float q1 = Q1();
        float q2 = Q2();

        std::complex<float> lowPassResponse1 = BiquadSection::TransferFunction(cosw, sinw, q1, freq, false);
        std::complex<float> lowPassResponse2 = BiquadSection::TransferFunction(cosw, sinw, q2, freq, false);
        std::complex<float> highPassResponse1 = BiquadSection::TransferFunction(cosw, sinw, q1, freq, true);
        std::complex<float> highPassResponse2 = BiquadSection::TransferFunction(cosw, sinw, q2, freq, true);

        return {lowPassResponse1 * lowPassResponse2, highPassResponse1 * highPassResponse2};
    }

    struct ScalarCrossoverOutput
    {
        float m_lowPass;
        float m_highPass;
    };

    static ScalarCrossoverOutput FrequencyResponse(float cutoffFreq, float freq)
    {
        ComplexCrossoverOutput complex = TransferFunction(cutoffFreq, freq);
        return {std::abs(complex.m_lowPass), std::abs(complex.m_highPass)};
    }
};

// One cutoff shared by every channel; coefficients are computed once.
//
template<typename Number, size_t Size>
struct LinkwitzRileyCrossoverGeneric : LinkwitzRileyCrossoverBase
{
    using Vector = MultiChannelNumber<Number, Size>;
    using Section = BiquadSectionGeneric<Number, Size>;

    // Low-pass filter: two biquad sections
    //
    Section m_lowBiquad1;
    Section m_lowBiquad2;

    // High-pass filter: two biquad sections
    //
    Section m_highBiquad1;
    Section m_highBiquad2;

    float m_cyclesPerSample;

    LinkwitzRileyCrossoverGeneric()
        : m_cyclesPerSample(0.1f)
    {
    }

    struct CrossoverOutput
    {
        Vector m_lowPass;
        Vector m_highPass;
    };

    CrossoverOutput Process(const Vector& input)
    {
        Vector lowPassOut = m_lowBiquad2.Process(m_lowBiquad1.Process(input));
        Vector highPassOut = m_highBiquad2.Process(m_highBiquad1.Process(input));
        return {lowPassOut, highPassOut};
    }

    void SetCyclesPerSample(float cyclesPerSample)
    {
        m_cyclesPerSample = (cyclesPerSample < x_minCyclesPerSample) ? x_minCyclesPerSample : (cyclesPerSample > x_maxCyclesPerSample) ? x_maxCyclesPerSample : cyclesPerSample;

        float omega = 2.0f * M_PI * m_cyclesPerSample;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);
        float q1 = Q1();
        float q2 = Q2();

        m_lowBiquad1.SetCoefficients(cosw, sinw, q1, false);
        m_lowBiquad2.SetCoefficients(cosw, sinw, q2, false);
        m_highBiquad1.SetCoefficients(cosw, sinw, q1, true);
        m_highBiquad2.SetCoefficients(cosw, sinw, q2, true);
    }

    void Reset()
    {
        m_lowBiquad1.Reset();
        m_lowBiquad2.Reset();
        m_highBiquad1.Reset();
        m_highBiquad2.Reset();
    }
};

using LinkwitzRileyCrossover = LinkwitzRileyCrossoverGeneric<float, 1>;
