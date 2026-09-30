#include "doctest.h"
#include "../support/SynthRig.hpp"

#include <cstring>
#include <new>

DOCTEST_TEST_CASE("Patch startup: initial trio drives encoder and visualizer selection")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);
    DOCTEST_CHECK(rig.Internal().m_activeTrio == TheNonagonSmartGrid::Trio::Water);
    DOCTEST_CHECK(rig.Internal().m_squiggleBoy.m_encoders.GetCurrentTrack() == 0);
    DOCTEST_CHECK(rig.UIState().m_squiggleBoyUIState.m_activeTrack.load() == 0);
}

DOCTEST_TEST_CASE("Patch startup: loaded trio drives encoder and visualizer selection")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    internal.SetActiveTrio(TheNonagonSmartGrid::Trio::Fire);
    std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());
    internal.SetActiveTrio(TheNonagonSmartGrid::Trio::Water);

    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(2);
    DOCTEST_CHECK(internal.m_activeTrio == TheNonagonSmartGrid::Trio::Fire);
    DOCTEST_CHECK(internal.m_squiggleBoy.m_encoders.GetCurrentTrack() == 2);
    DOCTEST_CHECK(rig.UIState().m_squiggleBoyUIState.m_activeTrack.load() == 2);
}

DOCTEST_TEST_CASE("Patch startup: new patch keeps trio selection synchronized")
{
    synthrig::SynthRig rig;
    rig.Internal().SetActiveTrio(TheNonagonSmartGrid::Trio::Fire);
    rig.ResetToDefaults();
    rig.RunFrames(2);
    DOCTEST_CHECK(rig.Internal().m_activeTrio == TheNonagonSmartGrid::Trio::Water);
    DOCTEST_CHECK(rig.Internal().m_squiggleBoy.m_encoders.GetCurrentTrack() == 0);
    DOCTEST_CHECK(rig.UIState().m_squiggleBoyUIState.m_activeTrack.load() == 0);
}

DOCTEST_TEST_CASE("Patch startup: stopped load accepts harmonic parameters without clock events")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& requested = internal.m_nonagon.m_state.m_lameJuisInput;
    auto& actual = internal.m_nonagon.m_nonagon.m_lameJuis;
    rig.RunFrames(2);
    requested.m_accumulatorInput[0].m_interval = LameJuisInternal::Accumulator::Interval::MajorThird;
    requested.m_operationInput[0].m_elements[0] = LameJuisInternal::MatrixSwitch::Inverted;
    requested.m_operationInput[0].m_rhs[1] = false;
    requested.m_operationInput[0].m_switch = LameJuisInternal::LogicOperation::SwitchVal::Middle;
    requested.m_laneInput[0].m_coMuteInput.m_coMutes[0] = true;
    requested.m_laneInput[0].m_chooserInput.m_strategy = HarmonicSheaf::SectionChoiceStrategy::Lowest;
    for (auto& element : requested.m_operationInput[1].m_elements)
    {
        element = LameJuisInternal::MatrixSwitch::Muted;
    }

    requested.m_operationInput[1].m_rhs[0] = true;
    requested.m_operationInput[1].m_switch = LameJuisInternal::LogicOperation::SwitchVal::Down;
    std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());
    rig.ResetToDefaults();
    rig.RunFrames(2);
    internal.m_messageOutBuffer.Clear();

    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    DOCTEST_CHECK(actual.m_accumulators[0].m_interval == LameJuisInternal::Accumulator::Interval::MajorThird);
    DOCTEST_CHECK(actual.m_operations[0].m_elements[0] == LameJuisInternal::MatrixSwitch::Inverted);
    DOCTEST_CHECK_FALSE(actual.m_operations[0].m_rhs[1]);
    DOCTEST_CHECK(actual.m_operations[0].m_switch == LameJuisInternal::LogicOperation::SwitchVal::Middle);
    DOCTEST_CHECK(actual.m_lanes[0].m_coMuteState.m_coMutes[0]);
    DOCTEST_CHECK(actual.m_lanes[0].m_strategy == HarmonicSheaf::SectionChoiceStrategy::Lowest);
    DOCTEST_CHECK(actual.m_operations[1].m_rhs[0]);
    DOCTEST_CHECK(actual.m_operations[1].m_switch == LameJuisInternal::LogicOperation::SwitchVal::Down);
    rig.RunFrames(2);
    DOCTEST_CHECK(actual.m_lanes[0].m_coMuteState.m_coMutes[0]);
    DOCTEST_CHECK_FALSE(rig.IsSequencerRunning());
    DOCTEST_CHECK(rig.GlobalPhase() == 0.0);
    DOCTEST_CHECK(internal.m_messageOutBuffer.m_numMessages == 0);
    for (bool gate : internal.m_nonagon.m_nonagon.m_output.m_gate)
    {
        DOCTEST_CHECK_FALSE(gate);
    }
}

DOCTEST_TEST_CASE("Patch startup: running load preserves tick-gated harmonic changes")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& requested = internal.m_nonagon.m_state.m_lameJuisInput;
    auto& actual = internal.m_nonagon.m_nonagon.m_lameJuis;
    requested.m_laneInput[0].m_coMuteInput.m_coMutes[5] = true;
    requested.m_operationInput[5].m_elements[5] = LameJuisInternal::MatrixSwitch::Inverted;
    std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());
    rig.ResetToDefaults();
    rig.StartSequencer();
    rig.RunFrames(2);
    DOCTEST_REQUIRE_FALSE(actual.m_lanes[0].m_coMuteState.m_coMutes[5]);
    DOCTEST_REQUIRE(actual.m_operations[5].m_elements[5] == LameJuisInternal::MatrixSwitch::Normal);

    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    DOCTEST_CHECK(rig.IsSequencerRunning());
    DOCTEST_CHECK_FALSE(actual.m_lanes[0].m_coMuteState.m_coMutes[5]);
    DOCTEST_CHECK(actual.m_operations[5].m_elements[5] == LameJuisInternal::MatrixSwitch::Normal);
}

DOCTEST_TEST_CASE("Patch startup: stopped edits take effect through regular processing")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& requested = internal.m_nonagon.m_state.m_lameJuisInput;
    auto& actual = internal.m_nonagon.m_nonagon.m_lameJuis;
    rig.RunFrames(2);
    internal.m_messageOutBuffer.Clear();
    requested.m_operationInput[0].m_elements[0] = LameJuisInternal::MatrixSwitch::Inverted;
    requested.m_laneInput[0].m_coMuteInput.m_coMutes[0] = true;
    requested.m_accumulatorInput[0].m_interval = LameJuisInternal::Accumulator::Interval::MajorThird;
    for (auto& element : requested.m_operationInput[1].m_elements)
    {
        element = LameJuisInternal::MatrixSwitch::Muted;
    }

    requested.m_operationInput[1].m_rhs[0] = true;
    requested.m_operationInput[1].m_switch = LameJuisInternal::LogicOperation::SwitchVal::Down;
    rig.RunFrames(2);
    DOCTEST_CHECK(actual.m_operations[0].m_elements[0] == LameJuisInternal::MatrixSwitch::Inverted);
    DOCTEST_CHECK(actual.m_lanes[0].m_coMuteState.m_coMutes[0]);
    DOCTEST_CHECK(actual.m_accumulators[0].m_interval == LameJuisInternal::Accumulator::Interval::MajorThird);
    DOCTEST_CHECK(actual.m_operations[1].m_rhs[0]);
    DOCTEST_CHECK(actual.m_operations[1].m_switch == LameJuisInternal::LogicOperation::SwitchVal::Down);
    DOCTEST_CHECK_FALSE(rig.IsSequencerRunning());
    DOCTEST_CHECK(rig.GlobalPhase() == 0.0);
    DOCTEST_CHECK(internal.m_messageOutBuffer.m_numMessages == 0);
    for (bool gate : internal.m_nonagon.m_nonagon.m_output.m_gate)
    {
        DOCTEST_CHECK_FALSE(gate);
    }
}

DOCTEST_TEST_CASE("Patch startup: restored scene selects its Nonagon values immediately")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    State* interval = internal.m_nonagon.m_stateSaver.Get("LameJuisInterval", 0);
    DOCTEST_REQUIRE(interval != nullptr);
    interval->Set(LameJuisInternal::Accumulator::Interval::MajorThird);
    interval->CopyToScene(3);
    interval->Set(LameJuisInternal::Accumulator::Interval::PerfectFifth);
    internal.SetLeftScene(3);
    JSON patch = rig.SavePatchJSON();
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    internal.SetLeftScene(0);
    rig.RunFrames(2);
    internal.FromJSON(patch, true);
    DOCTEST_CHECK(internal.m_context.m_sceneManager.m_scene1 == 3);
    DOCTEST_CHECK(interval->Get<LameJuisInternal::Accumulator::Interval>() == LameJuisInternal::Accumulator::Interval::MajorThird);
    rig.RunFrames(2);
    DOCTEST_CHECK(internal.m_nonagon.m_nonagon.m_lameJuis.m_accumulators[0].m_interval == LameJuisInternal::Accumulator::Interval::MajorThird);
    DOCTEST_CHECK(interval->Get<LameJuisInternal::Accumulator::Interval>() == LameJuisInternal::Accumulator::Interval::MajorThird);
}

DOCTEST_TEST_CASE("Patch startup: visualizer selectors are valid before audio publication")
{
    using UIState = SquiggleBoyWithEncoderBank::UIState;
    void* storage = ::operator new(sizeof(UIState));
    std::memset(storage, 0xA5, sizeof(UIState));
    UIState* state = new (storage) UIState;
    DOCTEST_CHECK(state->m_activeTrack.load() < TheNonagonInternal::x_numTrios);
    DOCTEST_CHECK(state->m_visualDisplayMode.load() == UIState::VisualDisplayMode::Voice);
    state->~UIState();
    ::operator delete(storage);
}
