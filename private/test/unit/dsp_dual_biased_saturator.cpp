#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "DualBiasedSaturator.hpp"

namespace
{

double ReferenceTanh(double input)
{
    if (std::abs(input) >= 3.0)
    {
        return std::copysign(1.0, input);
    }

    return input * (27.0 + input * input) / (27.0 + 9.0 * input * input);
}

double ReferenceSum(float gain, float bias)
{
    return ReferenceTanh(static_cast<double>(gain) + bias)
        + ReferenceTanh(static_cast<double>(gain) - bias);
}

double ReferenceProcess(float gain, float bias, float input)
{
    const double driven = static_cast<double>(gain) * input;
    const double numerator = ReferenceTanh(driven + bias) + ReferenceTanh(driven - bias);
    const double denominator = ReferenceTanh(static_cast<double>(gain) + bias)
                             + ReferenceTanh(static_cast<double>(gain) - bias);
    return numerator / denominator;
}

double ReferenceTanhDerivative(double input)
{
    if (std::abs(input) >= 3.0)
    {
        return 0.0;
    }

    const double squared = input * input;
    const double denominator = 27.0 + 9.0 * squared;
    return ((27.0 + 3.0 * squared) * denominator - 18.0 * squared * (27.0 + squared))
        / (denominator * denominator);
}

double ReferenceMaxDerivativeSum(double bias)
{
    const auto sum = [bias](double position)
    {
        return ReferenceTanhDerivative(position + bias) + ReferenceTanhDerivative(position - bias);
    };

    // Independently maximize the derivative by searching position directly,
    // without the production table, quartic, or bias transition constants.
    // The positive peak lies between zero and bias.
    //
    double lower = 0.0;
    double upper = bias;
    for (int iteration = 0; iteration < 80; ++iteration)
    {
        const double left = lower + (upper - lower) / 3.0;
        const double right = upper - (upper - lower) / 3.0;
        if (sum(left) < sum(right))
        {
            lower = left;
        }
        else
        {
            upper = right;
        }
    }

    return std::max({sum(0.0), sum(bias), sum(0.5 * (lower + upper))});
}

}

DOCTEST_TEST_CASE("DualBiasedSaturator: normalized odd monotonic curve in the ladder gain range")
{
    for (float gain : {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 5.0f})
    {
        for (float bias : {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, std::nextafter(3.0f, 0.0f), 3.0f})
        {
            DOCTEST_INFO("gain=" << gain << " bias=" << bias);
            const DualBiasedSaturator saturator(gain, bias);
            DOCTEST_CHECK(saturator.Process(0.0f) == 0.0f);
            DOCTEST_CHECK(saturator.Process(1.0f) == 1.0f);
            DOCTEST_CHECK(saturator.Process(-1.0f) == -1.0f);

            float previous = 0.0f;
            for (int index = 0; index <= 128; ++index)
            {
                const float input = static_cast<float>(index) / 128.0f;
                const float output = saturator.Process(input);
                DOCTEST_CHECK(output >= previous);
                DOCTEST_CHECK(output <= 1.0f);
                DOCTEST_CHECK(saturator.Process(-input) == -output);
                previous = output;
            }
        }
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: float curve matches double reference throughout the ladder range")
{
    for (int gainIndex = 0; gainIndex <= 64; ++gainIndex)
    {
        const float gain = 0.25f + 4.75f * gainIndex / 64.0f;
        for (int biasIndex = 0; biasIndex <= 128; ++biasIndex)
        {
            const float bias = 3.0f * biasIndex / 128.0f;
            const DualBiasedSaturator saturator(gain, bias);
            for (int inputIndex = -64; inputIndex <= 64; ++inputIndex)
            {
                const float input = static_cast<float>(inputIndex) / 8.0f;
                const double expected = ReferenceProcess(gain, bias, input);
                DOCTEST_INFO("gain=" << gain << " bias=" << bias << " input=" << input);
                DOCTEST_CHECK(std::abs(saturator.Process(input) - expected)
                    <= 2e-6 * std::max(1.0, std::abs(expected)));
            }
        }
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: clipping transitions match the reference and remain continuous")
{
    for (float gain : {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 5.0f})
    {
        for (float bias : {0.0f, 0.5f, 1.0f, 2.99f, std::nextafter(3.0f, 0.0f), 3.0f})
        {
            const DualBiasedSaturator saturator(gain, bias);
            for (float boundary : {(3.0f - bias) / gain, (3.0f + bias) / gain})
            {
                const float center = saturator.Process(boundary);
                for (float input : {std::nextafter(boundary, -std::numeric_limits<float>::infinity()),
                                    boundary, std::nextafter(boundary, std::numeric_limits<float>::infinity())})
                {
                    DOCTEST_INFO("gain=" << gain << " bias=" << bias << " boundary=" << boundary);
                    const double expected = ReferenceProcess(gain, bias, input);
                    DOCTEST_CHECK(std::abs(saturator.Process(input) - expected)
                        <= 2e-6 * std::max(1.0, std::abs(expected)));
                    DOCTEST_CHECK(std::abs(saturator.Process(input) - center)
                        <= 2e-6 * std::max(1.0f, std::abs(center)));
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: maximum derivative is conservative and accurate across bias")
{
    for (int index = 0; index <= 8192; ++index)
    {
        const float bias = 3.0f * static_cast<float>(index) / 8192.0f;
        const double peak = ReferenceMaxDerivativeSum(bias);
        for (float gain : {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 5.0f})
        {
            const DualBiasedSaturator saturator{gain, bias};
            const double expected = gain * peak / ReferenceSum(gain, bias);
            const double actual = saturator.MaxDerivative();
            DOCTEST_INFO("gain=" << gain << " bias=" << bias);
            DOCTEST_REQUIRE(std::isfinite(actual));
            DOCTEST_CHECK(actual >= expected);
            DOCTEST_CHECK(actual <= expected * (1.0 + 5e-6));
        }
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: maximum derivative captures the off-origin peak at full bias")
{
    for (float gain : {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 5.0f})
    {
        const DualBiasedSaturator saturator(gain, 3.0f);
        const double expected = gain / ReferenceSum(gain, 3.0f);
        DOCTEST_INFO("gain=" << gain);
        DOCTEST_CHECK(saturator.MaxDerivative() >= expected);
        DOCTEST_CHECK(saturator.MaxDerivative() <= expected * (1.0 + 5e-6));
    }

    const DualBiasedSaturator saturator(3.0f, 3.0f);
    DOCTEST_CHECK(saturator.MaxDerivative() == doctest::Approx(3.0).epsilon(2e-6));
}

DOCTEST_TEST_CASE("DualBiasedSaturator: derivative lookup handles table boundaries without wrapping")
{
    for (size_t index = 0; index < DualBiasedSaturator::x_derivativeTableSize; ++index)
    {
        const float bias = static_cast<float>(DualBiasedSaturator::x_derivativeBiasMin
            + index * DualBiasedSaturator::x_derivativeBiasStep);
        for (float neighbor : {std::nextafter(bias, 0.0f), bias, std::nextafter(bias, 3.0f)})
        {
            const DualBiasedSaturator saturator{1.0f, neighbor};
            const double expected = ReferenceMaxDerivativeSum(neighbor) / ReferenceSum(1.0f, neighbor);
            DOCTEST_INFO("index=" << index << " bias=" << neighbor);
            DOCTEST_CHECK(saturator.MaxDerivative() >= expected);
            DOCTEST_CHECK(saturator.MaxDerivative() <= expected * (1.0 + 5e-6));
        }
    }

    for (float boundary : {static_cast<float>(DualBiasedSaturator::x_derivativeBiasMin), 1.5f})
    {
        const DualBiasedSaturator below{1.0f, std::nextafter(boundary, 0.0f)};
        const DualBiasedSaturator above{1.0f, std::nextafter(boundary, 3.0f)};
        DOCTEST_CHECK(below.MaxDerivative() == doctest::Approx(above.MaxDerivative()).epsilon(2e-6));
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: curve and derivative follow modulated gain and bias")
{
    DualBiasedSaturator saturator{1.0f, 0.0f};
    for (int index = 0; index < 1024; ++index)
    {
        const float gain = 0.25f + 4.75f * static_cast<float>((index * 17) % 1024) / 1023.0f;
        const float bias = 3.0f * static_cast<float>((index * 73) % 1024) / 1023.0f;
        saturator.SetGainAndBias(gain, bias);
        for (float input : {-4.0f, -1.0f, -0.125f, 0.0f, 0.125f, 1.0f, 4.0f})
        {
            const double reference = ReferenceProcess(gain, bias, input);
            DOCTEST_INFO("gain=" << gain << " bias=" << bias << " input=" << input);
            DOCTEST_CHECK(std::abs(saturator.Process(input) - reference)
                <= 2e-6 * std::max(1.0, std::abs(reference)));
        }

        const double expected = saturator.m_gain * ReferenceMaxDerivativeSum(saturator.m_bias)
            / ReferenceSum(saturator.m_gain, saturator.m_bias);
        DOCTEST_CHECK(saturator.MaxDerivative() >= expected);
        DOCTEST_CHECK(saturator.MaxDerivative() <= expected * (1.0 + 5e-6));
    }
}
