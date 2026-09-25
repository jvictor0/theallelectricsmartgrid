#include "doctest.h"

#include "SampleTop.hpp"
#include "TheoryOfTimeBase.hpp"
#include "PolyXFader.hpp"
#include "VectorPhaseShaper.hpp"

DOCTEST_TEST_CASE("SampleTop: VPS stores base-rate offsets even when processed at four times the rate")
{
    AdaptiveWaveTable table;
    for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
    {
        table.m_waveTable.m_table[i] = 0.25f;
    }

    table.Generate();
    VectorPhaseShaperInternal vps;
    vps.m_morphingWaveTable.SetLeft(&table);
    vps.m_morphingWaveTable.SetRight(&table);
    vps.m_phase = 0.9921875f;
    VectorPhaseShaperInternal::Input input;
    input.m_useVoct = false;
    input.m_freq = 0.015625f;
    vps.Process(input, 1.0 / 192000.0);
    DOCTEST_CHECK(vps.m_top.m_triggered);
    DOCTEST_CHECK(vps.m_top.m_offset == doctest::Approx(-0.125));
}

DOCTEST_TEST_CASE("SampleTop: interpolates the latest crossed boundary in either direction")
{
    struct Case
    {
        double m_previous;
        double m_current;
        bool m_triggered;
        double m_offset;
    };

    constexpr Case x_cases[] =
    {
        {0.875, 1.125, true, -0.5},
        {0.875, 1.375, true, -0.75},
        {1.125, 0.875, true, -0.5},
        {-0.125, 0.125, true, -0.5},
        {-0.875, -1.125, true, -0.5},
        {0.125, 2.125, true, -0.0625},
        {2.125, 0.125, true, -0.4375},
        {2.5, 1.0, true, -2.0 / 3.0},
        {1.0, 0.75, true, -1.0},
        {0.75, 1.0, true, 0.0},
        {1.25, 1.0, false, 0.0},
        {1.0, 1.0, false, 0.0},
    };

    for (const Case& item : x_cases)
    {
        DOCTEST_CAPTURE(item.m_previous);
        DOCTEST_CAPTURE(item.m_current);
        SampleTop top = SampleTop::FromPhases(item.m_previous, item.m_current);
        DOCTEST_CHECK(top.m_triggered == item.m_triggered);
        DOCTEST_CHECK(top.m_offset == doctest::Approx(item.m_offset));
    }
}

DOCTEST_TEST_CASE("SampleTop: VPS publishes the fractional crossing on its existing top")
{
    VectorPhaseShaperInternal vps;
    vps.m_phase = 0.9765625f;
    vps.m_freq = 0.015625f;
    vps.UpdatePhase(1.0 / SampleTimer::x_sampleRate);
    DOCTEST_CHECK_FALSE(vps.m_top.m_triggered);
    vps.UpdatePhase(1.0 / SampleTimer::x_sampleRate);
    DOCTEST_CHECK(vps.m_top.m_triggered);
    DOCTEST_CHECK(vps.m_top.m_offset == doctest::Approx(-0.5));
    vps.UpdatePhase(1.0 / SampleTimer::x_sampleRate);
    DOCTEST_CHECK_FALSE(vps.m_top.m_triggered);
    DOCTEST_CHECK(vps.m_top.m_offset == 0.0);
}

DOCTEST_TEST_CASE("SampleTop: Theory of Time retains both domains through rollover and stop")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    input.m_unmodulatedPhase = 0.875;
    time.Process(7, input);
    DOCTEST_CHECK(time.CrossedCycleBoundary(5, 7, PhaseDomain::Modulated).m_offset == 0.0);
    input.m_unmodulatedPhase = 1.125;
    input.m_phaseOffset = -0.25;
    time.Process(8, input);

    time.RolloverMicroblockBuffer();
    SampleTop unmodulated = time.CrossedCycleBoundary(5, 0, PhaseDomain::Unmodulated);
    SampleTop modulated = time.CrossedCycleBoundary(5, 0, PhaseDomain::Modulated);
    DOCTEST_CHECK(unmodulated.m_triggered);
    DOCTEST_CHECK(unmodulated.m_offset == doctest::Approx(-0.5));
    DOCTEST_CHECK_FALSE(modulated.m_triggered);
    input.m_running = false;
    time.Process(1, input);
    SampleTop stopped = time.CrossedCycleBoundary(5, 1, PhaseDomain::Unmodulated);
    DOCTEST_CHECK_FALSE(stopped.m_triggered);
    DOCTEST_CHECK(stopped.m_offset == 0.0);
}

DOCTEST_TEST_CASE("SampleTop: topology changes retain the crossing computed before the edit")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    input.m_input[4].m_parentMult = 2;
    input.m_unmodulatedPhase = 1.125;
    time.Process(1, input);
    input.m_input[4].m_parentMult = 3;
    input.m_unmodulatedPhase = 2.625;
    time.Process(2, input);

    DOCTEST_REQUIRE(time.GetCycleRatio(4, 2) == 3);
    SampleTop top = time.CrossedCycleBoundary(4, 2, PhaseDomain::Modulated);
    DOCTEST_CHECK(top.m_triggered);
    DOCTEST_CHECK(top.m_offset == doctest::Approx(-1.0 / 12.0));
}

DOCTEST_TEST_CASE("SampleTop: PolyXFader retains the latest top when all active loops cross")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_input[0].m_parentIndex = 5;
    clock.m_input[0].m_parentMult = 2;
    clock.m_input[1].m_parentIndex = 5;
    clock.m_input[1].m_parentMult = 3;
    clock.m_unmodulatedPhase = 0.1;
    time.Process(1, clock);
    clock.m_unmodulatedPhase = 0.8;
    time.Process(2, clock);

    PolyXFaderInternal lfo;
    PolyXFaderInternal::Input input;
    input.m_theoryOfTime = &time;
    input.m_size = 2;
    input.m_center = 0.5f;
    input.m_slope = 0.5f;
    input.m_samplePosition = 2.0f;
    lfo.Process(input);
    DOCTEST_CHECK(lfo.m_top.m_triggered);
    DOCTEST_CHECK(lfo.m_top.m_offset == doctest::Approx(-4.0 / 21.0));

    clock.m_unmodulatedPhase = 0.85;
    time.Process(3, clock);
    input.m_size = 2;
    input.m_samplePosition = 3.0f;
    lfo.Process(input);
    DOCTEST_CHECK_FALSE(lfo.m_top.m_triggered);
    DOCTEST_CHECK(lfo.m_top.m_offset == 0.0);
}
