// WP-1: absolute-set encoder message (MessageIn::Mode::EncoderSet).
//
// Proves end-to-end that an EncoderSet message, pushed through the MessageInBus
// front door and dispatched to TheNonagonSquiggleBoyInternal::Apply, sets the
// targeted encoder to a value in [0,1] deterministically -- bypassing the
// timestamp-dependent acceleration of EncoderIncDec -- and that the result is
// observable in the encoder bank UIState after a process frame.
//
// See SystemFixture.hpp: the whole-system instance is a process-wide singleton
// (the global grid-id allocator does not support repeated construction), so we
// share it with the infra spike and operate on specific, connected cells rather
// than assuming a pristine system.

#include "doctest.h"

#include <cmath>

#include "../support/GlobalEnv.hpp"
#include "../support/SystemFixture.hpp"

#include "MessageIn.hpp"
#include "MessageInBus.hpp"
#include "TheNonagonSquiggleBoy.hpp"

namespace
{

// The encoder bank is a 4x4 grid; channel 0 = (track 0, voice 0).
//
constexpr size_t kChannel = 0;

// Read back the value the encoder bank published to UIState for cell (x,y),
// channel k. This is cell->m_output[k] (the un-slewed computed value), populated
// during ProcessFrame -> PopulateUIState.
//
float ReadEncoderValue(TheNonagonSquiggleBoyInternal& system, int x, int y, size_t k = kChannel)
{
    return system.m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.GetValue(x, y, k);
}

bool IsConnected(TheNonagonSquiggleBoyInternal& system, int x, int y)
{
    return system.m_uiState.m_squiggleBoyUIState.m_encoderBankUIState.GetConnected(x, y);
}

// Find a connected, visible encoder cell in the currently-selected bank by
// scanning UIState after a frame. Returns true and sets (outX,outY) on success.
//
bool FindConnectedCell(TheNonagonSquiggleBoyInternal& system, int& outX, int& outY)
{
    SystemFixture::RunFrame(system);
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (IsConnected(system, x, y))
            {
                outX = x;
                outY = y;
                return true;
            }
        }
    }
    return false;
}

// Push an EncoderSet message through the bus and process a frame so the new
// value propagates to compute + UIState. Uses the Routes::Encoder routeId is not
// needed here: ProcessMessages dispatches straight to internal.Apply, which is
// exactly what the bus does for the internal target.
//
void ApplyAndRun(SmartGrid::MessageInBus& bus,
                 TheNonagonSquiggleBoyInternal& system,
                 SmartGrid::MessageIn msg,
                 size_t timestamp)
{
    bus.Push(msg);
    bus.ProcessMessages(&system, timestamp);
    SystemFixture::RunFrame(system);
}

} // namespace

DOCTEST_TEST_CASE("EncoderSet sets an encoder to a value via the message bus")
{
    GlobalEnv::ResetPerTest();

    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();
    SmartGrid::MessageInBus bus;

    int x = -1;
    int y = -1;
    DOCTEST_REQUIRE(FindConnectedCell(system, x, y));

    // 14-bit quantization (1/16383) plus a small allowance for any residual
    // scene-blend representation error.
    //
    const float tol = 1.0f / 16383.0f + 1e-4f;

    const float values[] = {0.25f, 0.5f, 0.75f, 0.1f};
    size_t timestamp = 1000;
    for (float v : values)
    {
        ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, v), timestamp);
        timestamp += 1000;
        DOCTEST_CHECK(std::abs(ReadEncoderValue(system, x, y) - v) <= tol);
    }
}

DOCTEST_TEST_CASE("EncoderSet clamps out-of-range inputs and hits extremes")
{
    GlobalEnv::ResetPerTest();

    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();
    SmartGrid::MessageInBus bus;

    int x = -1;
    int y = -1;
    DOCTEST_REQUIRE(FindConnectedCell(system, x, y));

    const float tol = 1.0f / 16383.0f + 1e-4f;
    size_t timestamp = 100000;

    // v = 0 extreme.
    //
    ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, 0.0f), timestamp);
    DOCTEST_CHECK(std::abs(ReadEncoderValue(system, x, y) - 0.0f) <= tol);

    // v = 1 extreme.
    //
    timestamp += 1000;
    ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, 1.0f), timestamp);
    DOCTEST_CHECK(std::abs(ReadEncoderValue(system, x, y) - 1.0f) <= tol);

    // Out-of-range below: -0.5 -> 0.
    //
    timestamp += 1000;
    ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, -0.5f), timestamp);
    DOCTEST_CHECK(std::abs(ReadEncoderValue(system, x, y) - 0.0f) <= tol);

    // Out-of-range above: 1.5 -> 1.
    //
    timestamp += 1000;
    ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, 1.5f), timestamp);
    DOCTEST_CHECK(std::abs(ReadEncoderValue(system, x, y) - 1.0f) <= tol);
}

DOCTEST_TEST_CASE("EncoderSet is timestamp-independent (no acceleration)")
{
    GlobalEnv::ResetPerTest();

    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();

    int x = -1;
    int y = -1;
    {
        // Use a throwaway bus just to discover a connected cell via a frame.
        //
        DOCTEST_REQUIRE(FindConnectedCell(system, x, y));
    }

    const float target = 0.6f;

    // Apply the same EncoderSet with a small timestamp.
    //
    {
        SmartGrid::MessageInBus bus;
        ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, target), /*timestamp=*/10);
    }
    float valueSmallTs = ReadEncoderValue(system, x, y);

    // Now move it away, then apply the SAME EncoderSet with a hugely different
    // timestamp. EncoderIncDec would accelerate based on timestamp deltas;
    // EncoderSet must not -- the resulting value must be identical.
    //
    {
        SmartGrid::MessageInBus bus;
        ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, 0.05f), /*timestamp=*/20);
    }
    {
        SmartGrid::MessageInBus bus;
        ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, target), /*timestamp=*/9000000000ull);
    }
    float valueLargeTs = ReadEncoderValue(system, x, y);

    DOCTEST_CHECK(valueSmallTs == doctest::Approx(valueLargeTs));
}

DOCTEST_TEST_CASE("EncoderIncDec still moves the value (non-regression)")
{
    GlobalEnv::ResetPerTest();

    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();

    int x = -1;
    int y = -1;
    DOCTEST_REQUIRE(FindConnectedCell(system, x, y));

    SmartGrid::MessageInBus bus;

    // Park the encoder at a known mid value so an up-increment has room to move.
    //
    ApplyAndRun(bus, system, SmartGrid::MessageIn::EncoderSetMsg(x, y, 0.5f), /*timestamp=*/200000);
    float before = ReadEncoderValue(system, x, y);

    // Positive IncDec should increase the value.
    //
    SmartGrid::MessageIn incUp(SmartGrid::MessageIn::Mode::EncoderIncDec, x, y, /*amount=*/1);
    ApplyAndRun(bus, system, incUp, /*timestamp=*/300000);
    float afterUp = ReadEncoderValue(system, x, y);
    DOCTEST_CHECK(afterUp > before);

    // Negative IncDec should decrease the value.
    //
    SmartGrid::MessageIn incDown(SmartGrid::MessageIn::Mode::EncoderIncDec, x, y, /*amount=*/-1);
    ApplyAndRun(bus, system, incDown, /*timestamp=*/400000);
    float afterDown = ReadEncoderValue(system, x, y);
    DOCTEST_CHECK(afterDown < afterUp);
}
