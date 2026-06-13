// sys_theoryoftime_topology.cpp — Theory of Time topology grid: button-based
// cells that set per-loop multipliers (change: ui-state-reset-coverage).
//
// The topology grid lives on the BottomRight base grid (route 3). For loop bit
// i (0..x_numTimeBits-2) the column x=i carries multiplier pads at y=mult-2 for
// mult in {2,3,4,5}; pressing one toggles m_theoryOfTimeInput.m_input[i].m_parentMult.
// The pad whose mult equals the live value lights White (on); the rest are
// Fuscia (off). Values persist via StateSaver ("TheoryOfTimeMult").
//
// These tests confirm BOTH the underlying value (m_parentMult) and the published
// grid light, plus persistence across save/load and behavior across scenes.

#include "doctest.h"

#include <cmath>
#include <string>

#include "../support/SynthRig.hpp"

namespace
{

using synthrig::SynthRig;

// Read loop bit i's live parent multiplier (the value the topology pad sets).
//
int ParentMult(SynthRig& rig, int i)
{
    return rig.Internal().m_nonagon.m_state.m_theoryOfTimeInput.m_input[i].m_parentMult;
}

// The published color of topology grid cell (x, y) on the BottomRight grid.
//
SmartGrid::Color TopoColor(SynthRig& rig, int x, int y)
{
    return rig.Internal().m_nonagon.m_theoryOfTimeTopologyGrid->GetColor(x, y);
}

bool IsWhite(SmartGrid::Color c)
{
    return c.m_red == 253 && c.m_green == 253 && c.m_blue == 253;
}

// Tap the multiplier pad that sets loop bit i to `mult` (y = mult - 2).
//
void TapMult(SynthRig& rig, int i, int mult)
{
    rig.TapPad(SynthRig::RouteBottomRight, i, mult - 2);
    rig.RunFrames(2);
}

} // namespace

// ---------------------------------------------------------------------------
// Pressing a multiplier pad sets the loop's parent multiplier AND the lights
// reflect the active value.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_theoryoftime_topology: multiplier pad sets value and lights")
{
    SynthRig rig;
    rig.RunFrames(2);

    const int bit = 0;
    const int def = ParentMult(rig, bit);
    const int target = (def == 3) ? 4 : 3; // a value different from the default

    TapMult(rig, bit, target);

    // Underlying value changed.
    //
    DOCTEST_CHECK(ParentMult(rig, bit) == target);

    // The target pad lights White; the other multiplier pads do not.
    //
    DOCTEST_CHECK(IsWhite(TopoColor(rig, bit, target - 2)));
    for (int mult = 2; mult <= 5; ++mult)
    {
        if (mult != target)
        {
            DOCTEST_CHECK_FALSE(IsWhite(TopoColor(rig, bit, mult - 2)));
        }
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Topology multipliers survive a save / reset / load round-trip (value + light).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_theoryoftime_topology: multipliers survive save/load round-trip")
{
    SynthRig rig;
    rig.RunFrames(2);

    // Set distinct non-default multipliers across several loop bits.
    //
    const int bits[] = {0, 1, 2};
    const int mults[] = {5, 4, 3};
    for (int k = 0; k < 3; ++k)
    {
        TapMult(rig, bits[k], mults[k]);
        DOCTEST_REQUIRE(ParentMult(rig, bits[k]) == mults[k]);
    }

    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE(!patch.empty());

    rig.ResetToDefaults();
    rig.RunFrames(2);
    // After reset the multipliers should differ from what we set (back to default).
    //
    bool anyChangedByReset = false;
    for (int k = 0; k < 3; ++k)
    {
        if (ParentMult(rig, bits[k]) != mults[k]) { anyChangedByReset = true; }
    }
    DOCTEST_CHECK(anyChangedByReset);

    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(2);

    // Values restored, and the lights republish the restored values.
    //
    for (int k = 0; k < 3; ++k)
    {
        DOCTEST_CHECK(ParentMult(rig, bits[k]) == mults[k]);
        DOCTEST_CHECK(IsWhite(TopoColor(rig, bits[k], mults[k] - 2)));
    }

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// A loaded non-default topology drives the clock: the master phasor advances,
// the loops gate, and the output stays finite and bounded.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_theoryoftime_topology: loaded topology drives the clock")
{
    SynthRig rig;
    rig.RunFrames(2);

    TapMult(rig, 0, 5);
    TapMult(rig, 1, 4);

    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE(!patch.empty());
    rig.ResetToDefaults();
    rig.RunFrames(2);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(2);

    rig.StartSequencer();
    rig.ClearNaN();
    rig.ClearOutput();

    const double p0 = rig.MasterPhasor();
    rig.RunSeconds(0.5);
    const double p1 = rig.MasterPhasor();

    DOCTEST_CHECK(std::fabs(p1 - p0) > 0.001); // clock advanced
    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < 10.0f);

    rig.StopSequencer();
}

// ---------------------------------------------------------------------------
// Scene behavior: the topology multipliers ARE scene-banked (registered with
// the per-scene StateSaver as "TheoryOfTimeMult"). Each scene remembers its own
// multiplier, so switching the active scene swaps the live value, and switching
// back restores it. This pins the scene-banked semantics — a regression that
// made the topology global (or dropped a scene's value) would fail here.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_theoryoftime_topology: multipliers are scene-banked and remembered per scene")
{
    SynthRig rig;
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    rig.SetBlend(0.0f); // scene 0 active
    rig.RunFrames(2);

    // Scene 0: set bit 0 -> 5.
    //
    TapMult(rig, 0, 5);
    DOCTEST_REQUIRE(ParentMult(rig, 0) == 5);

    // Switch to scene 1 (untouched): it carries its OWN value, not scene 0's 5
    // (this is what proves the multiplier is scene-banked, not global).
    //
    rig.SetBlend(1.0f);
    rig.RunFrames(4);
    const int scene1Value = ParentMult(rig, 0);
    DOCTEST_CHECK(scene1Value != 5);

    // Edit it in scene 1 -> 3.
    //
    TapMult(rig, 0, 3);
    DOCTEST_REQUIRE(ParentMult(rig, 0) == 3);

    // Back to scene 0: its 5 is remembered.
    //
    rig.SetBlend(0.0f);
    rig.RunFrames(4);
    DOCTEST_CHECK(ParentMult(rig, 0) == 5);

    // Back to scene 1: its 3 is remembered.
    //
    rig.SetBlend(1.0f);
    rig.RunFrames(4);
    DOCTEST_CHECK(ParentMult(rig, 0) == 3);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Per-scene topology persistence: distinct multipliers in scene 0 and scene 1
// both survive a save / reset / load round-trip (the grid "remembers scenes
// across saved patches").
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_theoryoftime_topology: per-scene multipliers round-trip through a patch")
{
    SynthRig rig;
    rig.SetLeftScene(0);
    rig.SetRightScene(1);

    // Scene 0: bit 0 -> 5.
    //
    rig.SetBlend(0.0f);
    rig.RunFrames(2);
    TapMult(rig, 0, 5);
    DOCTEST_REQUIRE(ParentMult(rig, 0) == 5);

    // Scene 1: bit 0 -> 3.
    //
    rig.SetBlend(1.0f);
    rig.RunFrames(4);
    TapMult(rig, 0, 3);
    DOCTEST_REQUIRE(ParentMult(rig, 0) == 3);

    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE(!patch.empty());

    rig.ResetToDefaults();
    rig.RunFrames(2);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(2);

    // Both scenes' values come back.
    //
    rig.SetBlend(0.0f);
    rig.RunFrames(4);
    DOCTEST_CHECK(ParentMult(rig, 0) == 5);

    rig.SetBlend(1.0f);
    rig.RunFrames(4);
    DOCTEST_CHECK(ParentMult(rig, 0) == 3);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}
