#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "QuadReverb.hpp"

#include <memory>

namespace
{

QuadReverb::LogicalSpec ReverbSpec(float feedback, float saturatorGain)
{
    return {QuadFloat::Splat(2048.0f), QuadFloat::Splat(feedback),
        QuadFloat::Splat(1.0f / 65536.0f), QuadFloat::Splat(65536.0f),
        QuadFloat::Splat(saturatorGain)};
}

double ReverbImpulseEnergy(float feedback, float saturatorGain, size_t startSample)
{
    auto reverb = std::make_unique<QuadReverb>();
    reverb->ComputeCoefficients(ReverbSpec(feedback, saturatorGain));
    double energy = 0;
    for (size_t sample = 0; sample < 192000; ++sample)
    {
        const auto input = QuadFloat(sample == 0 ? 0.01f : 0.0f, 0, 0, 0);
        const auto output = reverb->Process(input);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_REQUIRE(std::isfinite(output[channel]));
            if (sample >= startSample)
            {
                energy += output[channel] * output[channel];
            }
        }
    }

    return energy / 0.0001;
}

}

DOCTEST_TEST_CASE("QuadReverb SAT changes the wet return without feedback")
{
    GlobalEnv::ResetPerTest();
    const double minimumSat = ReverbImpulseEnergy(0.0f, 0.5f, 0);
    const double maximumSat = ReverbImpulseEnergy(0.0f, 4.0f, 0);
    DOCTEST_CHECK(minimumSat > 0.05);
    DOCTEST_CHECK(minimumSat < 0.15);
    DOCTEST_CHECK(maximumSat / minimumSat == doctest::Approx(64.0).epsilon(0.01));
}

DOCTEST_TEST_CASE("QuadReverb feedback sustains the late tail")
{
    GlobalEnv::ResetPerTest();
    double dryTail = ReverbImpulseEnergy(0.0f, QuadReverb::x_defaultSaturatorGain, 144000);
    double feedbackTail = ReverbImpulseEnergy(1.25f, QuadReverb::x_defaultSaturatorGain, 144000);
    DOCTEST_CHECK(feedbackTail > 1e-4);
    DOCTEST_CHECK(feedbackTail > 5.0 * dryTail);
    DOCTEST_CHECK(feedbackTail < 1.0);
}

DOCTEST_TEST_CASE("QuadReverb silent input does not generate a tail at ordinary feedback")
{
    GlobalEnv::ResetPerTest();
    auto reverb = std::make_unique<QuadReverb>();
    reverb->ComputeCoefficients(ReverbSpec(0.7f, QuadReverb::x_defaultSaturatorGain));
    for (size_t sample = 0; sample < 8192; ++sample)
    {
        const auto output = reverb->Process(QuadFloat());
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_REQUIRE(output[channel] == 0.0f);
        }
    }
}
