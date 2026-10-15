#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "BasicWaveTable.hpp"

// DualBiasedSaturator: Saturator implementing (tanh(a * x + b) + tanh(a * x - b)) / (tanh(a + b) + tanh(a - b))
// Uses the clipped rational tanh approximation.
//
struct DualBiasedSaturator
{
    static constexpr size_t x_derivativeTableBits = 9;
    static constexpr size_t x_derivativeTableSize = BasicWaveTableGeneric<x_derivativeTableBits>::x_tableSize;

    // The derivative peak leaves the origin at sqrt(9 - 6 * sqrt(2)).
    // At bias 1.5 the two derivative peaks no longer overlap at their centers.
    //
    static constexpr double x_derivativeBiasMin = 0.7174389352143001;
    static constexpr double x_derivativeBiasMax = 1.5;
    static constexpr double x_derivativeBiasWidth = x_derivativeBiasMax - x_derivativeBiasMin;
    static constexpr double x_derivativeBiasStep = x_derivativeBiasWidth / (x_derivativeTableSize - 1);
    static constexpr double x_derivativeLastPhase = static_cast<double>(x_derivativeTableSize - 1) / x_derivativeTableSize;
    static constexpr float x_derivativeRoundingMargin = 12.0f * std::numeric_limits<float>::epsilon();

    float m_gain;
    float m_bias;
    float m_sumGainBias;

    DualBiasedSaturator()
        : DualBiasedSaturator(1.0f, 0.0f)
    {
    }

    DualBiasedSaturator(float gain, float bias)
    {
        SetGainAndBias(gain, bias);
    }

    void SetGainAndBias(float gain, float bias)
    {
        assert(std::isfinite(gain) && gain > 0.0f);
        assert(0.0f <= bias && bias <= 3.0f);
        m_gain = gain;
        m_bias = bias;
        m_sumGainBias = Sum(gain, bias);
        assert(std::isfinite(m_sumGainBias) && m_sumGainBias > 0.0f);
    }

    template<typename T>
    static T TanhDerivative(T input)
    {
        if (std::abs(input) >= static_cast<T>(3.0))
        {
            return static_cast<T>(0.0);
        }

        const T squared = input * input;
        const T ratio = (static_cast<T>(9.0) - squared) / (static_cast<T>(3.0) * (squared + static_cast<T>(3.0)));
        return ratio * ratio;
    }

    static BasicWaveTableGeneric<x_derivativeTableBits> BuildMaxDerivativeTable()
    {
        BasicWaveTableGeneric<x_derivativeTableBits> table;
        for (size_t i = 0; i < x_derivativeTableSize; ++i)
        {
            const double bias = x_derivativeBiasMin + i * x_derivativeBiasStep;
            double peak;
            if (i == 0)
            {
                peak = 2.0 * TanhDerivative(x_derivativeBiasMin);
            }
            else if (i == x_derivativeTableSize - 1)
            {
                peak = 1.0;
            }
            else
            {
                // With c = bias^2 and z = peakPosition^2, the stationary
                // equation is z^4 + quadratic*z^2 + linear*z + constant = 0.
                // It is strictly decreasing on [0, c], with one root there.
                // Solve only while building the shared startup table.
                //
                const double c = bias * bias;
                const double quadratic = -6.0 * (c * c + 18.0 * c + 9.0);
                const double linear = 8.0 * (c - 3.0) * (c * c + 12.0 * c + 9.0);
                const double constant = -3.0 * (c + 3.0) * (c + 3.0) * (c * c - 18.0 * c + 9.0);
                double lower = 0.0;
                double upper = c;
                for (size_t iteration = 0; iteration < 48; ++iteration)
                {
                    const double z = 0.5 * (lower + upper);
                    const double value = ((z * z + quadratic) * z + linear) * z + constant;
                    if (value > 0.0)
                    {
                        lower = z;
                    }
                    else
                    {
                        upper = z;
                    }
                }

                const double position = std::sqrt(0.5 * (lower + upper));
                peak = TanhDerivative(position + bias) + TanhDerivative(position - bias);
            }

            float roundedPeak = static_cast<float>(peak);
            if (static_cast<double>(roundedPeak) < peak)
            {
                roundedPeak = std::nextafter(roundedPeak, std::numeric_limits<float>::infinity());
            }

            table.Write(i, roundedPeak);
        }

        return table;
    }

    inline static const BasicWaveTableGeneric<x_derivativeTableBits> x_maxDerivativeTable = BuildMaxDerivativeTable();

    static float MaxDerivativeSum(float bias)
    {
        assert(0.0f <= bias && bias <= 3.0f);
        if (bias <= static_cast<float>(x_derivativeBiasMin))
        {
            return 2.0f * TanhDerivative(bias);
        }

        if (bias >= static_cast<float>(x_derivativeBiasMax))
        {
            return 1.0f;
        }

        // Map the closed interval onto entries 0 through size - 1, keeping
        // the wavetable's periodic wrap outside the interpolated interval.
        //
        const float phase = (bias - static_cast<float>(x_derivativeBiasMin))
            * static_cast<float>(x_derivativeLastPhase / x_derivativeBiasWidth);
        const float position = phase * x_derivativeTableSize;
        const float fraction = position - std::floor(position);
        const float interpolated = x_maxDerivativeTable.EvaluateLinear(phase);

        // q = TanhDerivative has q'' >= -16/9, so the sum of two shifted
        // copies has curvature >= -32/9. Maximizing over their position
        // preserves the interpolation bound: chord + (16/9)*step^2*f*(1-f).
        // Float lookup and phase rounding are covered by MaxDerivative's margin.
        //
        const float interpolationBound = static_cast<float>((16.0 / 9.0) * x_derivativeBiasStep * x_derivativeBiasStep)
            * fraction * (1.0f - fraction);
        return interpolated + interpolationBound;
    }

    static float Sum(float t, float b)
    {
        assert(t >= 0.0f);
        assert(0.0f <= b && b <= 3.0f);

        if (t <= 3.0f - b)
        {
            const float z = t * t;
            const float b2 = b * b;
            const float d = z - b2;

            // Positive terms avoid cancellation as the bias approaches 3.
            //
            const float gap = (3.0f - b) * (3.0f + b);
            const float denominator = d * d + 6.0f * (z + b2) + 9.0f;
            const float numerator = z * z + (30.0f - 2.0f * b2) * z + gap * gap;

            return 2.0f * t * numerator / (9.0f * denominator);
        }

        if (t < 3.0f + b)
        {
            // Factor 1 + Tanh(u) as (u + 3)^3 / (9 * (u^2 + 3)).
            // Keep the small distance from -3 before subtracting the bias.
            //
            const float u = t - b;
            const float delta = t + (3.0f - b);
            return delta * delta * delta / (9.0f * (u * u + 3.0f));
        }

        return 2.0f;
    }

    float Process(float input) const
    {
        const float numerator = Sum(m_gain * std::abs(input), m_bias);
        return std::copysign(numerator / m_sumGainBias, input);
    }

    // Conservative approximation of max |F'(x)| over all real x. The table
    // depends only on bias, so modulated parameters need no change detection.
    //
    float MaxDerivative() const
    {
        return (m_gain / m_sumGainBias) * MaxDerivativeSum(m_bias)
            * (1.0f + x_derivativeRoundingMargin);
    }
};
