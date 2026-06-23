#include "doctest.h"

#include "ClockModeConfigJSON.hpp"
#include "Json.hpp"

DOCTEST_TEST_CASE("Clock mode config JSON writes and reads external clock")
{
    JsonArena arena(4096);
    JSON nonagonConfig = arena.Object();

    ClockModeConfigJSON::WriteExternalClock(nonagonConfig, arena, true);

    DOCTEST_CHECK(nonagonConfig.Get("external_clock").BooleanValue());
    DOCTEST_CHECK(ClockModeConfigJSON::ReadExternalClock(nonagonConfig, false));
}

DOCTEST_TEST_CASE("Clock mode config JSON missing field keeps internal default")
{
    JsonArena arena(4096);
    JSON nonagonConfig = arena.Object();

    DOCTEST_CHECK_FALSE(ClockModeConfigJSON::ReadExternalClock(nonagonConfig, false));
}
