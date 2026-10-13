#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Filter.hpp"

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

double ReferenceProcess(float gain, float bias, float input)
{
    const double driven = static_cast<double>(gain) * input;
    const double numerator = ReferenceTanh(driven + bias) + ReferenceTanh(driven - bias);
    const double denominator = ReferenceTanh(static_cast<double>(gain) + bias)
                             + ReferenceTanh(static_cast<double>(gain) - bias);
    return numerator / denominator;
}

}

DOCTEST_TEST_CASE("DualBiasedSaturator: finite normalized output across the parameter range")
{
    for (float gain : {std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::min(),
                       1e-20f, 1e-5f, 0.01f, 0.1f, 1.0f, 4.0f, 32.0f, std::numeric_limits<float>::max()})
    {
        for (float bias : {0.0f, 1.0f, 2.0f, 2.9999f, std::nextafter(3.0f, 0.0f), 3.0f})
        {
            DOCTEST_INFO("gain=" << gain << " bias=" << bias);
            const DualBiasedSaturator saturator{gain, bias};
            DOCTEST_REQUIRE(std::isfinite(saturator.Process(0.0f)));
            DOCTEST_CHECK(saturator.Process(0.0f) == 0.0f);
            DOCTEST_CHECK(saturator.Process(1.0f) == 1.0f);
            DOCTEST_CHECK(saturator.Process(-1.0f) == -1.0f);

            float previous = 0.0f;
            for (float input : {0.0f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                const float output = saturator.Process(input);
                DOCTEST_REQUIRE(std::isfinite(output));
                DOCTEST_CHECK(output >= previous);
                DOCTEST_CHECK(output <= 1.0f);
                DOCTEST_CHECK(saturator.Process(-input) == -output);
                previous = output;
            }
        }
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: small gain near maximum bias preserves the curve")
{
    struct Example
    {
        float m_gain;
        float m_bias;
        double m_expected;
    };

    // Independent high-precision evaluations of the two rational tanh terms
    // at input 0.5, using the exact binary float parameter values.
    //
    const Example examples[] =
    {
        {0.01f, 3.0f, 0.124687500659657},
        {0.01f, 2.9999f, 0.128424037042056},
        {1e-4f, 2.9999f, 0.406096177387841},
        {1e-5f, 2.9999f, 0.498751434874790}
    };

    for (const Example& example : examples)
    {
        DOCTEST_INFO("gain=" << example.m_gain << " bias=" << example.m_bias);
        const DualBiasedSaturator saturator{example.m_gain, example.m_bias};
        DOCTEST_REQUIRE(std::isfinite(saturator.Process(0.5f)));
        DOCTEST_CHECK(saturator.Process(0.5f) == doctest::Approx(example.m_expected).epsilon(2e-6));
    }

    // At bias 3 the low-gain normalized curve tends to x cubed.
    //
    const DualBiasedSaturator tinyGain{std::numeric_limits<float>::denorm_min(), 3.0f};
    DOCTEST_REQUIRE(std::isfinite(tinyGain.Process(0.5f)));
    DOCTEST_CHECK(tinyGain.Process(0.5f) == doctest::Approx(0.125f).epsilon(2e-6));
    DOCTEST_CHECK(tinyGain.Process(0.25f) == doctest::Approx(0.015625f).epsilon(2e-6));
}

DOCTEST_TEST_CASE("DualBiasedSaturator: matches direct rational tanh away from cancellation")
{
    for (float gain : {0.5f, 1.0f, 2.0f, 4.0f, 32.0f})
    {
        for (float bias : {0.0f, 0.5f, 1.0f, 2.0f, 2.99f, 3.0f})
        {
            const DualBiasedSaturator saturator{gain, bias};
            for (int index = -128; index <= 128; ++index)
            {
                const float input = static_cast<float>(index) / 16.0f;
                const double expected = ReferenceProcess(gain, bias, input);
                DOCTEST_INFO("gain=" << gain << " bias=" << bias << " input=" << input);
                DOCTEST_CHECK(std::abs(saturator.Process(input) - expected)
                              <= 2e-6 * std::max(1.0, std::abs(expected)));
            }
        }
    }
}

DOCTEST_TEST_CASE("DualBiasedSaturator: clipping transitions are continuous")
{
    for (float bias : {0.0f, 0.5f, 1.0f, 2.99f, 3.0f})
    {
        const DualBiasedSaturator saturator{1.0f, bias};
        for (float boundary : {3.0f - bias, 3.0f + bias})
        {
            const float center = saturator.Process(boundary);
            for (float input : {std::nextafter(boundary, -std::numeric_limits<float>::infinity()),
                                std::nextafter(boundary, std::numeric_limits<float>::infinity())})
            {
                DOCTEST_INFO("bias=" << bias << " boundary=" << boundary);
                const double expected = ReferenceProcess(1.0f, bias, input);
                DOCTEST_CHECK(std::abs(saturator.Process(input) - expected)
                              <= 2e-6 * std::max(1.0, std::abs(expected)));
                DOCTEST_CHECK(std::abs(saturator.Process(input) - center)
                              <= 2e-6 * std::max(1.0f, std::abs(center)));
            }
        }
    }
}
