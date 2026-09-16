#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "QuadReverb.hpp"

#include <memory>

namespace
{

QuadReverb::Input ReverbInput(float feedback)
{
    QuadReverb::Input input;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        input.m_delayTimeSamples[channel] = 2048.0f;
        input.m_feedback[channel] = feedback;
        input.m_bffBase[channel] = 1.0f / 65536.0f;
        input.m_bffWidth[channel] = 65536.0f;
        input.m_lfoInput.m_freq[channel] = 0.5f / 48000.0f;
    }

    return input;
}

double ReverbImpulseEnergy(float feedback, size_t startSample)
{
    auto reverb = std::make_unique<QuadReverb>();
    auto input = ReverbInput(feedback);
    double energy = 0;
    for (size_t sample = 0; sample < 48000; ++sample)
    {
        input.m_input = QuadFloat(sample == 0 ? 0.01f : 0.0f, 0, 0, 0);
        QuadFloat output = reverb->Process(input);
        input.m_return = output;
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

DOCTEST_TEST_CASE("QuadReverb impulse produces a substantial wet return without feedback")
{
    GlobalEnv::ResetPerTest();
    double energy = ReverbImpulseEnergy(0.0f, 0);
    DOCTEST_CHECK(energy > 0.3);
    DOCTEST_CHECK(energy < 0.7);
}

DOCTEST_TEST_CASE("QuadReverb feedback sustains the late tail")
{
    GlobalEnv::ResetPerTest();
    double dryTail = ReverbImpulseEnergy(0.0f, 12000);
    double feedbackTail = ReverbImpulseEnergy(0.7f, 12000);
    DOCTEST_CHECK(feedbackTail > 1e-4);
    DOCTEST_CHECK(feedbackTail > 10.0 * dryTail);
    DOCTEST_CHECK(feedbackTail < 1.0);
}

DOCTEST_TEST_CASE("QuadReverb silent input does not generate a tail at ordinary feedback")
{
    GlobalEnv::ResetPerTest();
    auto reverb = std::make_unique<QuadReverb>();
    auto input = ReverbInput(0.7f);
    for (size_t sample = 0; sample < 8192; ++sample)
    {
        input.m_return = reverb->Process(input);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_REQUIRE(input.m_return[channel] == 0.0f);
        }
    }
}
