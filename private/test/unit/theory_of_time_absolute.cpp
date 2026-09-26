#include "doctest.h"
#include "TheoryOfTime.hpp"

namespace
{
struct AbsoluteClockRig
{
    TheoryOfTimeBase m_time;
    TheoryOfTimeBase::Input m_input;
    size_t m_sampleIndex = 0;

    AbsoluteClockRig()
    {
        m_input.m_running = true;
        for (int i = 0; i < 5; ++i)
        {
            m_input.m_input[i].m_parentIndex = 5;
            m_input.m_input[i].m_parentMult = 1;
        }
    }

    void Step(double phase)
    {
        if (m_sampleIndex == 8)
        {
            m_time.RolloverMicroblockBuffer();
            m_sampleIndex = 0;
        }

        ++m_sampleIndex;
        m_input.m_unmodulatedPhase = phase;
        m_time.Process(m_sampleIndex, m_input);
    }
};
}

DOCTEST_TEST_CASE("AbsoluteTime: loop phase retains whole cycles")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(2.25);
    DOCTEST_CHECK(rig.m_time.GetPhase(4, rig.m_sampleIndex, PhaseDomain::Modulated) == doctest::Approx(6.75));
}

DOCTEST_TEST_CASE("AbsoluteTime: reset indices use signed absolute time")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(2.25);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex) == 6);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == 0);
    rig.Step(-0.25);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex) == -1);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == 2);
}

DOCTEST_TEST_CASE("AbsoluteTime: reparent waits for both parent boundaries")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 2;
    rig.m_input.m_input[3].m_parentIndex = 4;
    rig.Step(0.49);
    rig.m_input.m_input[3].m_parentIndex = 5;
    rig.m_input.m_input[3].m_parentMult = 3;
    rig.Step(0.51);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, rig.m_sampleIndex).m_input.m_parentIndex == 4);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, rig.m_sampleIndex).m_input.m_parentMult == 1);
    rig.Step(0.99);
    rig.Step(1.01);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, rig.m_sampleIndex).m_input.m_parentIndex == 5);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, rig.m_sampleIndex).m_input.m_parentMult == 3);
}

DOCTEST_TEST_CASE("AbsoluteTime: ancestor and descendant edits share pre-edit boundaries")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 2;
    rig.m_input.m_input[3].m_parentIndex = 4;
    rig.m_input.m_input[3].m_parentMult = 3;
    rig.m_input.m_input[2].m_parentIndex = 3;
    rig.Step(0.49);

    rig.m_input.m_input[3].m_parentMult = 5;
    rig.m_input.m_input[2].m_parentIndex = 4;
    rig.m_input.m_input[2].m_parentMult = 7;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(0.51);

    DOCTEST_CHECK(rig.m_time.GetCycleRatio(4, 2) == 2);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(3, 2) == 10);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(2, 2) == 14);
    DOCTEST_CHECK(rig.m_time.GetLoop(2, 2).m_input.m_parentIndex == 4);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));

    rig.Step(0.99);
    rig.Step(1.01);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(4, 4) == 3);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(3, 4) == 15);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(2, 4) == 21);
}

DOCTEST_TEST_CASE("AbsoluteTime: interpolation uses global phase and interval topology")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 2;
    rig.Step(9.99);
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(10.01);
    DOCTEST_CHECK(rig.m_time.GetPhase(4, 1.5, PhaseDomain::Modulated) == doctest::Approx(20.0));
    DOCTEST_CHECK(rig.m_time.GetPhase(4, 2.0, PhaseDomain::Modulated) == doctest::Approx(30.03));
}

DOCTEST_TEST_CASE("AbsoluteTime: crossing events survive multiple complete cycles")
{
    AbsoluteClockRig rig;
    rig.Step(0.125);
    rig.Step(2.125);
    const TimeLoop& loop = rig.m_time.GetLoop(5, rig.m_sampleIndex);
    DOCTEST_CHECK(loop.m_gate);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, rig.m_sampleIndex, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, rig.m_sampleIndex) == 2);
    rig.Step(2.125);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, rig.m_sampleIndex, PhaseDomain::Modulated));
}

DOCTEST_TEST_CASE("AbsoluteTime: lookahead interpolation and rollover retain absolute time")
{
    AbsoluteClockRig rig;
    for (int i = 1; i <= 8; ++i)
    {
        rig.Step(2.0 + 0.125 * i);
    }

    DOCTEST_CHECK(rig.m_time.GetPhase(5, 7.5, PhaseDomain::Unmodulated) == doctest::Approx(2.9375));
    rig.Step(3.125);
    DOCTEST_CHECK(rig.m_time.GetPhase(5, 0.0, PhaseDomain::Unmodulated) == doctest::Approx(3.0));
    DOCTEST_CHECK(rig.m_time.GetPhase(5, 0.5, PhaseDomain::Unmodulated) == doctest::Approx(3.0625));
}

DOCTEST_TEST_CASE("AbsoluteTime: domain separation and unrelated reset")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.m_input.m_phaseOffset = -1.5;
    rig.Step(2.25);
    DOCTEST_CHECK(rig.m_time.GetPhase(4, 1, PhaseDomain::Unmodulated) == doctest::Approx(6.75));
    DOCTEST_CHECK(rig.m_time.GetPhase(4, 1, PhaseDomain::Modulated) == doctest::Approx(2.25));
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, 1, 3) == 2);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, 1, 4) == 0);
}

DOCTEST_TEST_CASE("AbsoluteTime: stopped topology and startup are explicit")
{
    AbsoluteClockRig rig;
    rig.m_input.m_running = false;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(7.0);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 1) == 3);
    DOCTEST_CHECK(rig.m_time.GetPhase(5, 1, PhaseDomain::Modulated) == 0.0);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(4, 1).m_gate);
    rig.m_input.m_running = true;
    rig.Step(0.01);
    DOCTEST_CHECK(rig.m_time.GetLoop(4, 2).m_gate);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(4, 2, PhaseDomain::Modulated));
    rig.m_input.m_running = false;
    rig.Step(0.0);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 3) == 3);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(4, 3).m_gate);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(4, 3, PhaseDomain::Modulated));
}

DOCTEST_TEST_CASE("AbsoluteTime: reverse reparent waits for both boundaries")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 2;
    rig.m_input.m_input[3].m_parentIndex = 4;
    rig.Step(1.51);
    rig.m_input.m_input[3].m_parentIndex = 5;
    rig.m_input.m_input[3].m_parentMult = 3;
    rig.Step(1.49);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, 2).m_input.m_parentIndex == 4);
    rig.Step(1.01);
    rig.Step(0.99);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, 4).m_input.m_parentIndex == 5);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(3, 4) == 3);
}

DOCTEST_TEST_CASE("AbsoluteTime: lattice edits preserve unrelated loop events")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 2;
    rig.m_input.m_input[3].m_parentIndex = 4;
    rig.Step(10.49);
    rig.m_input.m_input[3].m_parentMult = 3;
    rig.Step(10.51);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 2) == 6);
    DOCTEST_CHECK(rig.m_time.GetPosition(2, PhaseDomain::Modulated) == 63);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2) == 10);
    rig.Step(10.52);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(3, 3, PhaseDomain::Modulated));
}

DOCTEST_TEST_CASE("AbsoluteTime: indices exceed 32 bits without winding history")
{
    AbsoluteClockRig rig;
    rig.Step(2147483649.5);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 1) == 2147483649LL);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 1, 5) == 0);
    rig.Step(-2147483649.5);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2) == -2147483650LL);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2, 5) == 0);
}

DOCTEST_TEST_CASE("AbsoluteTime: internal oscillator advances across global cycles")
{
    TheoryOfTime time;
    TheoryOfTime::Input input;
    SmartGrid::MessageOutBuffer messages;
    time.SetupMessageOutBuffer(&messages);
    input.m_running = true;
    input.m_freq = 0.125;
    input.m_modIndex.m_expParam = 0.0;
    int starts = 0;
    int clocks = 0;
    for (size_t block = 0; block < 3; ++block)
    {
        time.RolloverMicroblockBuffer();
        for (size_t j = 1; j <= 8; ++j)
        {
            time.Process(j, input);
            for (const auto& message : messages)
            {
                starts += message.m_mode == SmartGrid::MessageOut::Mode::Start;
                clocks += message.m_mode == SmartGrid::MessageOut::Mode::Clock;
            }

            messages.Clear();
        }
    }

    DOCTEST_CHECK(input.m_unmodulatedPhase == doctest::Approx(3.0));
    DOCTEST_CHECK(time.GetPhase(5, 8, PhaseDomain::Unmodulated) == doctest::Approx(3.0));
    DOCTEST_CHECK(time.GetPhase(5, 8, PhaseDomain::Modulated) == doctest::Approx(3.0));
    DOCTEST_CHECK(starts == 1);
    DOCTEST_CHECK(clocks > 0);
    time.RolloverMicroblockBuffer();
    input.m_running = false;
    time.Process(1, input);
    DOCTEST_REQUIRE(messages.m_numMessages == 1);
    DOCTEST_CHECK(messages.m_messages[0].m_mode == SmartGrid::MessageOut::Mode::Stop);
    DOCTEST_CHECK(time.GetPhase(5, 1, PhaseDomain::Unmodulated) == 0.0);
}

DOCTEST_TEST_CASE("AbsoluteTime: a stopped block clears its rolled over first sample")
{
    AbsoluteClockRig rig;
    for (size_t j = 0; j < 8; ++j)
    {
        rig.Step(100.125 + 0.125 * j);
    }

    rig.m_input.m_running = false;
    rig.m_input.m_input[4].m_parentMult = 3;
    for (size_t j = 0; j < 8; ++j)
    {
        rig.Step(0.0);
    }

    DOCTEST_CHECK_FALSE(rig.m_time.m_samples[0].m_running);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 0).m_gate);
    DOCTEST_CHECK(rig.m_time.GetPhase(5, 0, PhaseDomain::Modulated) == 0.0);
    DOCTEST_CHECK(rig.m_time.GetPhase(5, 0.5, PhaseDomain::Modulated) == 0.0);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(4, 0) == 3);
    DOCTEST_CHECK(rig.m_time.AnyChangeInMicroBlock());
    for (size_t j = 0; j < 8; ++j)
    {
        rig.Step(0.0);
    }

    DOCTEST_CHECK_FALSE(rig.m_time.AnyChangeInMicroBlock());
}

DOCTEST_TEST_CASE("WholeTick: undoubled odd lattice advances gates once per full cycle")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(0.1);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 1) == 3);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(4, 1) == 1);
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 1).m_gate);
    rig.Step(0.5);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 2).m_gate);
    rig.Step(1.0);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 3).m_gate);
    rig.Step(2.0);
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 4).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: reset wraps after every complete ancestor cycle")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.m_input.m_rhythm[4].m_resetLoopIndex = 5;
    const double phases[] = {0.1, 0.4, 0.8, 1.1, 1.4, 1.8};
    const int64_t indices[] = {0, 1, 2, 0, 1, 2};
    const bool gates[] = {true, false, true, true, false, true};
    for (size_t i = 0; i < 6; ++i)
    {
        rig.Step(phases[i]);
        DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == indices[i]);
        DOCTEST_CHECK(rig.m_time.GetLoop(4, rig.m_sampleIndex).m_gate == gates[i]);
        DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(4, rig.m_sampleIndex, PhaseDomain::Modulated));
    }
}

DOCTEST_TEST_CASE("WholeTick: equal neighboring gates and self reset still tick")
{
    for (int reset : {-1, 5})
    {
        AbsoluteClockRig rig;
        rig.m_input.m_rhythm[5].m_gate[1] = true;
        rig.m_input.m_rhythm[5].m_resetLoopIndex = reset;
        rig.Step(0.1);
        rig.Step(1.1);
        DOCTEST_CHECK(rig.m_time.GetLoop(5, 2).m_gate);
        DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
        DOCTEST_CHECK(rig.m_time.AnyTick(5));
        DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2, reset) == (reset == -1 ? 1 : 0));
    }
}

DOCTEST_TEST_CASE("WholeTick: gate edits wait for their own tick while faster loops tick")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 4;
    rig.m_input.m_rhythm[5].m_size = 1;
    rig.Step(0.1);
    rig.m_input.m_rhythm[5].m_gate[0] = false;
    rig.Step(0.1);
    DOCTEST_CHECK_FALSE(rig.m_time.m_samples[2].m_anyChange);
    rig.Step(0.25);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(4, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 3).m_gate);
    rig.Step(0.5);
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 4).m_gate);
    rig.Step(1.0);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 5).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: length edits wait for their own tick")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 4;
    auto& rhythm = rig.m_input.m_rhythm[5];
    rhythm.m_size = 3;
    rhythm.m_gate[0] = false;
    rhythm.m_gate[2] = true;
    rig.Step(2.1);
    DOCTEST_REQUIRE(rig.m_time.GetLoop(5, 1).m_gate);
    rhythm.m_size = 2;
    rig.Step(2.1);
    DOCTEST_CHECK_FALSE(rig.m_time.m_samples[2].m_anyChange);
    rig.Step(2.25);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(4, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 3).m_gate);
    rig.Step(3.0);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 4).m_gate);
    rig.Step(5.0);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 5).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: reset edits wait for their own tick")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[3].m_parentMult = 3;
    rig.m_input.m_input[4].m_parentMult = 12;
    rig.Step(1.05);
    DOCTEST_REQUIRE_FALSE(rig.m_time.GetLoop(3, 1).m_gate);
    rig.m_input.m_rhythm[3].m_resetLoopIndex = 5;
    rig.Step(1.05);
    DOCTEST_CHECK_FALSE(rig.m_time.m_samples[2].m_anyChange);
    rig.Step(1.1);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(4, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(3, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(3, 3).m_gate);
    rig.Step(1.34);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(3, 4, PhaseDomain::Modulated));
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(3, 4).m_gate);
    rig.Step(1.67);
    DOCTEST_CHECK(rig.m_time.GetLoop(3, 5).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: accepted topology remaps gates without accepting unrelated edits")
{
    AbsoluteClockRig rig;
    rig.m_input.m_input[4].m_parentMult = 2;
    rig.m_input.m_input[3].m_parentIndex = 4;
    rig.m_input.m_rhythm[3].m_size = 5;
    rig.m_input.m_rhythm[3].m_gate[0] = false;
    rig.m_input.m_rhythm[3].m_gate[3] = true;
    rig.m_input.m_rhythm[5].m_size = 1;
    rig.Step(10.49);
    DOCTEST_REQUIRE_FALSE(rig.m_time.GetLoop(3, 1).m_gate);
    rig.m_input.m_rhythm[5].m_gate[0] = false;
    rig.m_input.m_input[3].m_parentMult = 3;
    rig.Step(10.51);
    DOCTEST_CHECK(rig.m_time.GetCycleRatio(3, 2) == 6);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(3, 2, -1) == 63);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(3, 2, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetLoop(3, 2).m_gate);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 2).m_gate);
    rig.Step(11.0);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 3).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: rhythm sampling follows modulated rather than unmodulated crossings")
{
    AbsoluteClockRig rig;
    rig.m_input.m_rhythm[5].m_size = 1;
    rig.Step(0.9);
    rig.m_input.m_rhythm[5].m_gate[0] = false;
    rig.m_input.m_phaseOffset = -0.5;
    rig.Step(1.4);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Unmodulated));
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 2).m_gate);
    rig.Step(1.6);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, 3, PhaseDomain::Modulated));
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 3).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: reverse crossings use floor division and hold within a cycle")
{
    AbsoluteClockRig rig;
    rig.Step(1.1);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 1).m_gate);
    rig.Step(0.9);
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 2).m_gate);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
    rig.Step(-0.1);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 3).m_gate);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, 3, PhaseDomain::Modulated));
    rig.Step(-0.5);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 4, PhaseDomain::Modulated));
    rig.Step(-1.1);
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 5).m_gate);
}

DOCTEST_TEST_CASE("WholeTick: slot eight tick and gate enter the next microblock together")
{
    AbsoluteClockRig rig;
    for (size_t j = 0; j < 15; ++j)
    {
        rig.Step(0.1);
    }

    DOCTEST_REQUIRE_FALSE(rig.m_time.AnyTick(5));
    rig.Step(1.1);
    DOCTEST_CHECK_FALSE(rig.m_time.AnyTick(5));
    DOCTEST_CHECK(rig.m_time.GetLoop(5, 0).m_gate);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 8).m_gate);
    rig.Step(1.1);
    DOCTEST_CHECK(rig.m_time.AnyTick(5));
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, 0).m_gate);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 1, PhaseDomain::Modulated));
}

DOCTEST_TEST_CASE("WholeTick: nonbinary rhythms retain signed 64 bit indices through lookup")
{
    struct Example
    {
        int64_t m_step;
        int m_size;
        int m_expected;
    };

    const Example examples[] =
    {
        {2147483648LL, 3, 2},
        {-2147483649LL, 3, 0},
        {4294967297LL, 5, 2},
        {-4294967297LL, 5, 3},
        {2147483648LL, 7, 2},
        {-2147483649LL, 7, 4}
    };

    for (const Example& example : examples)
    {
        DOCTEST_INFO("step=", example.m_step, " size=", example.m_size);
        AbsoluteClockRig rig;
        auto& rhythm = rig.m_input.m_rhythm[5];
        rhythm.m_size = example.m_size;
        rhythm.m_gate[0] = false;
        rhythm.m_gate[example.m_expected] = true;
        DOCTEST_CHECK(rhythm.MonodromyIndexToIndex(example.m_step) == example.m_expected);
        DOCTEST_CHECK(rhythm.Gate(example.m_step));
        rig.Step(static_cast<double>(example.m_step) + 0.25);
        DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 1, -1) == example.m_step);
        DOCTEST_CHECK(rig.m_time.GetLoop(5, 1).m_gate);
    }
}
