#include "doctest.h"

#include <array>
#include <cmath>

#include "../support/SynthRig.hpp"

namespace
{
using FilterSection = SquiggleBoyVoice::FilterSection;
using FilterMachine = VoiceMachine::FilterMachine;
}

DOCTEST_TEST_CASE("filter bias: both controls map zero midpoint and maximum before slewing")
{
    FilterSection::Input input;
    for (auto* parameter : {&input.m_inputBias, &input.m_saturationBias})
    {
        DOCTEST_CHECK(parameter->m_expParam == 0.0f);
        DOCTEST_CHECK(parameter->Update(0.0f) == 0.0f);
        DOCTEST_CHECK(parameter->Update(0.25f) == doctest::Approx(0.1545084972f));
        DOCTEST_CHECK(parameter->Update(0.5f) == doctest::Approx(0.5f));
        DOCTEST_CHECK(parameter->Update(0.75f) == doctest::Approx(1.272542486f));
        DOCTEST_CHECK(parameter->Update(1.0f) == doctest::Approx(3.0f));
        DOCTEST_CHECK(parameter->Update(0.0f) == 0.0f);
    }
}

DOCTEST_TEST_CASE("filter bias: input bias affects both modes and symmetric bias affects only the ladder")
{
    GlobalEnv::ResetPerTest();
    for (FilterMachine machine : {FilterMachine::Ladder4Pole, FilterMachine::SVF2Pole})
    {
        SquiggleBoyVoiceConfig config;
        config.m_filterMachine = machine;
        std::array<FilterSection, 3> filters;
        std::array<FilterSection::Input, 3> inputs;
        for (auto& input : inputs)
        {
            input.m_voiceConfig = &config;
            input.m_vcoBaseFreq = 440.0f / 48000.0f;
            input.m_saturationGain.Update(0.75f);
            input.m_lpCutoffFactor.Update(0.25f);
            input.m_sampleRateReducerFreq.Update(1.0f);
        }

        inputs[1].m_inputBias.Update(0.5f);
        inputs[2].m_saturationBias.Update(0.5f);
        std::array<float, SquiggleBoyVoice::x_uBlockSize> source;
        source.fill(0.2f);
        std::array<SampleTop, SampleTimer::x_controlFrameRate> tops;
        double inputBiasDifference = 0.0;
        double symmetricBiasDifference = 0.0;
        for (int block = 0; block < 64; ++block)
        {
            for (size_t i = 0; i < filters.size(); ++i)
            {
                filters[i].ProcessUBlock(inputs[i], source.data(), tops.data());
            }

            if (block == 0)
            {
                const float alpha = filters[1].m_inputBiasSlew.m_filter.m_alpha;
                const double expected = 0.5 * (1.0 - std::pow(1.0 - alpha, source.size()));
                DOCTEST_CHECK(filters[1].m_inputBiasSlew.m_filter.m_output == doctest::Approx(expected));
                DOCTEST_CHECK(filters[2].m_saturationBiasSlew.m_filter.m_output == doctest::Approx(expected));
            }

            for (size_t i = 0; i < source.size(); ++i)
            {
                const double inputDelta = filters[1].m_uBlockOutput[i] - filters[0].m_uBlockOutput[i];
                const double symmetricDelta = filters[2].m_uBlockOutput[i] - filters[0].m_uBlockOutput[i];
                inputBiasDifference += inputDelta * inputDelta;
                symmetricBiasDifference += symmetricDelta * symmetricDelta;
            }
        }

        DOCTEST_CHECK(filters[1].m_inputBiasSlew.m_filter.m_output == doctest::Approx(0.5f));
        DOCTEST_CHECK(filters[2].m_saturationBiasSlew.m_filter.m_output == doctest::Approx(0.5f));
        DOCTEST_CHECK(inputBiasDifference > 1e-5);
        if (machine == FilterMachine::Ladder4Pole)
        {
            DOCTEST_CHECK(filters[2].m_lpLadder.m_saturator.m_bias == doctest::Approx(0.5f));
            DOCTEST_CHECK(symmetricBiasDifference > 1e-5);
        }
        else
        {
            DOCTEST_CHECK(symmetricBiasDifference == 0.0);
        }
    }
}

DOCTEST_TEST_CASE("filter bias: Shape encoders update voice parameters and slew toward changed targets")
{
    synthrig::SynthRig rig;
    auto& synth = rig.Internal().m_squiggleBoy;
    synth.m_encoders.SelectBank(SmartGridOneEncoders::Bank::Shape);
    rig.SetEncoder(1, 2, 0.5f);
    rig.SetEncoder(2, 2, 1.0f);
    const size_t latencySamples = SmartGrid::MessageInLatency::x_latencyUs * SampleTimer::x_sampleRate / 1000000;
    rig.RunSamples(latencySamples + 64);

    // EncoderSet transports a 14-bit value, so its midpoint is 8192/16383.
    //
    const double expectedInputBias = (std::pow(25.0, 8192.0 / 16383.0) - 1.0) / 8.0;

    const auto& input = synth.m_state[0].m_filterInput;
    const auto& filter = synth.m_voices[0].m_filter;
    DOCTEST_CHECK(input.m_inputBias.m_expParam == doctest::Approx(expectedInputBias));
    DOCTEST_CHECK(input.m_saturationBias.m_expParam == doctest::Approx(3.0f));
    DOCTEST_CHECK(filter.m_inputBiasSlew.m_target == doctest::Approx(expectedInputBias));
    DOCTEST_CHECK(filter.m_saturationBiasSlew.m_target == doctest::Approx(3.0f));
    DOCTEST_CHECK(filter.m_lpLadder.m_saturator.m_bias > 0.0f);
    DOCTEST_CHECK(filter.m_lpLadder.m_saturator.m_bias <= 3.0f);

    rig.RunSamples(1024);
    DOCTEST_CHECK(filter.m_inputBiasSlew.m_filter.m_output == doctest::Approx(expectedInputBias));
    DOCTEST_CHECK(filter.m_lpLadder.m_saturator.m_bias == doctest::Approx(3.0f));
    rig.SetEncoder(1, 2, 0.0f);
    rig.SetEncoder(2, 2, 0.0f);
    rig.RunSamples(8);
    DOCTEST_CHECK(filter.m_inputBiasSlew.m_target == 0.0f);
    DOCTEST_CHECK(filter.m_saturationBiasSlew.m_target == 0.0f);
    DOCTEST_CHECK(filter.m_inputBiasSlew.m_filter.m_output > 0.0f);
    DOCTEST_CHECK(filter.m_inputBiasSlew.m_filter.m_output < 0.5f);
    DOCTEST_CHECK(filter.m_lpLadder.m_saturator.m_bias > 0.0f);
    DOCTEST_CHECK(filter.m_lpLadder.m_saturator.m_bias < 3.0f);
}
