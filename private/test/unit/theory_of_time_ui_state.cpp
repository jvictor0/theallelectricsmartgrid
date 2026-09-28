#include "doctest.h"

#include "TheoryOfTimeBaseUIState.hpp"

#include <array>

namespace
{
TheoryOfTimeBase::Input FlatSnapshotClock()
{
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    for (size_t loop = 0; loop < TheoryOfTimeBase::x_globalLoop; ++loop)
    {
        input.m_input[loop].m_parentIndex = 5;
        input.m_input[loop].m_parentMult = 1;
    }

    return input;
}

void PrepareSnapshotClock(TheoryOfTimeBase& time, const TheoryOfTimeBase::Input& input)
{
    for (size_t sample = 1; sample <= TheoryOfTimeBase::x_microBlockSize; ++sample)
    {
        time.Process(sample, input);
    }

    time.RolloverMicroblockBuffer();
}
}

DOCTEST_TEST_CASE("SequencerUI: loop rhythm snapshots retain all sixteen slots and signed lookup")
{
    TheoryOfTimeRhythm rhythm;
    rhythm.m_size = 16;
    rhythm.m_gate[0] = false;
    rhythm.m_gate[15] = true;
    rhythm.m_resetLoopIndex = 5;
    TheoryOfTimeRhythm::UIState ui;
    rhythm.PopulateUIState(ui);
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GateAt(-1));
    DOCTEST_CHECK(ui.GateAt(15));
    DOCTEST_CHECK_FALSE(ui.GateAt(16));

    rhythm.m_size = 3;
    rhythm.m_resetLoopIndex = -1;
    rhythm.m_gate[1] = true;
    rhythm.m_gate[15] = false;
    rhythm.PopulateUIState(ui);
    DOCTEST_CHECK(ui.Changed());
    DOCTEST_CHECK(ui.GateAt(15));
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK_FALSE(ui.GateAt(15));
    DOCTEST_CHECK(ui.GateAt(-2));
    DOCTEST_CHECK(ui.GateAt(4294967296LL));
}

DOCTEST_TEST_CASE("SequencerUI: time points reconstruct six rhythm bits at absolute lattice ticks")
{
    struct Example
    {
        int64_t m_tick;
        uint8_t m_slice;
        int64_t m_resetPosition;
    };

    constexpr std::array<Example, 7> x_examples =
    {{
        {0, 63, 0},
        {1, 62, 1},
        {2, 61, 2},
        {3, 60, 3},
        {4, 3, 0},
        {-1, 0, 3},
        {4294967299LL, 60, 3}
    }};

    TheoryOfTimeBase time;
    auto input = FlatSnapshotClock();
    input.m_input[0].m_parentMult = 4;
    input.m_input[1].m_parentMult = 2;
    PrepareSnapshotClock(time, input);
    TheoryOfTimeBaseUIState ui;
    time.PopulateUIState(ui, input);
    ui.Snapshot();
    DOCTEST_REQUIRE(ui.GetGlobalPeriodTicks() == 4);
    for (const auto& example : x_examples)
    {
        DOCTEST_CAPTURE(example.m_tick);
        auto point = ui.GetTimePoint(example.m_tick, 0, 5);
        DOCTEST_CHECK(point.m_globalTickPosition == example.m_tick);
        DOCTEST_CHECK(point.m_timeSlice.m_bits == example.m_slice);
        DOCTEST_CHECK(point.m_loopCyclePosition == example.m_resetPosition);
        DOCTEST_CHECK(ui.GetTimePoint(example.m_tick, 0, -1).m_loopCyclePosition == example.m_tick);
        DOCTEST_CHECK(ui.GetTimePoint(example.m_tick, -1, -1).m_loopCyclePosition == 0);
    }

    input.m_rhythm[0].m_gate[1] = true;
    time.PopulateUIState(ui, input);
    DOCTEST_CHECK(ui.Changed());
    DOCTEST_CHECK(ui.GetTimePoint(1, 0, 5).m_timeSlice.m_bits == 62);
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GetTimePoint(1, 0, 5).m_timeSlice.m_bits == 63);
}

DOCTEST_TEST_CASE("SequencerUI: reset inference completes the divisibility square in live and snapshot queries")
{
    TheoryOfTimeBase time;
    auto input = FlatSnapshotClock();
    input.m_input[4].m_parentMult = 2;
    input.m_input[3].m_parentMult = 3;
    input.m_input[2].m_parentIndex = 4;
    input.m_input[2].m_parentMult = 3;
    input.m_input[1].m_parentIndex = 4;
    input.m_input[1].m_parentMult = 3;
    input.m_rhythm[2].m_resetLoopIndex = 3;
    input.m_rhythm[2].m_size = 3;
    input.m_unmodulatedPhase = 1.75;
    PrepareSnapshotClock(time, input);
    TheoryOfTimeBaseUIState ui;
    time.PopulateUIState(ui, input);
    ui.Snapshot();

    DOCTEST_REQUIRE(ui.GetGlobalPeriodTicks() == 6);
    DOCTEST_CHECK(time.GetLoopCyclePosition(2, 0, -1) == 10);
    DOCTEST_CHECK(time.GetLoopCyclePosition(2, 0, 3) == 0);
    DOCTEST_CHECK(ui.GetTimePoint(10, 2, 3).m_loopCyclePosition == 0);
    DOCTEST_CHECK(ui.GetTimePoint(-1, 2, 3).m_loopCyclePosition == 1);
    DOCTEST_CHECK(time.GetLoopCyclePosition(2, 0, 1) == 0);
    DOCTEST_CHECK(ui.GetTimePoint(10, 2, 1).m_loopCyclePosition == 0);
    DOCTEST_CHECK(time.GetLoopCyclePosition(3, 0, 4) == 5);
    DOCTEST_CHECK(ui.GetTimePoint(10, 3, 4).m_loopCyclePosition == 5);
    DOCTEST_CHECK(time.GetLoop(2, 0).m_gate);
    DOCTEST_CHECK(ui.GetTimePoint(10, 2, 3).m_timeSlice.Get(2));
}

DOCTEST_TEST_CASE("SequencerUI: pending topology does not replace accepted snapshot periods")
{
    TheoryOfTimeBase time;
    auto input = FlatSnapshotClock();
    input.m_input[0].m_parentMult = 4;
    input.m_unmodulatedPhase = 0.125;
    PrepareSnapshotClock(time, input);
    TheoryOfTimeBaseUIState ui;
    time.PopulateUIState(ui, input);
    ui.Snapshot();
    DOCTEST_REQUIRE(ui.GetGlobalPeriodTicks() == 4);

    input.m_input[0].m_parentMult = 3;
    input.m_unmodulatedPhase = 0.25;
    PrepareSnapshotClock(time, input);
    time.PopulateUIState(ui, input);
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GetGlobalPeriodTicks() == 4);

    input.m_unmodulatedPhase = 1.125;
    PrepareSnapshotClock(time, input);
    time.PopulateUIState(ui, input);
    DOCTEST_CHECK(ui.Changed());
    DOCTEST_CHECK(ui.GetGlobalPeriodTicks() == 4);
    ui.Snapshot();
    DOCTEST_CHECK_FALSE(ui.Changed());
    DOCTEST_CHECK(ui.GetGlobalPeriodTicks() == 3);
}
