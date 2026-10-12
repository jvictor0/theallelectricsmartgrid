#include "doctest.h"

#include <memory>

#include "SquiggleBoy.hpp"
#include "../support/GlobalEnv.hpp"

DOCTEST_TEST_CASE("AHD: consumed voice events preserve scope starts retriggers and ends")
{
    GlobalEnv::ResetPerTest();
    auto scopes = std::make_unique<ScopeWriter>(1, 3);
    SquiggleBoyVoice::AmpSection amp;
    SquiggleBoyVoice::AmpSection::Input input;
    SquiggleBoyVoice::VoiceConfig config;
    TheoryOfTimeBase time;
    input.m_voiceConfig = &config;
    input.m_vcoTargetFreq = 50.0f / 48000.0f;
    input.m_gain.Update(1.0f);
    input.m_subGain.Update(0.0f);
    input.m_subTanhGain.Update(0.5f);
    amp.m_subOut = 0.0f;
    amp.m_scopeWriter = ScopeWriterHolder(scopes.get(), 0, 0);
    amp.m_ampEnvelopeScopeWriter = ScopeWriterHolder(scopes.get(), 0, 1);
    amp.m_modulationEnvelopeScopeWriter = ScopeWriterHolder(scopes.get(), 0, 2);
    input.m_ahdInput.m_theoryOfTime = &time;
    input.m_ahdInput.m_attackIncrement = 0.125f;
    input.m_ahdInput.m_decayIncrement = 1.0f;
    input.m_modulationAHDInput = input.m_ahdInput;

    AHD::AHDControl control;
    control.m_envelopePeriodSamples = 8.0;
    control.m_trig = true;
    control.m_release = true;
    input.m_ahdInput.Set(control);
    input.m_modulationAHDInput.Set(control);
    scopes->m_index = 10;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK_FALSE(input.m_ahdInput.m_trig);
    DOCTEST_CHECK_FALSE(input.m_ahdInput.m_release);
    DOCTEST_CHECK_FALSE(input.m_modulationAHDInput.m_trig);
    DOCTEST_CHECK_FALSE(input.m_modulationAHDInput.m_release);
    DOCTEST_CHECK(control.m_trig);
    DOCTEST_CHECK(control.m_release);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->m_advanceStartIndices[scope][0] == 1);
        DOCTEST_CHECK(scopes->m_startIndices[scope][0][0] == 10.0);
    }

    time.m_samples[0].m_modulatedPhase = 0.5;
    scopes->m_index = 14;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(amp.m_ahd.m_rawOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK(amp.m_modulationAHD.m_rawOutput == doctest::Approx(0.5f));
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->m_advanceStartIndices[scope][0] == 1);
    }

    input.m_ahdInput.Set(control);
    input.m_modulationAHDInput.Set(control);
    scopes->m_index = 15;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(amp.m_ahd.m_startOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK(amp.m_modulationAHD.m_startOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK(scopes->m_endIndices[0][0][0] == 15);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->m_advanceStartIndices[scope][0] == 2);
        DOCTEST_CHECK(scopes->m_startIndices[scope][0][1] == 15.0);
    }

    control.m_trig = false;
    input.m_ahdInput.Set(control);
    input.m_modulationAHDInput.Set(control);
    scopes->m_index = 16;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(amp.m_ahd.m_state == AHD::State::Idle);
    DOCTEST_CHECK(amp.m_modulationAHD.m_state == AHD::State::Idle);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->m_advanceStartIndices[scope][0] == 2);
        DOCTEST_CHECK(scopes->m_endIndices[scope][0][1] == 16);
    }
}
