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
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex) == 13);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == 1);
    rig.Step(-0.25);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex) == -2);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, rig.m_sampleIndex, 5) == 4);
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
    DOCTEST_CHECK(loop.m_gateStepChanged);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(5, rig.m_sampleIndex, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, rig.m_sampleIndex) == 4);
    rig.Step(2.125);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(5, rig.m_sampleIndex).m_gateStepChanged);
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
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, 1, 3) == 4);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(4, 1, 4) == 0);
}

DOCTEST_TEST_CASE("AbsoluteTime: stopped topology and startup are explicit")
{
    AbsoluteClockRig rig;
    rig.m_input.m_running = false;
    rig.m_input.m_input[4].m_parentMult = 3;
    rig.Step(7.0);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 1) == 6);
    DOCTEST_CHECK(rig.m_time.GetPhase(5, 1, PhaseDomain::Modulated) == 0.0);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(4, 1).m_gate);
    rig.m_input.m_running = true;
    rig.Step(0.01);
    DOCTEST_CHECK(rig.m_time.GetLoop(4, 2).m_gateStepChanged);
    DOCTEST_CHECK(rig.m_time.CrossedCycleBoundary(4, 2, PhaseDomain::Modulated));
    rig.m_input.m_running = false;
    rig.Step(0.0);
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 3) == 6);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(4, 3).m_gateStepChanged);
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
    DOCTEST_CHECK(rig.m_time.GetPeriodTicks(5, 2) == 12);
    DOCTEST_CHECK(rig.m_time.GetPosition(2, PhaseDomain::Modulated) == 126);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(5, 2, PhaseDomain::Modulated));
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2) == 21);
    rig.Step(10.52);
    DOCTEST_CHECK_FALSE(rig.m_time.GetLoop(3, 3).m_gateStepChanged);
    DOCTEST_CHECK_FALSE(rig.m_time.CrossedCycleBoundary(3, 3, PhaseDomain::Modulated));
}

DOCTEST_TEST_CASE("AbsoluteTime: indices exceed 32 bits without winding history")
{
    AbsoluteClockRig rig;
    rig.Step(2147483649.5);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 1) == 4294967299LL);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 1, 5) == 1);
    rig.Step(-2147483649.5);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2) == -4294967299LL);
    DOCTEST_CHECK(rig.m_time.GetGateStepIndex(5, 2, 5) == 1);
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
