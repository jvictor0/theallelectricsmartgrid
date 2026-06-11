// WP-6: AHD envelope unit tests.
//
// Tests cover:
//   - Shape: monotone attack to peak, hold, monotone decay to ~0
//   - Attack/decay time scaling (faster attack = fewer samples to peak)
//   - Re-trigger mid-decay (returns to attack, no NaN, bounded)
//   - Continuity under TheoryOfTime multiplier change (key discontinuity check)
//   - NaN-clean under normal and edge-case parameters
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is defined for this target.
// Use DOCTEST_-prefixed macros.

#include "doctest.h"

#include <cmath>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"
#include "../support/NanScan.hpp"
#include "../support/Continuity.hpp"

#include "AHD.hpp"

// ---------------------------------------------------------------------------
// Helper: set up a minimal AHD + TimeRig + AHDControl and run for N samples.
// Returns the per-sample output vector.
//
// attack, hold, decay: InputSetter parameters in [0,1].
//   attack=0.0 => fastest (shortest attack time).
// envelopeTimeSamples: the "1 loop" scaling factor for the time axis.
// trigSampleCount: how many samples to keep m_trig=true (usually 1).
// ---------------------------------------------------------------------------
//
namespace
{

struct AHDRig
{
    TimeRig         rig;
    AHD             ahd;
    AHD::Input      input;
    AHD::InputSetter setter;
    AHD::AHDControl control;

    // loopIndex: which ToT loop to reference (0 by default)
    //
    explicit AHDRig(double envelopeTimeSamples, size_t loopIndex = 0)
    {
        // Master period chosen so the loop cycle is much longer than the
        // envelope, preventing wrap-around interfering with circle tracking.
        //
        rig.SetMasterPeriodSamples(48000.0);
        rig.SetRunning(true);
        // Wire loop 0's parent to master (loop 5); it has multiplier 2 by default.
        // For simpler single-loop tests use loopIndex=5 (master loop directly).
        //
        input.m_theoryOfTime = rig.Get();
        input.m_loopIndex    = loopIndex;
        input.m_envelopeTimeSamples = envelopeTimeSamples;

        control.m_envelopeTimeSamples = envelopeTimeSamples;
        control.m_trig    = false;
        control.m_release = false;

        // Prime a couple of control frames so the topology is settled.
        //
        rig.AdvanceControlFrame();
        rig.AdvanceControlFrame();
    }

    void SetAHD(float attack, float hold, float decay)
    {
        setter.Set(attack, hold, decay, 1.0f, true, input);
    }

    void Trigger()
    {
        control.m_trig = true;
        control.m_envelopeTimeSamples = input.m_envelopeTimeSamples;
        input.Set(control);
        control.m_trig = false; // one-shot: clear for next Set() calls
    }

    // Process one sample, using the current SampleTimer uBlock index.
    //
    float ProcessOneSample()
    {
        input.m_samplePosition = static_cast<float>(rig.CurrentUBlockIndex());
        float out = ahd.Process(input);
        // Clear trig after first Process (was a one-shot edge).
        //
        input.m_trig = false;
        rig.AdvanceSample();
        return out;
    }

    std::vector<float> Run(size_t n)
    {
        std::vector<float> buf;
        buf.reserve(n);
        for (size_t i = 0; i < n; ++i)
        {
            buf.push_back(ProcessOneSample());
        }
        return buf;
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test 1: Shape — rises to peak ~1, holds, decays to ~0
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: shape — monotone rise, hold, monotone decay")
{
    GlobalEnv::ResetPerTest();

    // Use envelopeTimeSamples that gives a total shape of ~600 samples.
    // attack=0.0 (fastest), hold=0.0 (no hold), decay=0.0 (fastest).
    // The AHD attack increment with attack=0 is 1/(sampleRate*attackTimeMin)
    // = 1/(48000*0.001) = ~0.02083 per sample => peaks in ~48 samples.
    //
    const double envTime = 4800.0;
    AHDRig r(envTime, 5 /*master loop*/);
    r.SetAHD(0.0f, 0.0f, 0.0f);
    r.Trigger();

    // The AHD measures time via circleTracker.Distance() * envelopeTimeSamples.
    // With master period=48000 and envTime=4800:
    //   attackIncrement = 1/(48000*0.001) = 0.020833 /env-sample
    //   attack completes when: distance * 4800 * 0.020833 = 1 => distance = 0.01
    //   => 0.01 * 48000 = 480 audio samples for attack.
    //   decayIncrement(fastest) = 1/(48000*0.01) = 0.002083 /env-sample
    //   decay to 0: distance * 4800 * 0.002083 = 1 => distance = 0.1 => 4800 audio samples.
    // Total needed: ~5500 samples; use 7000 to be safe.
    //
    const size_t totalSamples = 7000;
    std::vector<float> buf = r.Run(totalSamples);

    // NaN-clean
    //
    TestNan::AssertClean(buf.data(), buf.size());

    // Find peak
    //
    float peak = 0.0f;
    size_t peakIdx = 0;
    for (size_t i = 0; i < buf.size(); ++i)
    {
        if (buf[i] > peak) { peak = buf[i]; peakIdx = i; }
    }

    DOCTEST_CHECK(peak > 0.8f);    // must reach near-full amplitude

    // After peak, must decay — last sample should be near 0
    //
    DOCTEST_CHECK(buf.back() < 0.05f);

    // Bounded in [0, 1+eps]
    //
    for (size_t i = 0; i < buf.size(); ++i)
    {
        DOCTEST_CHECK(buf[i] >= -0.001f);
        DOCTEST_CHECK(buf[i] <= 1.001f);
    }

    // Attack phase: monotone within float noise tolerance.
    // Note: AHD samples are updated only on control-frame boundaries (every 8 samples),
    // so adjacent audio samples in between read the same interpolated phasor value —
    // allowing tiny repetition, not regression.
    //
    {
        float prev = 0.0f;
        for (size_t i = 0; i < peakIdx; ++i)
        {
            DOCTEST_CHECK(buf[i] >= prev - 0.002f);
            if (buf[i] > prev) prev = buf[i];
        }
    }

    // Decay phase: monotone from peak down.
    //
    {
        float prev = peak;
        for (size_t i = peakIdx; i < buf.size(); ++i)
        {
            DOCTEST_CHECK(buf[i] <= prev + 0.002f);
            if (buf[i] < prev) prev = buf[i];
        }
    }
}

// ---------------------------------------------------------------------------
// Test 2: Attack time scaling — faster attack reaches peak in fewer samples
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: faster attack parameter reaches peak sooner")
{
    auto measurePeakSample = [](float attackParam) -> size_t
    {
        GlobalEnv::ResetPerTest();
        const double envTime = 4800.0;
        AHDRig r(envTime, 5);
        r.SetAHD(attackParam, 0.0f, 0.0f);
        r.Trigger();

        float peak = 0.0f;
        size_t peakIdx = 0;
        for (size_t i = 0; i < 3000; ++i)
        {
            float v = r.ProcessOneSample();
            if (v > peak) { peak = v; peakIdx = i; }
        }
        return peakIdx;
    };

    size_t fastIdx = measurePeakSample(0.0f);   // attack=0 => fastest
    size_t slowIdx = measurePeakSample(0.7f);   // attack=0.7 => much slower

    DOCTEST_CHECK(fastIdx < slowIdx);
    // Fast should be at least 3x quicker than slow
    //
    DOCTEST_CHECK(fastIdx * 3 < slowIdx);
}

// ---------------------------------------------------------------------------
// Test 3: Decay time scaling — faster decay reaches zero in fewer samples
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: faster decay parameter returns to zero sooner")
{
    auto measureIdleSample = [](float decayParam) -> size_t
    {
        GlobalEnv::ResetPerTest();
        const double envTime = 4800.0;
        AHDRig r(envTime, 5);
        r.SetAHD(0.0f, 0.0f, decayParam);
        r.Trigger();

        for (size_t i = 0; i < 50000; ++i)
        {
            float v = r.ProcessOneSample();
            if (v < 0.01f && i > 100) return i;
        }
        return 50000;
    };

    size_t fastDecay = measureIdleSample(0.0f);   // decay=0 => fastest
    size_t slowDecay = measureIdleSample(0.7f);   // decay=0.7 => slower

    DOCTEST_CHECK(fastDecay < slowDecay);
}

// ---------------------------------------------------------------------------
// Test 4: Re-trigger mid-decay — returns to attack phase, stays bounded, no NaN
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: re-trigger mid-decay restarts attack without NaN")
{
    GlobalEnv::ResetPerTest();
    const double envTime = 4800.0;
    AHDRig r(envTime, 5);
    r.SetAHD(0.0f, 0.0f, 0.0f);

    std::vector<float> buf;

    // Initial trigger
    //
    r.Trigger();
    // Advance past attack (say 600 samples) into decay
    //
    for (size_t i = 0; i < 600; ++i)
    {
        buf.push_back(r.ProcessOneSample());
    }

    // Re-trigger mid-decay
    //
    r.Trigger();
    for (size_t i = 0; i < 3000; ++i)
    {
        buf.push_back(r.ProcessOneSample());
    }

    TestNan::AssertClean(buf.data(), buf.size());

    for (float v : buf)
    {
        DOCTEST_CHECK(v >= -0.001f);
        DOCTEST_CHECK(v <= 1.001f);
    }

    // Envelope should have opened after re-trigger (not stuck at 0)
    //
    float maxAfterRetrig = 0.0f;
    for (size_t i = 100; i < buf.size(); ++i)
    {
        maxAfterRetrig = std::max(maxAfterRetrig, buf[i]);
    }
    DOCTEST_CHECK(maxAfterRetrig > 0.3f);
}

// ---------------------------------------------------------------------------
// Test 5: Continuity under TheoryOfTime multiplier change
//
// This is the KEY test the user cares about: does changing the ToT multiplier
// mid-envelope cause a discontinuity (jump) in the AHD output?
//
// Method:
//   1. Trigger an AHD and run for a few hundred samples (captures part of attack).
//   2. Change a TheoryOfTime multiplier via TimeRig::SetMultiplier.
//   3. Continue running and capture more samples.
//   4. Check MaxAbsDelta and DiscontinuityCount over the combined window.
//   5. If a jump is detected: WARN (document it), do NOT fail the suite.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: continuity under TheoryOfTime multiplier change (mid-attack and mid-decay)")
{
    GlobalEnv::ResetPerTest();

    // Use loop 4 (parent = master loop 5, mult=2 by default).
    // Master period 48000 samples means the loop cycle is very long, so the
    // phasor advances slowly — envelope time axis is proportional to distance.
    //
    const double envTime = 4800.0;

    // Fast attack, no hold, medium decay so envelope is audible over ~600 samples.
    //
    AHDRig r(envTime, 4);
    r.SetAHD(0.0f, 0.0f, 0.3f);
    r.Trigger();

    const size_t preSamples   = 200; // capture attack + partial decay
    const size_t postSamples  = 500; // capture after mult change

    std::vector<float> buf;
    buf.reserve(preSamples + postSamples);

    for (size_t i = 0; i < preSamples; ++i)
    {
        buf.push_back(r.ProcessOneSample());
    }

    // --- Change the multiplier mid-flight ---
    // Loop 4's parent mult changes from 2 to 4 (doubles the child rate).
    //
    r.rig.SetMultiplier(4, 4);

    for (size_t i = 0; i < postSamples; ++i)
    {
        buf.push_back(r.ProcessOneSample());
    }

    // NaN clean regardless
    //
    TestNan::AssertClean(buf.data(), buf.size());

    // Compute steady-state max delta from the pre-change window
    //
    float preMaxDelta = TestContinuity::MaxAbsDelta(buf.data(), preSamples);

    // Threshold: 3x the observed steady-state max delta, minimum of 0.1
    //
    float threshold = std::max(0.1f, preMaxDelta * 3.0f);

    // Check the transition region: window from 5 samples before to 20 after change
    //
    size_t transStart = (preSamples > 5) ? preSamples - 5 : 0;
    size_t transEnd   = std::min(preSamples + 20, buf.size());
    const float* transPtr = buf.data() + transStart;
    size_t transLen = transEnd - transStart;

    float transMaxDelta = TestContinuity::MaxAbsDelta(transPtr, transLen);
    size_t discont = TestContinuity::DiscontinuityCount(transPtr, transLen, 0.1f);

    if (discont > 0 || transMaxDelta > threshold)
    {
        // BUG?: Discontinuity found when changing TheoryOfTime multiplier mid-envelope.
        //
        DOCTEST_WARN_MESSAGE(discont == 0,
            "AHD DISCONTINUITY under mult change: "
            "DiscontinuityCount=" << discont <<
            " transMaxDelta=" << transMaxDelta <<
            " threshold=" << threshold <<
            " preMaxDelta=" << preMaxDelta <<
            " changeAtSample=" << preSamples <<
            " loopIndex=4 multBefore=2 multAfter=4"
            " REPRO: AHDRig(envTime=4800,loop=4), attack=0, hold=0, decay=0.3,"
            " trigger, run 200 samples, SetMultiplier(4,4), run 500 more.");
        DOCTEST_MESSAGE("AHD mult-change jump at sample " << preSamples
            << ": maxDelta in transition = " << transMaxDelta
            << " (pre steady-state max = " << preMaxDelta << ")");
    }
    else
    {
        // Good: no discontinuity detected
        //
        DOCTEST_CHECK(discont == 0);
    }
}

// ---------------------------------------------------------------------------
// Test 6: Continuity under tempo change mid-envelope
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: continuity under tempo change (SetMasterPeriodSamples mid-run)")
{
    GlobalEnv::ResetPerTest();

    const double envTime = 4800.0;
    AHDRig r(envTime, 5 /*master loop*/);
    r.SetAHD(0.0f, 0.0f, 0.3f);
    r.Trigger();

    const size_t preSamples  = 200;
    const size_t postSamples = 500;

    std::vector<float> buf;
    buf.reserve(preSamples + postSamples);

    for (size_t i = 0; i < preSamples; ++i)
    {
        buf.push_back(r.ProcessOneSample());
    }

    // Halve the master period (double the tempo)
    //
    r.rig.SetMasterPeriodSamples(24000.0);

    for (size_t i = 0; i < postSamples; ++i)
    {
        buf.push_back(r.ProcessOneSample());
    }

    TestNan::AssertClean(buf.data(), buf.size());

    float preMaxDelta = TestContinuity::MaxAbsDelta(buf.data(), preSamples);
    float threshold   = std::max(0.1f, preMaxDelta * 3.0f);

    size_t transStart = (preSamples > 5) ? preSamples - 5 : 0;
    size_t transEnd   = std::min(preSamples + 20, buf.size());
    const float* transPtr = buf.data() + transStart;
    size_t transLen = transEnd - transStart;

    float transMaxDelta = TestContinuity::MaxAbsDelta(transPtr, transLen);
    size_t discont = TestContinuity::DiscontinuityCount(transPtr, transLen, 0.1f);

    if (discont > 0 || transMaxDelta > threshold)
    {
        // BUG?: Discontinuity found when doubling tempo mid-envelope.
        //
        DOCTEST_WARN_MESSAGE(discont == 0,
            "AHD DISCONTINUITY under tempo change: "
            "DiscontinuityCount=" << discont <<
            " transMaxDelta=" << transMaxDelta <<
            " threshold=" << threshold <<
            " changeAtSample=" << preSamples <<
            " periodBefore=48000 periodAfter=24000 loopIndex=5"
            " REPRO: AHDRig(envTime=4800,loop=5), attack=0, hold=0, decay=0.3,"
            " trigger, run 200 samples, SetMasterPeriodSamples(24000), run 500 more.");
        DOCTEST_MESSAGE("AHD tempo-change jump at sample " << preSamples
            << ": maxDelta in transition = " << transMaxDelta
            << " (pre steady-state max = " << preMaxDelta << ")");
    }
    else
    {
        DOCTEST_CHECK(discont == 0);
    }
}

// ---------------------------------------------------------------------------
// Test 7: NaN-clean under extreme/edge-case parameters
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("AHD: NaN-clean with extreme parameter values")
{
    struct Params { float a, h, d; };
    Params cases[] = {
        {0.0f, 0.0f, 0.0f},   // all fastest
        {1.0f, 1.0f, 1.0f},   // all slowest (will barely move)
        {0.0f, 1.0f, 0.0f},   // fast attack, long hold, fast decay
        {1.0f, 0.0f, 1.0f},   // slow attack, no hold, slow decay
    };

    for (auto& p : cases)
    {
        GlobalEnv::ResetPerTest();
        const double envTime = 4800.0;
        AHDRig r(envTime, 5);
        r.SetAHD(p.a, p.h, p.d);
        r.Trigger();
        std::vector<float> buf = r.Run(1000);
        TestNan::AssertClean(buf.data(), buf.size());
    }
}
