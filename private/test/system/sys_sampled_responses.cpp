#include "doctest.h"
#include "../support/SynthRig.hpp"

DOCTEST_TEST_CASE("Sampled response wiring: every displayed filter publishes at audio cadence")
{
    synthrig::SynthRig rig;
    auto& engine = rig.Internal().m_squiggleBoy;
    auto& ui = rig.UIState().m_squiggleBoyUIState;
    for (auto& voice : engine.m_voiceConfig)
    {
        voice.m_sourceMachine = VoiceMachine::SourceMachine::PhysicalModeling;
    }

    rig.RunSamples(4096);
    for (size_t track = 0; track < SquiggleBoy::x_numTracks; ++track)
    {
        DOCTEST_CHECK(ui.m_voiceFilterUIState[track].Sample().size() > 1);
        DOCTEST_CHECK(ui.m_physicalModelingUIState[track].Sample().size() > 1);
    }

    DOCTEST_CHECK(ui.m_delayUIState.m_dampingFilter.Sample().size() > 1);
    DOCTEST_CHECK(ui.m_reverbUIState.m_dampingFilter.Sample().size() > 1);
    for (auto* damping : {&ui.m_delayUIState.m_dampingFilter, &ui.m_reverbUIState.m_dampingFilter})
    {
        DOCTEST_CHECK(damping->NumLanes() == 4);
        auto samples = damping->Sample();
        for (double hz : {20.0, 1000.0, 12000.0, 24000.0})
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                const auto response = damping->ResponseHz(samples, hz, SampleTimer::x_sampleRate, lane);
                DOCTEST_CHECK(std::isfinite(response.m_lower));
                DOCTEST_CHECK(std::isfinite(response.m_upper));
                DOCTEST_CHECK(response.m_lower >= 0);
                DOCTEST_CHECK(response.m_lower <= response.m_median);
                DOCTEST_CHECK(response.m_median <= response.m_upper);
            }
        }
    }

    DOCTEST_CHECK(ui.m_stereoMasteringChainUIState.Sample().size() > 1);
    DOCTEST_CHECK(ui.m_sourceMixerUIState.Sample().size() > 1);
    DOCTEST_CHECK(ui.m_sourceMixerUIState.NumLanes() == 4);
    DOCTEST_CHECK(ui.m_voiceFilterUIState[0].NumLanes() == 3);
    DOCTEST_CHECK(ui.m_physicalModelingUIState[0].NumLanes() == 3);

    for (auto& voice : engine.m_voiceConfig)
    {
        voice.m_filterMachine = VoiceMachine::FilterMachine::SVF2Pole;
    }

    rig.RunSamples(SampleTimer::x_oversample);
    auto samples = ui.m_voiceFilterUIState[0].Sample();
    DOCTEST_REQUIRE(samples.size() == 1);
    DOCTEST_CHECK(ui.m_voiceFilterUIState[0].m_readSnapshots.front().m_filterMachine
        == VoiceMachine::FilterMachine::SVF2Pole);

    for (auto& voice : engine.m_voiceConfig)
    {
        voice.m_filterMachine = VoiceMachine::FilterMachine::NaiveReuppingLadder;
    }

    rig.RunSamples(SampleTimer::x_oversample);
    DOCTEST_REQUIRE(ui.m_voiceFilterUIState[0].Sample().size() == 1);
    DOCTEST_CHECK(ui.m_voiceFilterUIState[0].m_readSnapshots.front().m_filterMachine
        == VoiceMachine::FilterMachine::NaiveReuppingLadder);
}
