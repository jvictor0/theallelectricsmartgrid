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
    for (size_t voice = 0; voice < SquiggleBoy::x_numVoices; ++voice)
    {
        DOCTEST_CHECK(ui.m_voiceFilterUIState[voice].Sample().size() > 1);
        DOCTEST_CHECK(ui.m_voiceSourceUIState[voice].m_physicalModelingUIState.Sample().size() > 1);
    }

    DOCTEST_CHECK(ui.m_delayUIState.m_dampingFilter.Sample().size() > 1);
    DOCTEST_CHECK(ui.m_reverbUIState.m_dampingFilter.Sample().size() > 1);
    DOCTEST_CHECK(ui.m_stereoMasteringChainUIState.Sample().size() > 1);
    for (auto& source : ui.m_sourceMixerUIState.m_sources)
    {
        DOCTEST_CHECK(source.Sample().size() > 1);
    }

    for (auto& voice : engine.m_voiceConfig)
    {
        voice.m_filterMachine = VoiceMachine::FilterMachine::SVF2Pole;
    }

    rig.RunSamples(SampleTimer::x_oversample);
    auto samples = ui.m_voiceFilterUIState[0].Sample();
    DOCTEST_REQUIRE(samples.size() == 1);
    DOCTEST_CHECK(ui.m_voiceFilterUIState[0].m_readSnapshots.front().m_filterMachine
        == VoiceMachine::FilterMachine::SVF2Pole);
}
