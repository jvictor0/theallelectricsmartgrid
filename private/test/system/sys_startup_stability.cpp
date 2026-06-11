// WP-10: startup-stability capstone.
//
// The user's #1 reported concern: "the synth seems UNSTABLE at startup until
// the sequencer is first started." These tests reproduce, characterize, and
// guard the pre-start / start / stop / repeated-cycle behavior of the whole
// system through the frozen SynthRig front door.
//
// ---------------------------------------------------------------------------
// What "instability at startup" actually is (empirical findings, WP-10)
// ---------------------------------------------------------------------------
// Probed via SynthRig + Internal() observables:
//
//   * PRE-START the system is fully QUIESCENT, not unstable in any numeric
//     sense. The TheoryOfTime master phasor is frozen at exactly 0.0 and does
//     not advance (delta over 0.7s == 0.0). The audio path is dead silent
//     (OutputPeak == 0.0) even with the gain faders raised. No NaN, no drift,
//     no growth. So there is NO runaway/exploding pre-start signal.
//
//   * The master phasor only begins advancing once m_running is set (the
//     sequencer's running toggle). TheNonagonInternal forces TheoryOfTime to
//     run when m_running || m_multiPhasorGate.m_anyGate; pre-start neither is
//     true, so time is stopped. THIS is the observable behind "feels unstable
//     until you press start": before the first start the instrument simply
//     produces nothing -- no clock, no notes -- which reads as a dead/limbo
//     state, not as audible artifacts.
//
//   * The FIRST start has a slightly larger output transient than subsequent
//     starts (run-peak ~0.76 on cycle 0 vs ~0.63 thereafter) but it is bounded
//     and NaN-clean -- a normal envelope onset, not an instability.
//
//   * STOP leaves a decaying tail (delay/reverb) rather than instant silence;
//     that is documented expected semantics, not corruption.
//
// To make voices audible we raise the three per-track gain faders (indices
// 5,6,7) -- they default to 0, which is why the smoke test sees a running
// sequencer with zero output. See StressHelpers.hpp::GetNotesPlaying.

#include "doctest.h"

#include <cmath>

#include "../support/SynthRig.hpp"
#include "../support/StressHelpers.hpp"

using synthrig::SynthRig;

namespace
{
constexpr float kOutputBound = 64.0f;   // master chain healthy range
constexpr float kAlarmBound  = 10.0f;   // unbounded-growth alarm threshold
} // namespace

// ---------------------------------------------------------------------------
// Pre-start characterization: no NaN, bounded, and (per findings) silent+frozen.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("startup: pre-start run is finite, bounded, and quiescent")
{
    SynthRig rig;

    // Do NOT start the sequencer. Raise gain so that IF anything were leaking
    // through the audio path we would see it.
    stress::OpenGain(rig);
    rig.RunFrames(2);
    rig.ClearOutput();
    rig.ClearNaN();

    const double phasorBefore = rig.MasterPhasor();
    rig.RunSeconds(0.8);
    const double phasorAfter = rig.MasterPhasor();

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);

    // CHARACTERIZE: pre-start is silent and the master clock is frozen.
    DOCTEST_MESSAGE("pre-start OutputPeak = " << rig.OutputPeak());
    DOCTEST_MESSAGE("pre-start master phasor: " << phasorBefore << " -> " << phasorAfter);

    // WARN on anything alarming (unbounded growth). This is the user's reported
    // "instability" -- we assert it does NOT manifest as a runaway signal.
    if (rig.OutputPeak() > kAlarmBound)
    {
        DOCTEST_WARN_MESSAGE(false,
            "pre-start output exceeded alarm bound (" << rig.OutputPeak()
            << ") -- possible startup instability");
    }

    // Documented semantics: pre-start the clock does not advance and the audio
    // path is silent. (If this ever changes, the message above will flag it.)
    DOCTEST_CHECK(std::fabs(phasorAfter - phasorBefore) < 1e-9);
    DOCTEST_CHECK(rig.OutputPeak() < 1e-6f);
}

// ---------------------------------------------------------------------------
// Start -> run -> stop, then post-stop sanity (decay toward silence, no NaN).
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("startup: start/run/stop leaves a sane, decaying state")
{
    SynthRig rig;
    stress::GetNotesPlaying(rig);
    DOCTEST_CHECK(rig.IsSequencerRunning());

    rig.ClearOutput();
    rig.ClearNaN();
    rig.RunSeconds(0.7);

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);
    const float runPeak = rig.OutputPeak();
    // With notes playing we expect real audio (sanity that the recipe works).
    DOCTEST_CHECK(runPeak > 1e-3f);
    DOCTEST_MESSAGE("running OutputPeak = " << runPeak);

    rig.StopSequencer();
    DOCTEST_CHECK_FALSE(rig.IsSequencerRunning());

    // Post-stop: run stopped, watch the tail decay. The first window holds the
    // delay/reverb tail; a later window should be quieter than the run peak.
    rig.ClearOutput();
    rig.ClearNaN();
    rig.RunSeconds(0.6);
    const float tailPeak = rig.OutputPeak();

    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);
    DOCTEST_MESSAGE("post-stop tail OutputPeak = " << tailPeak);

    // Documented semantics: stop does not hard-mute -- effects tail off. We
    // only assert the tail does not EXCEED the running level (no growth on
    // stop) rather than demanding instant silence.
    DOCTEST_CHECK(tailPeak <= runPeak + 1e-3f);

    // The master phasor must be frozen again after stop.
    const double p0 = rig.MasterPhasor();
    rig.RunSeconds(0.3);
    const double p1 = rig.MasterPhasor();
    DOCTEST_CHECK(std::fabs(p1 - p0) < 1e-9);
}

// ---------------------------------------------------------------------------
// Repeated start/stop cycles: no NaN ever, no monotonic growth in peak, encoder
// UIState stays valid, system still responds to SetEncoder afterward.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("startup: repeated start/stop cycles stay stable")
{
    SynthRig rig;
    rig.RunFrames(2);
    stress::OpenGain(rig);
    rig.RunFrames(2);

    float prevRunPeak = 0.0f;
    float maxRunPeak = 0.0f;
    bool monotonicGrowth = true; // turns false as soon as a cycle is not larger

    constexpr int kCycles = 3;
    for (int c = 0; c < kCycles; ++c)
    {
        rig.ClearOutput();
        rig.ClearNaN();
        rig.StartSequencer();
        rig.RunSeconds(0.4);
        const float runPeak = rig.OutputPeak();

        DOCTEST_CHECK_FALSE(rig.SawNaN());
        DOCTEST_CHECK(runPeak < kOutputBound);

        rig.ClearOutput();
        rig.ClearNaN();
        rig.StopSequencer();
        rig.RunSeconds(0.4);

        DOCTEST_CHECK_FALSE(rig.SawNaN());
        DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);

        if (c > 0 && runPeak <= prevRunPeak + 1e-3f)
        {
            monotonicGrowth = false;
        }
        if (runPeak > maxRunPeak)
        {
            maxRunPeak = runPeak;
        }
        prevRunPeak = runPeak;
        DOCTEST_MESSAGE("cycle " << c << " runPeak = " << runPeak);
    }

    // No runaway growth across cycles: the peak must NOT climb every cycle.
    DOCTEST_CHECK_FALSE(monotonicGrowth);
    DOCTEST_CHECK(maxRunPeak < kOutputBound);

    // Encoder UIState still valid: at least one connected encoder publishes a
    // finite value.
    bool sawConnected = false;
    for (int x = 0; x < 4 && !sawConnected; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y))
            {
                sawConnected = true;
                DOCTEST_CHECK(std::isfinite(rig.EncoderValue(x, y)));
                break;
            }
        }
    }
    DOCTEST_CHECK(sawConnected);

    // System still responds to SetEncoder after all the cycling.
    int ex = -1, ey = -1;
    for (int x = 0; x < 4 && ex < 0; ++x)
    {
        for (int y = 0; y < 4; ++y)
        {
            if (rig.EncoderConnected(x, y)) { ex = x; ey = y; break; }
        }
    }
    DOCTEST_REQUIRE(ex >= 0);
    const float tol = 1.0f / 16383.0f + 1e-3f;
    rig.SetEncoder(ex, ey, 0.33f);
    rig.RunFrames(2);
    DOCTEST_CHECK(std::fabs(rig.EncoderValue(ex, ey) - 0.33f) <= tol);
}

// ---------------------------------------------------------------------------
// Reproduce the user's report directly: compare fresh-rig behavior to behavior
// after a first start/stop. The concrete, reproducible observable is that the
// master clock is FROZEN pre-first-start and only ever advances after a start.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("startup: reproduce pre-first-start frozen-clock observable")
{
    SynthRig rig;
    stress::OpenGain(rig);
    rig.RunFrames(2);

    // (a) Fresh rig, never started: clock frozen, audio silent.
    rig.ClearOutput();
    const double freshP0 = rig.MasterPhasor();
    rig.RunSeconds(0.6);
    const double freshP1 = rig.MasterPhasor();
    const float freshPeak = rig.OutputPeak();

    const bool freshClockFrozen = std::fabs(freshP1 - freshP0) < 1e-9;
    const bool freshSilent = freshPeak < 1e-6f;
    DOCTEST_MESSAGE("FRESH (never started): clockFrozen=" << freshClockFrozen
                    << " silent=" << freshSilent << " peak=" << freshPeak);

    // This IS the reproduction of "unstable until first start": before the
    // first start the instrument is inert. We WARN-document it as the concrete,
    // benign explanation (no NaN / no runaway), so the report is corroborated
    // by a live observable rather than prose alone.
    const bool freshInert = freshClockFrozen && freshSilent;
    DOCTEST_WARN_MESSAGE(freshInert,
        "pre-first-start the master clock advances or audio is non-silent -- "
        "investigate; expected behavior is frozen+silent until first start");
    DOCTEST_CHECK(freshClockFrozen);
    DOCTEST_CHECK(freshSilent);

    // (b) First start: clock advances, audio appears, NaN-clean.
    rig.StartSequencer();
    rig.ClearOutput();
    rig.ClearNaN();
    const double runP0 = rig.MasterPhasor();
    rig.RunSeconds(0.6);
    const double runP1 = rig.MasterPhasor();
    DOCTEST_CHECK(std::fabs(runP1 - runP0) > 0.05);   // clock now moving
    DOCTEST_CHECK(rig.OutputPeak() > 1e-3f);          // audio now present
    DOCTEST_CHECK_FALSE(rig.SawNaN());

    // (c) Stop, then a SECOND start behaves identically (no corrupted state).
    rig.StopSequencer();
    rig.RunSeconds(0.3);
    rig.StartSequencer();
    rig.ClearOutput();
    rig.ClearNaN();
    rig.RunSeconds(0.6);
    DOCTEST_CHECK_FALSE(rig.SawNaN());
    DOCTEST_CHECK(rig.OutputPeak() > 1e-3f);
    DOCTEST_CHECK(rig.OutputPeak() < kOutputBound);
}
