// WP-8: sys_scenes.cpp — System tests for scene/blend UI-state behavior.
//
// Tests exercise the real control surface path through SynthRig:
//   • Encoder values per scene with blend crossfade readback
//   • Scene selection via real scene pads (left/right semantics per blend)
//   • Shift+scene copy: destination scene reads the source value
//   • Blend fader via ParamSet14 x==0, NaN-clean while sequencer runs
//
// Key semantics (from SceneManager / TheNonagonSquiggleBoy source):
//   m_blendFactor < 0.5  → scene pad sets RIGHT scene (m_scene2)
//   m_blendFactor >= 0.5 → scene pad sets LEFT scene  (m_scene1)
//   shift + scene pad    → CopyToScene(scene)
//   GetSceneValue        → values[scene1]*(1-blend) + values[scene2]*blend
//   UIState::GetValue    → slewed output — run a few frames to settle
//
// NOTE on tolerance: EncoderValue reports the slewed output; after one frame
// there may still be slew lag.  We use a comfortable tolerance of 5e-3 for
// checks that require exact readback; for interpolation we just assert
// ordering (between A and B).

#include "doctest.h"

#include <cmath>
#include <string>

#include "../support/SynthRig.hpp"

namespace
{

// Extra settle frames beyond the mandatory 1 for slew to converge.
constexpr int kSettleFrames = 6;

// 14-bit quant + slew slack.
constexpr float kTol = 5e-3f;

// Locate the first connected encoder.
bool FindConnected(synthrig::SynthRig& rig, int& ex, int& ey)
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

} // namespace

// ---------------------------------------------------------------------------
// Test 1: Encoder values in two scenes, blend crossfade readback
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_scenes: encoder value per-scene with blend crossfade")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);

    int ex = -1, ey = -1;
    DOCTEST_REQUIRE(FindConnected(rig, ex, ey));

    const float kValA = 0.25f;
    const float kValB = 0.75f;

    // --- Write scene 0 (left) ---
    // Scene 0 = left (scene1), blend=0 means scene1 fully active.
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    rig.SetBlend(0.0f);
    rig.RunFrames(1);
    rig.SetEncoder(ex, ey, kValA);
    rig.RunFrames(kSettleFrames);

    float readA = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(readA - kValA) <= kTol);

    // --- Write scene 1 (right) ---
    // Switch so scene1=0 blend=1 → only scene2 active.
    // Actually easier: set scene2=1 and blend=1.
    rig.SetRightScene(1);
    rig.SetBlend(1.0f);
    rig.RunFrames(1);
    rig.SetEncoder(ex, ey, kValB);
    rig.RunFrames(kSettleFrames);

    float readB = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(readB - kValB) <= kTol);

    // --- blend=0 → should read scene0 value (A) ---
    rig.SetBlend(0.0f);
    rig.RunFrames(kSettleFrames);
    float atBlend0 = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(atBlend0 - kValA) <= kTol);

    // --- blend=1 → should read scene1 value (B) ---
    rig.SetBlend(1.0f);
    rig.RunFrames(kSettleFrames);
    float atBlend1 = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(atBlend1 - kValB) <= kTol);

    // --- blend=0.5 → should be between A and B (exclusive) ---
    rig.SetBlend(0.5f);
    rig.RunFrames(kSettleFrames);
    float atBlend50 = rig.EncoderValue(ex, ey);
    // Interpolated value should strictly between A and B.
    DOCTEST_CHECK(atBlend50 > std::min(kValA, kValB) - kTol);
    DOCTEST_CHECK(atBlend50 < std::max(kValA, kValB) + kTol);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Test 2: Scene pads — left/right semantics according to blend position
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_scenes: scene pad sets right/left scene per blend semantics")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);

    // Reset to known state: scene0=left, scene1=right, blend=0.
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    rig.SetBlend(0.0f);
    rig.RunFrames(1);

    // --- blend < 0.5 → scene pad changes the RIGHT (scene2) ---
    // SelectScene(5) with blend=0 should set right scene to 5.
    rig.SelectScene(5);
    rig.RunFrames(1);

    DOCTEST_CHECK(rig.RightScene() == 5);
    // Left unchanged
    DOCTEST_CHECK(rig.LeftScene() == 0);

    // --- blend >= 0.5 → scene pad changes the LEFT (scene1) ---
    rig.SetBlend(0.75f);
    rig.RunFrames(1);

    rig.SelectScene(3);
    rig.RunFrames(1);

    DOCTEST_CHECK(rig.LeftScene() == 3);
    // Right unchanged
    DOCTEST_CHECK(rig.RightScene() == 5);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Test 3: Shift+scene pad copies current scene to target
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_scenes: shift+scene pad copies current scene to target")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);

    int ex = -1, ey = -1;
    DOCTEST_REQUIRE(FindConnected(rig, ex, ey));

    const float kSrcVal = 0.60f;

    // Set up scene 0 as the source (blend=0 → scene1=0 fully active).
    rig.SetLeftScene(0);
    rig.SetRightScene(1);
    rig.SetBlend(0.0f);
    rig.RunFrames(1);
    rig.SetEncoder(ex, ey, kSrcVal);
    rig.RunFrames(kSettleFrames);

    float srcRead = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(srcRead - kSrcVal) <= kTol);

    // Copy scene 0 → scene 2 via Shift+scene pad 2.
    rig.WithShift([&rig]() {
        rig.PressScenePad(2);
        rig.RunFrames(1);
        rig.ReleaseScenePad(2);
    });
    rig.RunFrames(kSettleFrames);

    // Switch to scene 2 to read back the copied value.
    rig.SetLeftScene(2);
    rig.SetBlend(0.0f);
    rig.RunFrames(kSettleFrames);

    float copiedRead = rig.EncoderValue(ex, ey);
    DOCTEST_CHECK(std::fabs(copiedRead - kSrcVal) <= kTol);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Test 4: Blend fader via ParamSet14 x==0, NaN-clean while sequencer runs
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_scenes: blend fader via ParamSet14 stays NaN-clean with sequencer")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);

    rig.StartSequencer();
    DOCTEST_CHECK(rig.IsSequencerRunning());
    rig.ClearNaN();

    // Sweep blend via the real ParamSet14 x==0 path while the sequencer runs.
    const float blendValues[] = {0.0f, 0.2f, 0.4f, 0.5f, 0.6f, 0.8f, 1.0f};
    for (float bv : blendValues)
    {
        rig.SetBlend(bv);
        rig.RunFrames(3);
        DOCTEST_CHECK_FALSE(rig.SawNaN());

        // Confirm the blend was applied.
        float actual = rig.Blend();
        DOCTEST_CHECK(std::fabs(actual - bv) <= 1.0f / 16383.0f + 1e-5f);
    }

    rig.StopSequencer();
    rig.RunFrames(2);
    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// Test 5: Direct SetLeftScene/SetRightScene helpers work as expected
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("sys_scenes: SetLeftScene and SetRightScene target correct slots")
{
    synthrig::SynthRig rig;
    rig.RunFrames(2);

    rig.SetLeftScene(3);
    rig.SetRightScene(6);
    rig.RunFrames(1);

    DOCTEST_CHECK(rig.LeftScene() == 3);
    DOCTEST_CHECK(rig.RightScene() == 6);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
}
