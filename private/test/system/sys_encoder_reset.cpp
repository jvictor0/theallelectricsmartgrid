// sys_encoder_reset.cpp — Regression net for encoder reset / shift-command
// cache coherence (change: ui-state-reset-coverage).
//
// ============================================================
// The class of bug under test
// ============================================================
// Encoder outputs are recomputed lazily. BankedEncoderCell::Compute only runs
// the gesture/modulation mixing when m_forceUpdate is set OR an *affecting*
// modulator/gesture changed this frame (EncoderBank.hpp). The affecting
// bitmasks are cached and only recomputed by SetModulatorsAffecting().
//
// Therefore any edit that CLEARS a cell's modulation/gesture config must also
// invalidate the lazy cache (set force-update). If it does not, the affecting
// bitmask goes empty, no source change can ever re-trigger Compute, and
// m_output stays FROZEN at the last modulated/gestured value — the published
// UI value disagrees with the (now empty) affecting bitmask forever.
//
//   Bug 1: shift-press the ENCODER       (HandleShiftPress)  — missing force-update.
//   Bug 2: shift-press the GESTURE BUTTON (ClearGesture)     — missing force-update.
//
// Note: SynthRig::EncoderValue() reads the UNSLEWED published m_output
// (PopulateUIState copies cell->m_output[k]); so once Compute runs, the value
// settles to the expected target within a control frame, no slew lag.

#include "doctest.h"

#include <cmath>
#include <string>

#include "../support/SynthRig.hpp"

namespace
{

using synthrig::SynthRig;

constexpr int kSettleFrames = 8;
constexpr float kTol = 1e-2f;

// Locate the first connected encoder in the visible 4x4 bank.
//
bool FindConnected(SynthRig& rig, int& ex, int& ey)
{
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y))
            {
                ex = x;
                ey = y;
                return true;
            }
        }
    }
    return false;
}

BitSet16 ModulatorsAffecting(SynthRig& rig, int ex, int ey)
{
    return rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState.GetModulatorsAffecting(ex, ey);
}

BitSet16 GesturesAffecting(SynthRig& rig, int ex, int ey)
{
    return rig.UIState().m_squiggleBoyUIState.m_encoderBankUIState.GetGesturesAffecting(ex, ey);
}

// Set up a single connected encoder with base value `base` and gesture
// `gesture` driven (analog input/fader at `weight`) so the encoder's published
// output is pushed away from base WITHOUT clamping the gesture target (a clamped
// target leaks the surplus increment into the base and muddies the test). Leaves
// the fader at `weight`. Reports (ex,ey) and `ungesturedBase` — the output with
// the gesture neutralized (fader 0), i.e. the true post-clear target.
//
// Mirrors the front-door recipe in sys_gestures.cpp: arm the fader first so
// increments route to the gesture cell, hold the gesture pad, increment, release.
//
void SetupGesturedEncoder(SynthRig& rig, int& ex, int& ey, int gesture,
                          float base, float weight, float& ungesturedBase)
{
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    rig.SetBlend(0.0f);
    rig.RunFrames(2);

    DOCTEST_REQUIRE(FindConnected(rig, ex, ey));

    rig.SetEncoder(ex, ey, base);
    rig.RunFrames(kSettleFrames);

    rig.SetFader(gesture, weight);
    rig.RunFrames(2);

    rig.PressGesturePad(gesture);
    rig.RunFrames(1);
    // Moderate drive: enough to move the output clearly off base, but not so far
    // that the gesture target clamps at 1.0 (which would leak into the base).
    //
    for (int i = 0; i < 4; ++i)
    {
        rig.IncEncoder(ex, ey, +50);
        rig.RunFrames(1);
    }
    rig.ReleaseGesturePad(gesture);
    rig.RunFrames(kSettleFrames);

    // Measure the ungestured base: with the gesture's weight at 0 it contributes
    // nothing, so the output equals the cell's banked (base) value. Restore weight.
    //
    rig.SetFader(gesture, 0.0f);
    rig.RunFrames(kSettleFrames);
    ungesturedBase = rig.EncoderValue(ex, ey);
    rig.SetFader(gesture, weight);
    rig.RunFrames(kSettleFrames);
}

// Assign modulation slot 0 to the connected encoder via the encoder-push
// selection flow (mirrors sys_gestures.cpp Test 4): push the encoder to enter
// selection mode, increment the depth cell at (0,0), deselect via (3,3). After
// this the encoder's modulatorsAffecting bitmask includes slot 0.
//
void SetupModulatedEncoder(SynthRig& rig, int& ex, int& ey)
{
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    rig.SetBlend(0.0f);
    rig.RunFrames(2);

    DOCTEST_REQUIRE(FindConnected(rig, ex, ey));

    rig.PressEncoder(ex, ey);     // enter selection -> visible grid shows depth cells
    rig.RunFrames(1);
    for (int i = 0; i < 4; ++i)
    {
        rig.IncEncoder(0, 0, +50); // drive depth cell (slot 0) to a nonzero amount
        rig.RunFrames(1);
    }
    rig.PressEncoder(3, 3);        // deselect
    rig.RunFrames(kSettleFrames);
}

// How a test removes the configuration under test.
//
enum class ClearMethod { EncoderShift, GestureButton };

void ApplyClear(SynthRig& rig, ClearMethod m, int ex, int ey, int gesture)
{
    if (m == ClearMethod::EncoderShift)
    {
        rig.WithShift([&]() { rig.PressEncoder(ex, ey); rig.ReleaseEncoder(ex, ey); });
    }
    else
    {
        rig.WithShift([&]() { rig.PressGesturePad(gesture); rig.ReleaseGesturePad(gesture); });
    }
    rig.RunFrames(kSettleFrames);
}

} // namespace

// ---------------------------------------------------------------------------
// Bug 1 — shift-press the ENCODER clears its linked gesture AND restores base.
//
// With a gesture active (fader up) the output is driven off-base. Shift-pressing
// the encoder (no gesture selected) routes to HandleShiftPress -> ZeroModulators,
// which deactivates the linked gesture. The encoder must return to its ungestured
// base value. Pre-fix it stays frozen at the gestured value.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: shift-press encoder clears linked gesture and restores base")
{
    SynthRig rig;
    int ex = -1, ey = -1;
    float ungesturedBase = 0.0f;
    SetupGesturedEncoder(rig, ex, ey, /*gesture=*/0, /*base=*/0.20f, /*weight=*/1.0f, ungesturedBase);

    // Precondition: the gesture really moved the output away from base, and the
    // affecting mask shows the gesture (otherwise the test proves nothing).
    //
    const float gestured = rig.EncoderValue(ex, ey);
    DOCTEST_REQUIRE_MESSAGE(std::fabs(gestured - ungesturedBase) > 5.0f * kTol,
        "precondition: gesture must move output off base (gestured " << gestured
        << " vs ungestured base " << ungesturedBase << ")");
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(0));

    // Shift-press the encoder (no gesture selected) -> HandleShiftPress.
    //
    rig.WithShift([&]() {
        rig.PressEncoder(ex, ey);
        rig.ReleaseEncoder(ex, ey);
    });
    rig.RunFrames(kSettleFrames);

    // The linked gesture must be gone from the mask...
    //
    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).IsZero());

    // ...AND the published value must return to the ungestured base (NOT frozen
    // at `gestured`).
    //
    DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - ungesturedBase) <= kTol);

    // The gesture is truly gone: sweeping its analog input no longer moves output.
    //
    rig.SetFader(0, 0.0f);
    rig.RunFrames(kSettleFrames);
    const float atZero = rig.EncoderValue(ex, ey);
    rig.SetFader(0, 1.0f);
    rig.RunFrames(kSettleFrames);
    const float atOne = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(atOne - atZero) <= kTol);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Bug 2 — shift-press the GESTURE BUTTON deletes the gesture AND restores base.
//
// Shift + gesture-selector pad routes to ClearGesture (a different code path),
// which deactivates the gesture but — pre-fix — never sets force-update, so the
// encoder is left frozen at its gestured value.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: shift-press gesture button deletes gesture and restores base")
{
    SynthRig rig;
    int ex = -1, ey = -1;
    float ungesturedBase = 0.0f;
    SetupGesturedEncoder(rig, ex, ey, /*gesture=*/1, /*base=*/0.20f, /*weight=*/1.0f, ungesturedBase);

    const float gestured = rig.EncoderValue(ex, ey);
    DOCTEST_REQUIRE_MESSAGE(std::fabs(gestured - ungesturedBase) > 5.0f * kTol,
        "precondition: gesture must move output off base (gestured " << gestured
        << " vs ungestured base " << ungesturedBase << ")");
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(1));

    // Shift + gesture pad 1 -> ClearGesture(1).
    //
    rig.WithShift([&]() {
        rig.PressGesturePad(1);
        rig.ReleaseGesturePad(1);
    });
    rig.RunFrames(kSettleFrames);

    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).Get(1) == false);
    DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - ungesturedBase) <= kTol);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Orthogonality matrix — gesture clear is correct at every analog-input
// position (down / middle / up) for both clear methods.
//
// The requester's point: regardless of where the gesture-weight analog input
// sits at clear time, deleting the gesture must return the encoder to its
// ungestured base, and a subsequent input sweep must no longer move it.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: gesture clear is correct at every analog-input position")
{
    const float weights[] = {0.0f, 0.5f, 1.0f};
    const ClearMethod methods[] = {ClearMethod::EncoderShift, ClearMethod::GestureButton};
    const int gesture = 2;

    for (ClearMethod method : methods)
    {
        for (float weight : weights)
        {
            DOCTEST_CAPTURE(static_cast<int>(method));
            DOCTEST_CAPTURE(weight);

            SynthRig rig;
            int ex = -1, ey = -1;
            float ungesturedBase = 0.0f;
            SetupGesturedEncoder(rig, ex, ey, gesture, /*base=*/0.20f, weight, ungesturedBase);

            // The gesture must be present in the mask after setup (it was created
            // and activated regardless of weight).
            //
            DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).Get(gesture));

            // At weight > 0 the gesture should visibly move the output off base.
            //
            if (weight > 0.0f)
            {
                DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - ungesturedBase) > 2.0f * kTol);
            }

            ApplyClear(rig, method, ex, ey, gesture);

            // Gesture gone from the mask, output back at the ungestured base.
            //
            DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).Get(gesture) == false);
            DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - ungesturedBase) <= kTol);

            // Post-clear: the analog input is inert (gesture truly removed).
            //
            rig.SetFader(gesture, 0.0f);
            rig.RunFrames(kSettleFrames);
            const float lo = rig.EncoderValue(ex, ey);
            rig.SetFader(gesture, 1.0f);
            rig.RunFrames(kSettleFrames);
            const float hi = rig.EncoderValue(ex, ey);
            DOCTEST_CHECK(std::fabs(hi - lo) <= kTol);

            DOCTEST_CHECK_FALSE(rig.SawNaN());
        }
    }
}

// ---------------------------------------------------------------------------
// Shift-press with a gesture SELECTED deactivates only the selected gesture and
// leaves the modulation slot intact (the HandleShiftPress else-branch).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: shift-press with gesture selected clears only the gesture")
{
    SynthRig rig;
    int ex = -1, ey = -1;

    // Assign a modulator slot first.
    //
    SetupModulatedEncoder(rig, ex, ey);
    DOCTEST_REQUIRE(ModulatorsAffecting(rig, ex, ey).Get(0));

    // Then add a gesture on top (gesture 3, weight 1).
    //
    const int gesture = 3;
    rig.SetFader(gesture, 1.0f);
    rig.RunFrames(2);
    rig.PressGesturePad(gesture);
    rig.RunFrames(1);
    for (int i = 0; i < 4; ++i) { rig.IncEncoder(ex, ey, +50); rig.RunFrames(1); }
    rig.ReleaseGesturePad(gesture);
    rig.RunFrames(kSettleFrames);
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(gesture));

    // Now SELECT the gesture (hold its pad) and shift-press the encoder. The
    // else-branch deactivates the selected gesture only; the modulator survives.
    //
    rig.PressGesturePad(gesture);
    rig.RunFrames(1);
    rig.WithShift([&]() { rig.PressEncoder(ex, ey); rig.ReleaseEncoder(ex, ey); });
    rig.ReleaseGesturePad(gesture);
    rig.RunFrames(kSettleFrames);

    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).Get(gesture) == false);
    DOCTEST_CHECK(ModulatorsAffecting(rig, ex, ey).Get(0)); // modulator intact
    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Modulator-slot path: assigning a modulator sets modulatorsAffecting; shift-
// pressing the encoder clears it (the original "modulation" bug path). Asserted
// deterministically on the mask; value-freeze is proven by the gesture tests
// (same Compute force-update gate). Sequencer runs for live signal / NaN check.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: shift-press encoder clears an assigned modulator slot")
{
    SynthRig rig;
    int ex = -1, ey = -1;
    SetupModulatedEncoder(rig, ex, ey);

    DOCTEST_REQUIRE(ModulatorsAffecting(rig, ex, ey).Get(0));

    rig.StartSequencer();
    rig.RunFrames(4);
    rig.ClearNaN();

    rig.WithShift([&]() { rig.PressEncoder(ex, ey); rig.ReleaseEncoder(ex, ey); });
    rig.RunFrames(kSettleFrames);

    DOCTEST_CHECK(ModulatorsAffecting(rig, ex, ey).IsZero());
    DOCTEST_CHECK(std::isfinite(rig.EncoderValue(ex, ey)));

    rig.RunFrames(8);
    DOCTEST_CHECK_FALSE(rig.SawNaN());
    rig.StopSequencer();
}

// ---------------------------------------------------------------------------
// Scene isolation: a shift-reset performed while one scene is active must not
// disturb the gesture configuration stored in another scene.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: shift-reset in one scene leaves the other scene intact")
{
    SynthRig rig;
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    const int gesture = 4;
    rig.SetFader(gesture, 1.0f);
    rig.SetBlend(0.0f); // scene 0 active
    rig.RunFrames(2);

    int ex = -1, ey = -1;
    DOCTEST_REQUIRE(FindConnected(rig, ex, ey));
    rig.SetEncoder(ex, ey, 0.20f);
    rig.RunFrames(kSettleFrames);

    auto buildGestureHere = [&]() {
        rig.PressGesturePad(gesture);
        rig.RunFrames(1);
        for (int i = 0; i < 4; ++i) { rig.IncEncoder(ex, ey, +50); rig.RunFrames(1); }
        rig.ReleaseGesturePad(gesture);
        rig.RunFrames(kSettleFrames);
    };

    // Build the gesture in scene 0...
    //
    buildGestureHere();
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(gesture));

    // ...and again in scene 1.
    //
    rig.SetBlend(1.0f); // scene 1 active
    rig.RunFrames(kSettleFrames);
    buildGestureHere();
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(gesture));

    // Reset scene 0 only.
    //
    rig.SetBlend(0.0f); // scene 0 active
    rig.RunFrames(kSettleFrames);
    rig.WithShift([&]() { rig.PressEncoder(ex, ey); rig.ReleaseEncoder(ex, ey); });
    rig.RunFrames(kSettleFrames);
    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).IsZero()); // scene 0 cleared

    // Scene 1 must still carry a FUNCTIONAL gesture: its mask bit survived the
    // scene-0 reset, and its analog input still moves the output (i.e. the gesture
    // wasn't silently deactivated). We assert the surviving-and-functional property
    // rather than reproducing an exact pre-reset value, which is sensitive to
    // per-scene base/target interactions across blend switches.
    //
    rig.SetBlend(1.0f);
    rig.RunFrames(kSettleFrames);
    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).Get(gesture));

    rig.SetFader(gesture, 0.0f);
    rig.RunFrames(kSettleFrames);
    const float scene1Ungestured = rig.EncoderValue(ex, ey);
    rig.SetFader(gesture, 1.0f);
    rig.RunFrames(kSettleFrames);
    const float scene1GesturedNow = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(scene1GesturedNow - scene1Ungestured) > 2.0f * kTol);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Persistence: post-reset state survives a save / reset-to-defaults / load
// round-trip (the cleared encoder stays cleared).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: cleared state round-trips through a patch")
{
    SynthRig rig;
    int ex = -1, ey = -1;
    float ungesturedBase = 0.0f;
    SetupGesturedEncoder(rig, ex, ey, /*gesture=*/5, /*base=*/0.20f, /*weight=*/1.0f, ungesturedBase);
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(5));

    // Clear via encoder shift-press, then round-trip the patch.
    //
    rig.WithShift([&]() { rig.PressEncoder(ex, ey); rig.ReleaseEncoder(ex, ey); });
    rig.RunFrames(kSettleFrames);
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).IsZero());
    const float afterClear = rig.EncoderValue(ex, ey);

    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE(!patch.empty());
    rig.ResetToDefaults();
    rig.RunFrames(kSettleFrames);
    DOCTEST_REQUIRE(rig.LoadPatch(patch));
    rig.RunFrames(kSettleFrames);

    // The reloaded encoder is still gesture-free and at the cleared value.
    //
    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).IsZero());
    DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - afterClear) <= kTol);
    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Whole-system reset (ResetToDefaults) clears all modulation + gesture masks.
// Hardened version of sys_gestures.cpp Test 5 (which only WARNed).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("sys_encoder_reset: ResetToDefaults clears all masks and values")
{
    SynthRig rig;
    int ex = -1, ey = -1;

    // Build a modulator AND a gesture on a connected encoder.
    //
    SetupModulatedEncoder(rig, ex, ey);
    const int gesture = 6;
    rig.SetFader(gesture, 1.0f);
    rig.RunFrames(2);
    rig.PressGesturePad(gesture);
    rig.RunFrames(1);
    for (int i = 0; i < 4; ++i) { rig.IncEncoder(ex, ey, +50); rig.RunFrames(1); }
    rig.ReleaseGesturePad(gesture);
    rig.RunFrames(kSettleFrames);

    DOCTEST_REQUIRE(ModulatorsAffecting(rig, ex, ey).Get(0));
    DOCTEST_REQUIRE(GesturesAffecting(rig, ex, ey).Get(gesture));

    rig.ResetToDefaults();
    rig.RunFrames(kSettleFrames);

    DOCTEST_CHECK(ModulatorsAffecting(rig, ex, ey).IsZero());
    DOCTEST_CHECK(GesturesAffecting(rig, ex, ey).IsZero());
    DOCTEST_CHECK_FALSE(rig.SawNaN());
}
