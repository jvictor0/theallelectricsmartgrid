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

    // Batch independent cutoffs across lanes, then share each coefficient set
    // across its audio channels. Three crossovers use a padded four-lane vector.
    //
    template<size_t Count>
    static void SetCyclesPerSampleBatch(LinkwitzRileyCrossoverGeneric* crossovers,
        const MultiChannelNumber<float, Count>& cyclesPerSample)
    {
        using Packed = MultiChannelNumber<float, Count>;
        Packed frequency = Packed::Min(Packed::Splat(x_maxCyclesPerSample),
            Packed::Max(Packed::Splat(x_minCyclesPerSample), cyclesPerSample));

        // Bilinear-transform coefficients using tan(pi*f). Reflect about f=1/4
        // so the rational approximation only covers [0, pi/4]; the reflected
        // half swaps low/high numerators and the sign of a1. This also avoids
        // subtracting a cosine near one at very low cutoffs.
        //
        Packed angle = Packed::Min(frequency, Packed::Splat(0.5f) - frequency) * static_cast<float>(M_PI);
        Packed squared = angle * angle;
        Packed numerator = Packed::FusedMultiplyAdd(squared,
            Packed::FusedMultiplyAdd(squared, Packed::Splat(378) - squared, Packed::Splat(-17325)), Packed::Splat(135135));
        Packed denominator = Packed::FusedMultiplyAdd(squared,
            Packed::FusedMultiplyAdd(squared,
                Packed::FusedMultiplyAdd(squared, Packed::Splat(-28), Packed::Splat(3150)), Packed::Splat(-62370)), Packed::Splat(135135));
        Packed tangent = angle * numerator / denominator;
        Packed tangentSquared = tangent * tangent;
        Packed sum = Packed::Splat(1) + tangentSquared;

        for (size_t section = 0; section < 2; ++section)
        {
            float q = section == 0 ? Q1() : Q2();
            Packed damping = tangent / q;
            Packed reciprocal = Packed::Splat(1) / (sum + damping);
            Packed small = tangentSquared * reciprocal;
            Packed lowB0 = Packed::IfLess(frequency, Packed::Splat(0.25f), small, reciprocal);
            Packed highB0 = Packed::IfLess(frequency, Packed::Splat(0.25f), reciprocal, small);
            Packed negativeA1 = (tangentSquared - 1) * 2 * reciprocal;
            Packed a1 = Packed::IfLess(frequency, Packed::Splat(0.25f), negativeA1, -negativeA1);
            Packed a2 = (sum - damping) * reciprocal;
            for (size_t lane = 0; lane < Count; ++lane)
            {
                auto& crossover = crossovers[lane];
                crossover.m_cyclesPerSample = frequency[lane];
                auto& low = section == 0 ? crossover.m_lowBiquad1 : crossover.m_lowBiquad2;
                auto& high = section == 0 ? crossover.m_highBiquad1 : crossover.m_highBiquad2;
                low.m_b0 = low.m_b2 = lowB0[lane];
                low.m_b1 = 2.0f * lowB0[lane];
                high.m_b0 = high.m_b2 = highB0[lane];
                high.m_b1 = -2.0f * highB0[lane];
                low.m_a1 = high.m_a1 = a1[lane];
                low.m_a2 = high.m_a2 = a2[lane];
            }
        }
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
