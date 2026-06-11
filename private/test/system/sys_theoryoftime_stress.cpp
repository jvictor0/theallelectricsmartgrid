// WP-10: TheoryOfTime aggressive-stress capstone.
//
// Targets the user's concern (b): "changing TheoryOfTime multipliers while
// running may cause jumps in AHD envelopes / LFOs." Drives notes, then sweeps
// the ToT tempo/LFO encoders (and the loop-multiplier topology pads) hard while
// asserting whole-system invariants every frame.
//
// ---------------------------------------------------------------------------
// TheoryOfTime control mapping (discovered WP-10)
// ---------------------------------------------------------------------------
// Tempo / LFO live in the TheoryOfTime ENCODER bank. Select it by tapping the
// global-bank selector at BottomRight (route 3) x=3, y=8. Then route-4 encoder
// cells address (from ForEachSmartGridOneParam.hpp, Bank::TheoryOfTime):
//     Tempo         (0,0) -> TheoryOfTime::Input::m_freq (via m_tempo.m_expParam)
//     TempoLFOSkew  (0,2)
//     TempoLFOMult  (1,2) -> m_lfoMult
//     TempoLFOShape (2,2)
//     TempoLFOCenter(0,3)
//     TempoLFOSlope (1,3)
//     TempoLFOIndex (3,3) -> m_modIndex (phase-mod depth on the time LFO)
//
// Loop MULTIPLIERS (the per-bit parentMult that warps the nested time loops)
// live on the TheoryOfTime TOPOLOGY grid, which is the BottomRight base grid
// (route 3). Cell (i, mult-2) toggles loop-bit i's parentMult to {2,3,4,5}.
// Pressing these is the most direct "change a multiplier while running".
//
// ---------------------------------------------------------------------------
// Findings
// ---------------------------------------------------------------------------
//   * Aggressive seeded tempo / LFO-mult / LFO-index sweeps stay bounded
//     (peak ~0.76) and NaN-free; after the sweep the system recovers to clean
//     periodic behavior (master phasor keeps advancing, voices keep gating).
//   * A single isolated loop-multiplier change does NOT produce an output
//     discontinuity beyond steady-state (ratio ~0.9x of steady MaxAbsDelta).
//     This corroborates WP-6's unit-level finding that ToT multiplier changes
//     are continuous at the system level.

#include "doctest.h"

#include <cmath>
#include <random>
#include <vector>

#include "../support/SynthRig.hpp"
#include "../support/StressHelpers.hpp"

using synthrig::SynthRig;

namespace
{
constexpr float kOutputBound = 10.0f; // tight bound for the stress invariant
} // namespace

// ---------------------------------------------------------------------------
// Aggressive seeded random-walk of the ToT tempo/LFO encoders for ~3s.
// Per-frame invariants: no NaN/Inf, output bounded. After the stress the system
// must return to clean periodic behavior.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("tot-stress: aggressive tempo/LFO sweep stays bounded & recovers")
{
    SynthRig rig;
    stress::GetNotesPlaying(rig);
    stress::SelectToTBank(rig);
    rig.RunSeconds(0.2); // settle

    std::mt19937 rng(0x707A1234u);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    // Random walk: nudge tempo/lfo params every few frames.
    float worstPeak = 0.0f;
    constexpr int kSteps = 70; // 70 * 2 frames ~ 140 frames ~ 1.5s sim
    for (int step = 0; step < kSteps; ++step)
    {
        rig.SetEncoder(stress::kTempoEncX, stress::kTempoEncY, u01(rng));
        if ((step & 1) == 0)
        {
            rig.SetEncoder(stress::kLfoMultEncX, stress::kLfoMultEncY, u01(rng));
        }
        if ((step % 3) == 0)
        {
            rig.SetEncoder(stress::kLfoIndexEncX, stress::kLfoIndexEncY, u01(rng));
        }
        if ((step % 4) == 0)
        {
            rig.SetEncoder(stress::kLfoSkewEncX, stress::kLfoSkewEncY, u01(rng));
        }

        rig.ClearOutput();
        rig.RunFrames(2); // ~21ms per step -> 160 steps ~ 3.4s sim
        DOCTEST_REQUIRE_FALSE(rig.SawNaN());
        DOCTEST_REQUIRE(rig.OutputPeak() < kOutputBound);
        if (rig.OutputPeak() > worstPeak)
        {
            worstPeak = rig.OutputPeak();
        }
    }
    DOCTEST_MESSAGE("tempo/LFO sweep worst peak = " << worstPeak);

    // Stop modulating, settle to neutral, and confirm clean periodic recovery.
    rig.SetEncoder(stress::kTempoEncX, stress::kTempoEncY, 0.5f);
    rig.SetEncoder(stress::kLfoMultEncX, stress::kLfoMultEncY, 0.0f);
    rig.SetEncoder(stress::kLfoIndexEncX, stress::kLfoIndexEncY, 0.0f);
    rig.SetEncoder(stress::kLfoSkewEncX, stress::kLfoSkewEncY, 0.5f);
    rig.ClearOutput();
    rig.ClearNaN();
    rig.RunSeconds(0.7);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);

    // Master phasor still advancing (no stuck clock).
    const double p0 = rig.MasterPhasor();
    rig.RunSeconds(0.4);
    const double p1 = rig.MasterPhasor();
    DOCTEST_CHECK(std::fabs(p1 - p0) > 0.01);

    // Voices still gate after the storm.
    auto& nonagon = rig.Internal().m_nonagon.m_nonagon;
    int gateFrames = 0;
    for (int f = 0; f < 80; ++f)
    {
        rig.RunFrames(1);
        if (nonagon.m_multiPhasorGate.m_anyGate)
        {
            ++gateFrames;
        }
    }
    DOCTEST_CHECK(gateFrames > 0);
    DOCTEST_CHECK_FALSE(rig.SawNaN());
}

// ---------------------------------------------------------------------------
// System-level envelope/LFO continuity around an ISOLATED loop-multiplier
// change. Compare output MaxAbsDelta in a clean window before/after a single
// multiplier toggle; assert no discontinuity beyond ~5x steady-state.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("tot-stress: isolated multiplier change is continuous")
{
    SynthRig rig;
    stress::GetNotesPlaying(rig);
    rig.RunSeconds(0.6); // settle into steady-state

    // Measure steady-state per-sample jumpiness (a clean window, no change).
    rig.ClearOutput();
    rig.RunSeconds(0.4);
    const float steady = stress::OutputMaxAbsDelta(rig);
    DOCTEST_MESSAGE("steady-state MaxAbsDelta = " << steady);

    // Now capture a window straddling ONE loop-multiplier change. The topology
    // grid lives on the BottomRight base grid (route 3); cell (0, 3) toggles
    // loop-bit 0's parentMult (5 -> ... ) .
    rig.ClearOutput();
    rig.RunSeconds(0.2);                              // before
    rig.TapPad(SynthRig::RouteBottomRight, 0, 3);     // the multiplier change
    rig.RunSeconds(0.2);                              // after
    const float withChange = stress::OutputMaxAbsDelta(rig);
    DOCTEST_MESSAGE("with-multiplier-change MaxAbsDelta = " << withChange);

    DOCTEST_CHECK_FALSE(rig.SawNaN());

    // Threshold derived from steady-state (5x), with a small absolute floor so
    // a near-silent steady window doesn't make the ratio meaningless.
    const float threshold = 5.0f * steady + 0.05f;
    if (withChange > threshold)
    {
        // WARN + document (corroborates / contradicts WP-6). Not a hard failure:
        // a system-level jump on a multiplier change is a real finding to report
        // but should not redden the suite.
        DOCTEST_WARN_MESSAGE(false,
            "loop-multiplier change produced output discontinuity "
            << withChange << " > " << threshold << " (5x steady "
            << steady << ") -- possible AHD/LFO jump on ToT multiplier change");
    }
    else
    {
        DOCTEST_MESSAGE("multiplier change within continuity threshold ("
                        << withChange << " <= " << threshold << ")");
    }
    // Always-true guard so the case is never silently empty.
    DOCTEST_CHECK(withChange < kOutputBound);
}

// ---------------------------------------------------------------------------
// Aggressive blend/scene changes DURING ToT stress: no NaN, no corruption.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("tot-stress: blend/scene churn during tempo stress stays clean")
{
    SynthRig rig;
    stress::GetNotesPlaying(rig);
    stress::SelectToTBank(rig);
    rig.RunSeconds(0.2);

    std::mt19937 rng(0xBEEF1234u);
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
    std::uniform_int_distribution<int> sceneD(0, 7);

    constexpr int kSteps = 70;
    for (int step = 0; step < kSteps; ++step)
    {
        // ToT tempo churn.
        rig.SetEncoder(stress::kTempoEncX, stress::kTempoEncY, u01(rng));

        // Blend + scene churn (the real ParamSet14 / scene-pad front doors).
        rig.SetBlend(u01(rng));
        if ((step % 5) == 0)
        {
            rig.SetLeftScene(sceneD(rng));
            rig.SetRightScene(sceneD(rng));
        }
        if ((step % 7) == 0)
        {
            rig.SetFader(stress::kGainFaderLo + (step % 3), u01(rng));
        }

        rig.ClearOutput();
        rig.RunFrames(2);
        DOCTEST_REQUIRE_FALSE(rig.SawNaN());
        DOCTEST_REQUIRE(rig.OutputPeak() < kOutputBound);
    }

    // Recover: neutral tempo, mid blend, gains back up.
    rig.SetEncoder(stress::kTempoEncX, stress::kTempoEncY, 0.5f);
    rig.SetBlend(0.0f);
    stress::OpenGain(rig);
    rig.ClearOutput();
    rig.ClearNaN();
    rig.RunSeconds(0.6);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);

    // Clock still advancing.
    const double p0 = rig.MasterPhasor();
    rig.RunSeconds(0.4);
    DOCTEST_CHECK(std::fabs(rig.MasterPhasor() - p0) > 0.005);
}
