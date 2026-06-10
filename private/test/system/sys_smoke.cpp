// WP-5: SynthRig whole-system smoke test.
//
// Exercises the frozen SynthRig harness end-to-end: silent run, sequencer
// start/stop, encoder set/read-back, and a save/load patch round-trip -- all
// while asserting the output stays finite and bounded. This is the canary that
// the system-level test harness drives the synth exactly like the JUCE wrapper.
//
// Each SynthRig builds a fresh TheNonagonSquiggleBoyInternal (the grid-id
// allocator is a no-op, so per-test fresh state is safe -- see SynthRig.hpp).
//
// Uses the DOCTEST_ prefixed macros (the target defines
// DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES to avoid colliding with the codebase's own
// INFO/WARN logging macros).

#include "doctest.h"

#include <cmath>
#include <string>

#include "../support/SynthRig.hpp"

namespace
{

// A bound that comfortably covers a healthy master chain output while still
// catching runaway/exploding signals.
constexpr float kOutputBound = 64.0f;

} // namespace

DOCTEST_TEST_CASE("SynthRig: silent run is finite and bounded")
{
    synthrig::SynthRig rig;

    rig.RunSeconds(1.5);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);
    // With no sequencer running and silent input the output should be quiet.
    DOCTEST_CHECK(rig.OutputPeak() < 1.0f);
    DOCTEST_CHECK_FALSE(rig.Output().empty());
}

DOCTEST_TEST_CASE("SynthRig: sequencer runs, phasor advances, output bounded")
{
    synthrig::SynthRig rig;

    // Warm up a touch so UIState is populated and a connected encoder exists.
    rig.RunFrames(2);

    bool sawConnectedEncoder = false;
    for (int x = 0; x < 4 && !sawConnectedEncoder; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y))
            {
                sawConnectedEncoder = true;
                break;
            }
        }
    }
    DOCTEST_CHECK(sawConnectedEncoder); // some UIState activity

    rig.StartSequencer();
    DOCTEST_CHECK(rig.IsSequencerRunning());

    rig.ClearOutput();
    rig.ClearNaN();

    // The master loop is exactly 4 seconds long, so sampling the phasor 4s apart
    // would land back where it started. Run a non-multiple so the advance is
    // unambiguous.
    const double phasorBefore = rig.MasterPhasor();
    rig.RunSeconds(1.5);
    const double phasorAfter = rig.MasterPhasor();

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);

    // The master phasor should have clearly moved while the sequencer ran. It
    // wraps in [0,1); 1.5s out of a 4s loop is a ~0.375 advance, well clear of
    // any approximate-equality tolerance.
    DOCTEST_CHECK(std::fabs(phasorAfter - phasorBefore) > 0.1);
}

DOCTEST_TEST_CASE("SynthRig: encoder set values read back")
{
    synthrig::SynthRig rig;

    rig.RunFrames(2);

    // Find a connected encoder cell to operate on.
    int ex = -1;
    int ey = -1;
    for (int x = 0; x < 4 && ex < 0; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y))
            {
                ex = x;
                ey = y;
                break;
            }
        }
    }
    DOCTEST_REQUIRE(ex >= 0);

    // 14-bit quantization tolerance plus a little slack for scene-blend rounding.
    const float tol = 1.0f / 16383.0f + 1e-3f;

    const float values[] = {0.25f, 0.5f, 0.75f};
    for (float v : values)
    {
        rig.SetEncoder(ex, ey, v);
        rig.RunFrames(1);
        DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - v) <= tol);
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

DOCTEST_TEST_CASE("SynthRig: stop sequencer leaves system stable")
{
    synthrig::SynthRig rig;

    rig.RunFrames(2);
    rig.StartSequencer();
    rig.RunSeconds(0.5);

    rig.StopSequencer();
    DOCTEST_CHECK_FALSE(rig.IsSequencerRunning());

    rig.ClearOutput();
    rig.ClearNaN();
    rig.RunSeconds(1.0);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);
}

DOCTEST_TEST_CASE("SynthRig: save/load patch round-trips, stays NaN-clean")
{
    synthrig::SynthRig rig;

    rig.RunFrames(2);

    // Move a connected encoder so the patch is non-trivial.
    int ex = -1;
    int ey = -1;
    for (int x = 0; x < 4 && ex < 0; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y))
            {
                ex = x;
                ey = y;
                break;
            }
        }
    }
    DOCTEST_REQUIRE(ex >= 0);
    rig.SetEncoder(ex, ey, 0.42f);
    rig.RunFrames(1);

    std::string json = rig.SavePatch();
    DOCTEST_CHECK_FALSE(json.empty());

    DOCTEST_CHECK(rig.LoadPatch(json));

    rig.ClearNaN();
    rig.RunSeconds(0.5);
    DOCTEST_CHECK_FALSE(rig.SawNaN());
}
