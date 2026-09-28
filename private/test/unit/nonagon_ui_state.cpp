#include "doctest.h"

#include "TheNonagon.hpp"
#include "../support/GlobalEnv.hpp"

#include <array>
#include <memory>

namespace
{
struct NonagonSnapshotRig
{
    std::unique_ptr<SmartGridOneContext> m_context;
    std::unique_ptr<TheNonagonSmartGrid> m_grid;
    TheNonagonInternal::UIState m_ui;

    NonagonSnapshotRig()
    {
        GlobalEnv::ResetPerTest();
        m_context = std::make_unique<SmartGridOneContext>();
        m_grid = std::make_unique<TheNonagonSmartGrid>(true, m_context.get());
        auto& input = m_grid->m_state;
        auto& nonagon = m_grid->m_nonagon;
        input.m_running = true;
        input.m_theoryOfTimeInput.m_running = true;
        input.m_lameJuisInput.m_accumulatorInput[0].m_interval = LameJuisInternal::Accumulator::Interval::Octave;
        for (size_t lane = 0; lane < LameJuisInternal::x_numLanes; ++lane)
        {
            auto& laneInput = input.m_lameJuisInput.m_laneInput[lane];
            laneInput.m_chooserInput.m_strategy = HarmonicSheaf::SectionChoiceStrategy::Percentile;
            for (bool& coMute : laneInput.m_coMuteInput.m_coMutes)
            {
                coMute = true;
            }
        }

        for (size_t voice = 0; voice < TheNonagonInternal::x_numVoices; ++voice)
        {
            input.m_arpInput.m_zoneHeight[voice] = 1.0f;
            input.m_arpInput.m_zoneOverlap[voice] = 1.0f;
            input.m_arpInput.m_offset[voice] = static_cast<float>(voice) / 16.0f;
            input.m_arpInput.m_interval[voice] = 0.125f;
            input.m_arpInput.m_pageInterval[voice] = 0.125f;
        }

        for (size_t sample = 1; sample <= TheoryOfTimeBase::x_microBlockSize; ++sample)
        {
            nonagon.m_theoryOfTime.TheoryOfTimeBase::Process(sample, input.m_theoryOfTimeInput);
        }

        nonagon.m_theoryOfTime.RolloverMicroblockBuffer();
        nonagon.SetIndexArpInputs(input);
        nonagon.m_indexArp.Process(input.m_arpInput);
        nonagon.SetLameJuisInput(input);
        nonagon.m_lameJuis.Process(input.m_lameJuisInput);
        Publish();
    }

    void Publish()
    {
        m_grid->PopulateUIState(&m_ui);
    }
};
}

DOCTEST_TEST_CASE("SequencerUI: Nonagon publication reproduces all nine raw harmonic selections")
{
    NonagonSnapshotRig rig;
    rig.m_ui.Snapshot();
    constexpr std::array<float, 9> x_pitches = {0, 1, 2, 2, 2, 2, 3, 3, 3};
    for (size_t voice = 0; voice < x_pitches.size(); ++voice)
    {
        DOCTEST_CAPTURE(voice);
        auto point = rig.m_ui.GetVoicePoint(0, voice);
        DOCTEST_CHECK(point.m_globalTickPosition == 0);
        DOCTEST_CHECK(point.m_timeSlice.m_bits == 63);
        DOCTEST_CHECK(point.m_choiceValue == doctest::Approx(static_cast<float>(voice) / 16.0f));
        DOCTEST_CHECK(point.m_pitch.m_value == doctest::Approx(x_pitches[voice]));
        const auto& live = rig.m_grid->m_nonagon.m_lameJuis.m_lanes[voice / 3].m_pitch[voice % 3];
        DOCTEST_CHECK(point.m_pitch == live);
    }
}

DOCTEST_TEST_CASE("SequencerUI: sequence growth is bounded and harmonic edits invalidate every voice")
{
    NonagonSnapshotRig rig;
    rig.m_ui.PreProcess(0);
    for (size_t voice = 0; voice < TheNonagonUIState::x_numVoices; ++voice)
    {
        auto& sequence = rig.m_ui.m_sequences[voice];
        DOCTEST_REQUIRE(sequence.m_points.size() == 1);
        rig.m_ui.Process(0, voice);
        DOCTEST_CHECK(sequence.m_points.size() == 65);
        rig.m_ui.Process(0, voice);
        DOCTEST_REQUIRE(sequence.m_points.size() == 96);
        DOCTEST_CHECK(sequence.StartPosition() == -32);
        DOCTEST_CHECK(sequence.EndPosition() == 64);
        DOCTEST_CHECK(sequence.GetPoint(-1).m_globalTickPosition == -1);
        DOCTEST_CHECK(sequence.GetPoint(63).m_globalTickPosition == 63);
    }

    DOCTEST_REQUIRE(rig.m_ui.GetVoicePoint(0, 8).m_pitch.m_value == doctest::Approx(3.0f));
    auto& input = rig.m_grid->m_state.m_lameJuisInput;
    input.m_accumulatorInput[0].m_interval = LameJuisInternal::Accumulator::Interval::Off;
    rig.m_grid->m_nonagon.m_lameJuis.Process(input);
    rig.Publish();
    DOCTEST_CHECK(rig.m_ui.Changed());
    DOCTEST_CHECK(rig.m_ui.GetVoicePoint(0, 8).m_pitch.m_value == doctest::Approx(3.0f));
    rig.m_ui.PreProcess(0);
    DOCTEST_CHECK_FALSE(rig.m_ui.Changed());
    for (size_t voice = 0; voice < TheNonagonUIState::x_numVoices; ++voice)
    {
        DOCTEST_REQUIRE(rig.m_ui.m_sequences[voice].m_points.size() == 1);
        DOCTEST_CHECK(rig.m_ui.m_sequences[voice].GetPoint(0).m_pitch.m_value == doctest::Approx(0.0f));
    }
}

DOCTEST_TEST_CASE("SequencerUI: desired ranges contain the signed current position at every cache scale")
{
    struct Example
    {
        int64_t m_position;
        int64_t m_period;
        int64_t m_start;
        int64_t m_end;
    };

    constexpr std::array<Example, 6> x_examples =
    {{
        {0, 32, -32, 64},
        {32, 32, 0, 96},
        {-1, 32, -64, 32},
        {100, 512, -256, 768},
        {-1, 1024, -1024, 0},
        {-1, 2048, -513, 511}
    }};

    for (const auto& example : x_examples)
    {
        DOCTEST_CAPTURE(example.m_position);
        DOCTEST_CAPTURE(example.m_period);
        auto range = TheNonagonUIState::Sequence::GetDesiredPositionRange(example.m_position, example.m_period);
        DOCTEST_CHECK(range.first == example.m_start);
        DOCTEST_CHECK(range.second == example.m_end);
        DOCTEST_CHECK(range.first <= example.m_position);
        DOCTEST_CHECK(example.m_position < range.second);
        DOCTEST_CHECK(range.second - range.first <= 1024);
    }
}

DOCTEST_TEST_CASE("SequencerUI: touching half-open cache windows reseed before trimming")
{
    for (int64_t position : {-96, 96})
    {
        NonagonSnapshotRig rig;
        rig.m_ui.PreProcess(0);
        rig.m_ui.Process(0, 0);
        rig.m_ui.Process(0, 0);
        auto& sequence = rig.m_ui.m_sequences[0];
        DOCTEST_REQUIRE(sequence.StartPosition() == -32);
        DOCTEST_REQUIRE(sequence.EndPosition() == 64);
        rig.m_ui.PreProcess(position);
        DOCTEST_REQUIRE_FALSE(sequence.m_points.empty());
        DOCTEST_REQUIRE(sequence.HasPoint(position));
        rig.m_ui.Process(position, 0);
        DOCTEST_CHECK(sequence.HasPoint(position));
    }
}

DOCTEST_TEST_CASE("SequencerUI: a disabled arp clock uses its reset choice despite a leading rest")
{
    NonagonSnapshotRig rig;
    auto& input = rig.m_grid->m_state.m_arpInput;
    input.m_clockSelect[0] = -1;
    input.m_input[0].m_read = true;
    input.m_input[0].m_rhythm[0] = false;
    input.m_offset[0] = 0.125f;
    input.m_interval[0] = 0.25f;
    rig.m_grid->m_nonagon.m_indexArp.Process(input);
    rig.Publish();
    rig.m_ui.Snapshot();
    DOCTEST_REQUIRE(rig.m_grid->m_nonagon.m_indexArp.m_arp[0].m_choiceValue == doctest::Approx(0.125f));
    DOCTEST_CHECK(rig.m_ui.GetVoicePoint(0, 0).m_choiceValue == doctest::Approx(0.125f));
}

DOCTEST_TEST_CASE("SequencerUI: leading rests retain the last reachable note across a clock reset")
{
    NonagonSnapshotRig rig;
    auto& input = rig.m_grid->m_state.m_arpInput;
    input.m_clockSelect[0] = 0;
    input.m_resetSelect[0] = 4;
    input.m_clocks[0] = true;
    input.m_input[0].m_read = true;
    input.m_input[0].m_rhythmLength = 5;
    for (size_t slot = 0; slot < IndexArp::x_rhythmLength; ++slot)
    {
        input.m_input[0].m_rhythm[slot] = slot == 3;
    }

    input.m_offset[0] = 0.0f;
    input.m_interval[0] = 0.25f;
    input.m_pageInterval[0] = 0.125f;
    rig.Publish();
    rig.m_ui.Snapshot();
    constexpr std::array<int64_t, 7> x_positions = {13, 14, 15, 0, 1, 2, 3};
    constexpr std::array<float, 7> x_choices = {0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.0f};
    for (size_t index = 0; index < x_positions.size(); ++index)
    {
        input.m_clockPosition[0] = x_positions[index];
        rig.m_grid->m_nonagon.m_indexArp.Process(input);
        rig.Publish();
        rig.m_ui.Snapshot();
        auto point = rig.m_ui.GetVoicePoint(static_cast<int64_t>(index) + 13, 0);
        DOCTEST_REQUIRE(point.m_loopCyclePosition == x_positions[index]);
        DOCTEST_CHECK(rig.m_grid->m_nonagon.m_indexArp.m_arp[0].m_choiceValue == doctest::Approx(x_choices[index]));
        DOCTEST_CHECK(point.m_choiceValue == doctest::Approx(x_choices[index]));
    }
}
