#include "doctest.h"

#include <memory>

#include "SquiggleBoy.hpp"
#include "../support/GlobalEnv.hpp"

DOCTEST_TEST_CASE("AHD: scope starts and retriggers end once at an idle voice crossing")
{
    GlobalEnv::ResetPerTest();
    auto scopes = std::make_unique<ScopeWriterGeneric<1, 3>>(1, 4096);
    using Amp = SquiggleBoyTrackGeneric<1>::AmpSection;
    Amp amp;
    Amp::Input input;
    TheoryOfTimeBase time;
    time.m_globalPeriodSamples = 8.0;
    input.m_vcoTargetFreq = 50.0f / 48000.0f;
    input.m_gain.Update(1.0f);
    input.m_subGain.Update(0.0f);
    input.m_subDrive.Update(0.5f);
    amp.m_subOut = 0.0f;
    amp.m_scopeWriter = ScopeWriterHolder(scopes.get(), 0);
    amp.m_ampEnvelopeScopeWriter = ScopeWriterHolder(scopes.get(), 1);
    amp.m_modulationEnvelopeScopeWriter = ScopeWriterHolder(scopes.get(), 2);
    AHD::Input& ahdInput = input.m_ahdInput.m_channels[0];
    AHD::Input& modulationAHDInput = input.m_modulationAHDInput.m_channels[0];
    ahdInput.m_theoryOfTime = &time;
    ahdInput.m_attackIncrement = 0.125f;
    ahdInput.m_decayIncrement = 1.0f;
    modulationAHDInput = ahdInput;
    const AHD& ahd = amp.m_ahd.m_channels[0];
    const AHD& modulationAHD = amp.m_modulationAHD.m_channels[0];

    AHD::AHDControl control;
    control.m_envelopePeriodSamples = 8.0;
    control.m_voiceCellCrossed = true;
    ahdInput.Set(control);
    modulationAHDInput.Set(control);
    scopes->m_index = 5;
    amp.Process(input, 1.0f, 0.0f);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        const auto& markers = scopes->GetMarkers(scope, 0);
        DOCTEST_CHECK(markers.m_startIndexIndex == 0);
        DOCTEST_CHECK(markers.m_endIndices[ScopeWriter::x_numStartIndices - 1] == 0);
        DOCTEST_CHECK_FALSE(markers.IsStarted());
    }

    control.m_trig = true;
    control.m_release = true;
    ahdInput.Set(control);
    modulationAHDInput.Set(control);
    scopes->m_index = 10;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK_FALSE(ahdInput.m_trig);
    DOCTEST_CHECK_FALSE(ahdInput.m_release);
    DOCTEST_CHECK_FALSE(modulationAHDInput.m_trig);
    DOCTEST_CHECK_FALSE(modulationAHDInput.m_release);
    DOCTEST_CHECK_FALSE(ahdInput.m_voiceCellCrossed);
    DOCTEST_CHECK_FALSE(modulationAHDInput.m_voiceCellCrossed);
    DOCTEST_CHECK(ahd.m_voiceCellCrossed);
    DOCTEST_CHECK(modulationAHD.m_voiceCellCrossed);
    DOCTEST_CHECK(control.m_voiceCellCrossed);
    DOCTEST_CHECK(control.m_trig);
    DOCTEST_CHECK(control.m_release);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndexIndex == 1);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndices[0] == 10.0);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_endIndices[0] == 0);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).IsStarted());
    }

    time.m_sample.m_modulatedPhase = 0.5;
    scopes->m_index = 14;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(ahd.m_rawOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK(modulationAHD.m_rawOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK_FALSE(ahd.m_voiceCellCrossed);
    DOCTEST_CHECK_FALSE(modulationAHD.m_voiceCellCrossed);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndexIndex == 1);
    }

    ahdInput.Set(control);
    modulationAHDInput.Set(control);
    scopes->m_index = 15;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(ahd.m_startOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK(modulationAHD.m_startOutput == doctest::Approx(0.5f));
    DOCTEST_CHECK(scopes->GetMarkers(0, 0).m_endIndices[0] == 15);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndexIndex == 2);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndices[1] == 15.0);
    }

    control.m_trig = false;
    control.m_voiceCellCrossed = false;
    ahdInput.Set(control);
    modulationAHDInput.Set(control);
    scopes->m_index = 16;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(ahd.m_state == AHD::State::Idle);
    DOCTEST_CHECK(modulationAHD.m_state == AHD::State::Idle);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndexIndex == 2);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_endIndices[1] == 0);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).IsStarted());
    }

    control.m_release = false;
    control.m_voiceCellCrossed = true;
    for (size_t index : {20, 24})
    {
        ahdInput.Set(control);
        modulationAHDInput.Set(control);
        scopes->m_index = index;
        amp.Process(input, 1.0f, 0.0f);
        DOCTEST_CHECK_FALSE(ahd.m_changed);
        DOCTEST_CHECK_FALSE(modulationAHD.m_changed);
        for (size_t scope = 0; scope < 3; ++scope)
        {
            DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_endIndices[1] == 20);
            DOCTEST_CHECK_FALSE(scopes->GetMarkers(scope, 0).IsStarted());
        }
    }

    control.m_trig = true;
    ahdInput.Set(control);
    modulationAHDInput.Set(control);
    scopes->m_index = 28;
    amp.Process(input, 1.0f, 0.0f);
    control.m_trig = false;
    control.m_release = true;
    ahdInput.Set(control);
    modulationAHDInput.Set(control);
    scopes->m_index = 29;
    amp.Process(input, 1.0f, 0.0f);
    DOCTEST_CHECK(ahd.m_state == AHD::State::Idle);
    DOCTEST_CHECK(modulationAHD.m_state == AHD::State::Idle);
    for (size_t scope = 0; scope < 3; ++scope)
    {
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_startIndices[2] == 28.0);
        DOCTEST_CHECK(scopes->GetMarkers(scope, 0).m_endIndices[2] == 29);
        DOCTEST_CHECK_FALSE(scopes->GetMarkers(scope, 0).IsStarted());
    }
}
