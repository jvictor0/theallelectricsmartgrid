// WP-6: MultiPhasorGate unit tests.
//
// MultiPhasorGateInternal wiring (discovered from TheNonagon.hpp):
//   - Called once per control frame from TheNonagonInternal::Process (when running).
//   - Input needs: m_theoryOfTime (TheoryOfTimeBase*), m_masterLoopSamples, m_numTrigs,
//     m_trigs[voice], m_newTrigCanStart[voice], m_phasorDenominator[voice], m_mute[voice].
//   - It fires gates based on the master phasor position (GetIndirectPhasor(0, masterLoop)).
//   - Gate goes high on trig, goes low when phasor-in-gate exceeds 0.5 * (1/denominator).
//
// *** ENTANGLEMENT NOTE ***
//   The full NonagonTrigLogic setup is tightly coupled to LameJuis lane triggers,
//   IndexArp sub-triggers, and pitch-change events — all from the full sequencer.
//   For standalone tests we bypass NonagonTrigLogic and drive
//   MultiPhasorGateInternal::Input directly, explicitly setting m_trigs and
//   m_newTrigCanStart. This is a valid subset of the real system's behavior.
//
// Tests cover:
//   - Gate fires when a trig is asserted at the correct phasor phase.
//   - Gate falls low after phasor travels ≥ 0.5/denominator.
//   - No stuck gates after stop/start cycle.
//   - NaN-clean on AHD outputs fed by the gate's ahdControl.
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is defined for this target.

#include "doctest.h"

#include <cmath>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"
#include "../support/NanScan.hpp"

#include "MultiPhasorGate.hpp"
#include "AHD.hpp"

// ---------------------------------------------------------------------------
// Helper: build a minimal MultiPhasorGateInternal::Input wired to the TimeRig.
// voice 0 is enabled; all others disabled.
// ---------------------------------------------------------------------------
//
static MultiPhasorGateInternal::Input MakeSimpleInput(TimeRig& rig,
                                                       bool trig,
                                                       bool newTrigCanStart,
                                                       int phasorDenominator = 1)
{
    MultiPhasorGateInternal::Input inp;
    inp.m_theoryOfTime = rig.Get();
    inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
    inp.m_numTrigs = 1;

    inp.m_trigs[0]           = trig;
    inp.m_newTrigCanStart[0] = newTrigCanStart;
    inp.m_phasorDenominator[0] = phasorDenominator;
    inp.m_mute[0] = false;

    // Voice 1+ disabled
    //
    for (size_t i = 1; i < MultiPhasorGateInternal::x_maxPoly; ++i)
    {
        inp.m_trigs[i]           = false;
        inp.m_newTrigCanStart[i] = false;
        inp.m_mute[i]            = false;
    }
    return inp;
}

// ---------------------------------------------------------------------------
// Test 1: Gate fires on trig and goes high
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MultiPhasorGate: gate goes high on trig with newTrigCanStart=true")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    // Short master period (512 samples) so phasor moves quickly.
    //
    rig.SetMasterPeriodSamples(512.0);
    rig.SetRunning(true);

    // Prime the rig so phasor is well-behaved.
    //
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    MultiPhasorGateInternal gate;

    // Fire a trig on voice 0.
    //
    {
        auto inp = MakeSimpleInput(rig, /*trig=*/true, /*newTrigCanStart=*/true, 1);
        inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
        gate.Process(inp);
    }

    // Gate should be high immediately after trig.
    //
    DOCTEST_CHECK(gate.m_gate[0] == true);
    DOCTEST_CHECK(gate.m_anyGate == true);
}

// ---------------------------------------------------------------------------
// Test 2: Gate does NOT fire when newTrigCanStart=false (muted/stopped)
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MultiPhasorGate: gate does not fire when newTrigCanStart=false")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(512.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    MultiPhasorGateInternal gate;

    {
        // trig=true but newTrigCanStart=false
        //
        auto inp = MakeSimpleInput(rig, /*trig=*/true, /*newTrigCanStart=*/false, 1);
        inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
        gate.Process(inp);
    }

    DOCTEST_CHECK(gate.m_gate[0] == false);
    DOCTEST_CHECK(gate.m_anyGate == false);
}

// ---------------------------------------------------------------------------
// Test 3: Gate falls low after phasor travels >= 0.5 of a cycle
//
// With denominator=1, gate goes low when the phasor distance from trigger
// point reaches 0.5. We advance the rig until the phasor has traveled half
// a master cycle and check that the gate drops.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MultiPhasorGate: gate falls low after phasor half-cycle")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    // Small master period so the test runs quickly.
    //
    const double periodSamples = 256.0;
    rig.SetMasterPeriodSamples(periodSamples);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    MultiPhasorGateInternal gate;

    // Fire a trig.
    //
    {
        auto inp = MakeSimpleInput(rig, true, true, 1);
        inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
        gate.Process(inp);
    }
    DOCTEST_CHECK(gate.m_gate[0] == true);

    // Now advance >0.5 of a master cycle, continuing to call Process (no new trig).
    // Gate must drop at some point before the cycle ends.
    //
    bool gateFell = false;
    const size_t maxSamples = static_cast<size_t>(periodSamples) * 2;
    for (size_t s = 0; s < maxSamples; ++s)
    {
        rig.AdvanceSample();
        // Only call Process on control-frame boundaries (mirroring real usage).
        //
        if (SampleTimer::IsControlFrame())
        {
            auto inp = MakeSimpleInput(rig, false, true, 1);
            inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
            gate.Process(inp);

            if (!gate.m_gate[0])
            {
                gateFell = true;
                break;
            }
        }
    }

    DOCTEST_CHECK(gateFell);
}

// ---------------------------------------------------------------------------
// Test 4: No stuck gates after stop/start cycle
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MultiPhasorGate: no stuck gates after stop/start cycle")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(256.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    MultiPhasorGateInternal gate;

    // Fire trig
    //
    {
        auto inp = MakeSimpleInput(rig, true, true, 1);
        inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
        gate.Process(inp);
    }
    DOCTEST_CHECK(gate.m_gate[0] == true);

    // Stop the sequencer (mirrors TheNonagon calling gate.Reset() when not running)
    //
    gate.Reset();

    // After Reset, the individual gate flags are cleared.
    //
    DOCTEST_CHECK(gate.m_gate[0] == false);
    // NOTE: gate.Reset() does NOT clear m_anyGate (it is recalculated only in Process).
    // This is a documentation observation — m_anyGate is stale after Reset() until
    // the next Process() call. The real system always calls Reset() in the not-running
    // path, so it never reads m_anyGate without a following Process() call.

    // Restart: after start, no spurious gates without a new trig.
    //
    rig.SetRunning(false);
    rig.AdvanceControlFrame();
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    {
        auto inp = MakeSimpleInput(rig, false, true, 1);
        inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
        gate.Process(inp);
    }
    DOCTEST_CHECK(gate.m_gate[0] == false);
    // After Process() with no trig, m_anyGate is properly computed = false.
    //
    DOCTEST_CHECK(gate.m_anyGate == false);
}

// ---------------------------------------------------------------------------
// Test 5: NaN-clean on AHD driven by MultiPhasorGate's ahdControl
//
// The real system pipes gate.m_ahdControl[i] into AHD::Input::Set() each sample.
// Verify no NaN/Inf in the AHD output stream.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MultiPhasorGate: AHD driven by ahdControl is NaN-clean")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(256.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    MultiPhasorGateInternal gate;

    // Wire an AHD
    //
    AHD ahd;
    AHD::Input ahdInput;
    AHD::InputSetter ahdSetter;
    ahdInput.m_theoryOfTime = rig.Get();
    ahdInput.m_loopIndex = static_cast<size_t>(TimeRig::x_masterLoop);
    ahdInput.m_envelopeTimeSamples = 1024.0;
    ahdSetter.Set(0.0f, 0.0f, 0.0f, 1.0f, true, ahdInput);

    std::vector<float> buf;
    buf.reserve(1024);

    // Fire trig on first control frame
    //
    bool trig = true;
    const size_t totalSamples = 1024;
    for (size_t s = 0; s < totalSamples; ++s)
    {
        // Control-frame processing
        //
        if (SampleTimer::IsControlFrame())
        {
            auto inp = MakeSimpleInput(rig, trig, true, 1);
            inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
            gate.Process(inp);
            trig = false; // one-shot

            // Copy ahdControl into AHD input via Set (mirroring real system usage)
            //
            ahdInput.Set(gate.m_ahdControl[0]);
        }

        // Per-sample AHD process
        //
        ahdInput.m_samplePosition = static_cast<float>(rig.CurrentUBlockIndex());
        float out = ahd.Process(ahdInput);
        buf.push_back(out);
        rig.AdvanceSample();
    }

    TestNan::AssertClean(buf.data(), buf.size());

    // Output bounded
    //
    for (float v : buf)
    {
        DOCTEST_CHECK(v >= -0.001f);
        DOCTEST_CHECK(v <= 1.001f);
    }
}

// ---------------------------------------------------------------------------
// Test 6: Multiple voices — independent gate state per voice
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("MultiPhasorGate: two voices have independent gate state")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(512.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    MultiPhasorGateInternal gate;

    // Fire trig on voice 0 only, not voice 1.
    //
    {
        auto inp = MakeSimpleInput(rig, true, true, 1);
        inp.m_numTrigs = 2;
        inp.m_trigs[1] = false;
        inp.m_newTrigCanStart[1] = true;
        inp.m_masterLoopSamples = rig.Get()->m_masterLoopSamples;
        gate.Process(inp);
    }

    DOCTEST_CHECK(gate.m_gate[0] == true);
    DOCTEST_CHECK(gate.m_gate[1] == false);
}
