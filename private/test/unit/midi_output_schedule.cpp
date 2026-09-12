#include "doctest.h"
#include "MidiOutputSchedule.hpp"

DOCTEST_TEST_CASE("MIDI schedule converts future microseconds into host ticks")
{
    auto plan = SmartGrid::MidiOutputSchedule::Plan(1000000, 1005000.0, 24000000.0);
    DOCTEST_CHECK_FALSE(plan.m_drop);
    DOCTEST_CHECK(plan.m_hostTicks == 24480000);
    DOCTEST_CHECK(plan.m_leadUs == 15000.0);
}

DOCTEST_TEST_CASE("MIDI schedule discards expired and invalid timestamps without catch-up")
{
    DOCTEST_CHECK(SmartGrid::MidiOutputSchedule::Plan(1000000, 1020000.0, 24000000.0).m_drop);
    DOCTEST_CHECK(SmartGrid::MidiOutputSchedule::Plan(1000000, 1200000.0, 24000000.0).m_drop);
    DOCTEST_CHECK(SmartGrid::MidiOutputSchedule::Plan(UINT64_MAX, 1.0, 24000000.0).m_drop);
    auto immediate = SmartGrid::MidiOutputSchedule::Plan(0, 1200000.0, 24000000.0);
    DOCTEST_CHECK_FALSE(immediate.m_drop);
    DOCTEST_CHECK(immediate.m_hostTicks == 0);
}

DOCTEST_TEST_CASE("MIDI native submission uses the explicit deadline")
{
    using Schedule = SmartGrid::MidiOutputSchedule;
    DOCTEST_CHECK(Schedule::Timestamp(1000, 123) == 1000);
    DOCTEST_CHECK(Schedule::Timestamp(0, 123) == 123);
    DOCTEST_CHECK_FALSE(Schedule::DropLate(1000, 999));
    DOCTEST_CHECK(Schedule::DropLate(1000, 1000));
    DOCTEST_CHECK_FALSE(Schedule::DropLate(0, 1000));
}
