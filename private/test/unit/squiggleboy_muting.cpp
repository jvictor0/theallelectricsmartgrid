#include "doctest.h"
#include "../support/SynthRig.hpp"

namespace
{

struct MutingAmpRig
{
    using Amp = SquiggleBoyTrackGeneric<3>::AmpSection;

    TheoryOfTimeBase m_time;
    Amp m_amp;
    Amp::Input m_input;

    MutingAmpRig()
    {
        m_time.m_globalPeriodSamples = 64.0;
        m_time.m_sample.m_running = true;
        m_time.m_sample.m_modulatedPhaseIncrement = 1.0 / 64.0;
        m_input.m_ahdInput.SetTheoryOfTime(&m_time);
        m_input.m_gain.Update(TrioFloat::Splat(1));
        m_amp.m_freqDependentGainTarget = TrioFloat::Splat(1);
        m_amp.m_freqDependentGainSlew.m_output = TrioFloat::Splat(1);
        for (size_t i = 0; i < 3; ++i)
        {
            auto& input = m_input.m_ahdInput.m_channels[i];
            input.m_attackIncrement = 0.25f;
            input.m_decayIncrement = 0.125f;
            input.m_envelopePeriodSamples = 64.0;
            input.m_amplitudePolarity = false;
            input.m_amplitude = static_cast<float>(i) * 0.5f;
        }
    }

    void Process()
    {
        m_amp.Process(m_input, TrioFloat::Splat(1), TrioFloat::Splat(0));
    }
};

}

DOCTEST_TEST_CASE("Amp muting: closed startup is silent and opening preserves configured depth")
{
    GlobalEnv::ResetPerTest();
    MutingAmpRig rig;
    rig.Process();
    DOCTEST_CHECK(rig.m_amp.m_output == TrioFloat::Splat(0));
    for (size_t i = 0; i < 3; ++i)
    {
        rig.m_input.m_mutingASDInput.m_channels[i].m_gate = true;
    }

    for (size_t sample = 0; sample < 4; ++sample)
    {
        rig.Process();
        for (size_t i = 0; i < 3; ++i)
        {
            float ampDepth = static_cast<float>(i) * 0.5f;
            float muteEnv = static_cast<float>(sample + 1) * 0.25f;
            DOCTEST_CHECK(rig.m_amp.m_mutingASD.m_output[i] == muteEnv);
            DOCTEST_CHECK(rig.m_amp.m_ahd.m_channels[i].m_amplitude == 1.0f - (1.0f - ampDepth) * muteEnv);
            DOCTEST_CHECK(rig.m_input.m_ahdInput.m_channels[i].m_amplitude == ampDepth);
        }
    }

    for (size_t sample = 0; sample < 4; ++sample)
    {
        rig.Process();
        DOCTEST_CHECK(rig.m_amp.m_ahd.m_output == TrioFloat(1.0f, 0.5f, 0.0f));
    }

    rig.m_input.m_mutingASDInput.m_channels[0].m_gate = false;
    for (size_t sample = 0; sample < 8; ++sample)
    {
        rig.Process();
        DOCTEST_CHECK(rig.m_amp.m_ahd.m_output[0] == 1.0f - static_cast<float>(sample + 1) * 0.125f);
        DOCTEST_CHECK(rig.m_amp.m_ahd.m_output[1] == 0.5f);
        DOCTEST_CHECK(rig.m_input.m_ahdInput.m_channels[0].m_amplitude == 0.0f);
    }

    DOCTEST_CHECK(rig.m_amp.m_output[0] == 0.0f);
}

DOCTEST_TEST_CASE("Amp muting: copied live timing and raw AHD release close a stopped drone")
{
    GlobalEnv::ResetPerTest();
    MutingAmpRig rig;
    auto& input = rig.m_input.m_ahdInput.m_channels[0];
    input.m_phaseRatio = 2.0;
    input.m_envelopePeriodSamples = 32.0;
    input.m_attackIncrement = 0.5f;
    input.m_decayIncrement = 0.25f;
    rig.m_input.m_mutingASDInput.m_channels[0].m_gate = true;
    rig.Process();
    DOCTEST_CHECK(rig.m_amp.m_mutingASD.m_output[0] == 0.5f);
    rig.Process();
    DOCTEST_CHECK(rig.m_amp.m_mutingASD.m_output[0] == 1.0f);
    const auto& mutingInput = rig.m_input.m_mutingASDInput.m_channels[0];
    DOCTEST_CHECK(mutingInput.m_theoryOfTime == &rig.m_time);
    DOCTEST_CHECK(mutingInput.m_phaseRatio == 2.0);
    DOCTEST_CHECK(mutingInput.m_envelopePeriodSamples == 32.0);
    DOCTEST_CHECK(mutingInput.m_attackIncrement == 0.5f);
    DOCTEST_CHECK(mutingInput.m_decayIncrement == 0.25f);

    rig.m_amp.m_ahd.m_channels[0].m_state = AHD::State::Running;
    rig.m_amp.m_ahd.m_channels[0].m_rawOutput = 0.75;
    rig.m_input.m_mutingASDInput.m_channels[0].m_gate = false;
    rig.m_time.m_sample.m_running = false;
    rig.m_time.m_sample.m_modulatedPhase = 0.0;
    input.m_release = true;
    rig.Process();
    DOCTEST_CHECK_FALSE(input.m_release);
    DOCTEST_CHECK(rig.m_amp.m_ahd.m_channels[0].m_rawOutput == 0.5);
    DOCTEST_CHECK(rig.m_amp.m_mutingASD.m_output[0] == 0.75f);
    DOCTEST_CHECK(rig.m_amp.m_ahd.m_channels[0].m_amplitude == 0.25f);
    DOCTEST_CHECK(rig.m_amp.m_ahd.m_output[0] == 0.875f);
    rig.m_time.m_globalPeriodSamples = 32.0;
    rig.Process();
    DOCTEST_CHECK(rig.m_amp.m_ahd.m_channels[0].m_rawOutput == 0.0);
    DOCTEST_CHECK(rig.m_amp.m_mutingASD.m_output[0] == 0.25f);
    rig.Process();
    DOCTEST_CHECK(rig.m_amp.m_ahd.m_channels[0].m_amplitude == 1.0f);
    DOCTEST_CHECK(rig.m_amp.m_ahd.m_output[0] == 0.0f);
    DOCTEST_CHECK(rig.m_amp.m_output[0] == 0.0f);
    DOCTEST_CHECK(input.m_amplitude == 0.0f);
}

DOCTEST_TEST_CASE("Amp muting: production gate is exactly ToT running and each voice unmuted")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& time = internal.m_nonagon.m_nonagon.m_theoryOfTime;
    auto& logic = internal.m_nonagon.m_state.m_trigLogic;
    for (bool running : {false, true})
    {
        time.m_sample.m_running = running;
        internal.m_running = !running;
        for (size_t mutedVoice = 0; mutedVoice < SquiggleBoy::x_numVoices; ++mutedVoice)
        {
            for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
            {
                logic.m_mute[voice] = voice == mutedVoice;
                logic.m_earlyMuted[voice] = true;
                logic.m_pitchChanged[voice] = false;
                logic.m_subTrigger[voice] = false;
            }

            internal.SetSquiggleBoyInputs();
            for (auto& trackInput : internal.m_squiggleBoy.m_trackState)
            {
                trackInput.SetGates();
            }

            for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
            {
                auto& trackInput = internal.m_squiggleBoy.m_trackState[voice / SquiggleBoy::x_voicesPerTrack];
                size_t lane = voice % SquiggleBoy::x_voicesPerTrack;
                bool gate = running && voice != mutedVoice;
                DOCTEST_CHECK(trackInput.m_mutingControl[lane].m_gate == gate);
                DOCTEST_CHECK(trackInput.m_ampInput.m_mutingASDInput.m_channels[lane].m_gate == gate);
            }
        }
    }
}
