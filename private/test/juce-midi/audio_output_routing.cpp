#include "doctest.h"
#include "NonagonWrapper.hpp"
#include "support/GlobalEnv.hpp"
#include "support/TempDir.hpp"

DOCTEST_TEST_CASE("audio output routing duplicates stereo only when outputs 3 and 4 are active")
{
    bool stereo = true;
    DOCTEST_SUBCASE("stereo")
    {
    }

    DOCTEST_SUBCASE("quad")
    {
        stereo = false;
    }

    for (const auto& layout : {std::pair{0, 1}, {0, 2}, {4, 2}, {4, 3}, {0, 4}, {0, 5}, {0, 7}})
    {
        const int numInputs = layout.first;
        const int numOutputs = layout.second;
        DOCTEST_CAPTURE(stereo);
        DOCTEST_CAPTURE(numInputs);
        DOCTEST_CAPTURE(numOutputs);
        GlobalEnv::ResetPerTest();
        synthrig::TempDir directory;
        DOCTEST_REQUIRE(directory.Valid());
        auto app = std::make_unique<NonagonWrapper>();
        app->SetRecordingDirectory(directory.String().c_str());
        app->PrepareToPlay(512, 48000);

        // Seed distinct source lanes for the sample before the next DSP control update.
        //
        auto& synth = app->m_internal.m_squiggleBoy;
        synth.m_sourceMixerState.m_sources[0].m_config.m_width = SourceMixer::SourceWidth::Stereo;
        synth.m_sourceMixer.m_sources[0].m_output[0] = 0.25f;
        synth.m_sourceMixer.m_sources[0].m_output[1] = -0.5f;

        const int numChannels = std::max(numInputs, numOutputs);
        juce::AudioBuffer<float> buffer(numChannels, 3);
        for (int channel = 0; channel < numChannels; ++channel)
        {
            for (int sample = 0; sample < 3; ++sample)
            {
                buffer.setSample(channel, sample, 0.75f);
            }
        }

        const juce::AudioSourceChannelInfo block(&buffer, 1, 1);
        const NonagonWrapper::IOInfo io{numInputs, numOutputs, std::min(numOutputs, stereo ? 2 : 4), stereo};
        app->Process(block, io);
        const auto& output = app->m_internal.m_output;
        DOCTEST_REQUIRE(output.m_stereoOutput[0] != 0.0f);
        DOCTEST_REQUIRE(output.m_stereoOutput[1] != 0.0f);
        DOCTEST_REQUIRE(output.m_stereoOutput[0] != output.m_stereoOutput[1]);

        float expected[7] = {};
        if (stereo)
        {
            expected[0] = output.m_stereoOutput[0];
            expected[1] = output.m_stereoOutput[1];
            if (numOutputs >= 4)
            {
                expected[2] = output.m_stereoOutput[0];
                expected[3] = output.m_stereoOutput[1];
            }
        }
        else
        {
            for (int channel = 0; channel < std::min(numOutputs, 4); ++channel)
            {
                expected[channel] = output.m_output[channel];
            }

            if (numOutputs >= 7)
            {
                expected[5] = output.m_stereoOutput[0];
                expected[6] = output.m_stereoOutput[1];
            }
        }

        if (numOutputs > 4)
        {
            expected[4] = output.m_sub;
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            DOCTEST_CHECK(buffer.getSample(channel, 0) == 0.75f);
            DOCTEST_CHECK(buffer.getSample(channel, 1) == expected[channel]);
            DOCTEST_CHECK(buffer.getSample(channel, 2) == 0.75f);
        }
    }
}
