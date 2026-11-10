#include "doctest.h"

#include "SampleTop.hpp"
#include "TheoryOfTimeBase.hpp"
#include "VectorPhaseShaper.hpp"

DOCTEST_TEST_CASE("SampleTop: VPS stores the wrap offset in samples")
{
    AdaptiveWaveTable table;
    for (size_t i = 0; i < BasicWaveTable::x_tableSize; ++i)
    {
        table.m_waveTable.m_table[i] = 0.25f;
    }

    table.Generate();
    VectorPhaseShaperInternal vps;
    vps.SetLeft(&table);
    vps.SetRight(&table);
    vps.m_phase = 0.9921875f;
    VectorPhaseShaperInternal::Input input;
    input.m_freq = 0.015625f;
    vps.Process(input);
    DOCTEST_CHECK(vps.m_top[0].m_triggered);
    DOCTEST_CHECK(vps.m_top[0].m_offset == doctest::Approx(-0.5));
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
    vps.UpdatePhase();
    DOCTEST_CHECK_FALSE(vps.m_top[0].m_triggered);
    vps.UpdatePhase();
    DOCTEST_CHECK(vps.m_top[0].m_triggered);
    DOCTEST_CHECK(vps.m_top[0].m_offset == doctest::Approx(-0.5));
    vps.UpdatePhase();
    DOCTEST_CHECK_FALSE(vps.m_top[0].m_triggered);
    DOCTEST_CHECK(vps.m_top[0].m_offset == 0.0);
}

DOCTEST_TEST_CASE("Time loop crossings: Theory of Time retains both domains and clears them on stop")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    input.m_unmodulatedPhase = 0.875;
    time.Process(input);
    DOCTEST_CHECK(time.CrossedCycleBoundary(5, PhaseDomain::Modulated));
    input.m_unmodulatedPhase = 1.125;
    input.m_phaseOffset = -0.25;
    time.Process(input);
    bool unmodulated = time.CrossedCycleBoundary(5, PhaseDomain::Unmodulated);
    bool modulated = time.CrossedCycleBoundary(5, PhaseDomain::Modulated);
    DOCTEST_CHECK(unmodulated);
    DOCTEST_CHECK_FALSE(modulated);
    input.m_running = false;
    time.Process(input);
    bool stopped = time.CrossedCycleBoundary(5, PhaseDomain::Unmodulated);
    DOCTEST_CHECK_FALSE(stopped);
}

DOCTEST_TEST_CASE("Time loop crossings: topology changes retain the crossing computed before the edit")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input input;
    input.m_running = true;
    input.m_input[4].m_parentMult = 2;
    input.m_unmodulatedPhase = 1.125;
    time.Process(input);
    input.m_input[4].m_parentMult = 3;
    input.m_unmodulatedPhase = 2.625;
    time.Process(input);

    DOCTEST_REQUIRE(time.GetCycleRatio(4) == 3);
    bool top = time.CrossedCycleBoundary(4, PhaseDomain::Modulated);
    DOCTEST_CHECK(top);
}
