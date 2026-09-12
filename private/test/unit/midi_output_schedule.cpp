#include "doctest.h"
#include "MidiOutputSchedule.hpp"
#include <thread>

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

DOCTEST_TEST_CASE("MIDI native deadline is scoped and does not leak to other threads")
{
    using Schedule = SmartGrid::MidiOutputSchedule;
    DOCTEST_CHECK(Schedule::Deadline() == 0);
    {
        Schedule::Scope outer(1000);
        DOCTEST_CHECK(Schedule::Timestamp(123) == 1000);
        {
            Schedule::Scope inner(2000);
            DOCTEST_CHECK(Schedule::Deadline() == 2000);
        }

        DOCTEST_CHECK(Schedule::Deadline() == 1000);
        std::uint64_t other = 999;
        std::thread thread([&] { other = Schedule::Deadline(); });
        thread.join();
        DOCTEST_CHECK(other == 0);
        DOCTEST_CHECK_FALSE(Schedule::DropLate(999));
        DOCTEST_CHECK(Schedule::DropLate(1000));
    }

    DOCTEST_CHECK(Schedule::Timestamp(123) == 123);
    DOCTEST_CHECK_FALSE(Schedule::DropLate(1000));
}
