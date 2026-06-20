// WP-3: TimeRig fixture tests.
//
// Verifies that TimeRig drives a TheoryOfTime exactly like the running synth,
// so downstream DSP tests can rely on it. See support/TimeRig.hpp for the full
// driving-cadence reference documentation.
//
// NOTE: the test target defines DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES, hence the
// DOCTEST_-prefixed macros.

#include "doctest.h"

#include <cmath>
#include <type_traits>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"

#include "AHD.hpp"

namespace
{

// Advance `rig` one sample at a time for `n` samples, recording the master
// indirect phasor at the current slot after each sample. The very first slot of
// a control frame (uBlock 0) is the rolled-over boundary sample from the prior
// frame, so the trace is the "now" value a per-sample consumer would read.
//
std::vector<double> TraceMasterPhasor(TimeRig& rig, size_t n)
{
    std::vector<double> trace;
    trace.reserve(n);
    for (size_t i = 0; i < n; ++i)
    {
        rig.AdvanceSample();
        trace.push_back(rig.MasterPhasor());
    }
    return trace;
}

template <typename T, typename = void>
struct HasClockMode : std::false_type
{
};

template <typename T>
struct HasClockMode<T, std::void_t<decltype(std::declval<T&>().m_clockMode)>> : std::true_type
{
};

} // namespace

DOCTEST_TEST_CASE("TimeRig: construction is SampleTimer-coherent and starts stopped")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    DOCTEST_CHECK(rig.IsRunning() == false);
    DOCTEST_CHECK(SampleTimer::GetSample() == 0);
    DOCTEST_CHECK(rig.CurrentUBlockIndex() == 0);
    DOCTEST_CHECK(rig.Get() != nullptr);
}

DOCTEST_TEST_CASE("TimeRig: TheoryOfTime input has no clock mode")
{
    DOCTEST_CHECK(HasClockMode<TheoryOfTime::Input>::value == false);
}

DOCTEST_TEST_CASE("TimeRig: SampleTimer advances in lockstep with AdvanceSample")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    rig.SetRunning(true);
    for (size_t i = 1; i <= 40; ++i)
    {
        rig.AdvanceSample();
        DOCTEST_CHECK(SampleTimer::GetSample() == i);
        DOCTEST_CHECK(rig.CurrentUBlockIndex() == i % SampleTimer::x_controlFrameRate);
    }
}

DOCTEST_TEST_CASE("TimeRig: master phasor is monotone modulo wrap and Top fires at wrap")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    // Short master period so we get several wraps quickly.
    //
    const double periodSamples = 128.0;
    rig.SetMasterPeriodSamples(periodSamples);
    rig.SetRunning(true);

    // Prime one control frame so slot 0 holds live data, then trace.
    //
    rig.AdvanceControlFrame();

    double prev = rig.MasterPhasor();
    int wraps = 0;
    int topCount = 0;

    // Run several master periods. Only inspect on control-frame boundaries so we
    // read the freshly-rolled-over slot-0 "now" value (top events are recorded
    // per control sample and surface at the slot consumed each sample).
    //
    const size_t totalSamples = static_cast<size_t>(periodSamples) * 6;
    for (size_t i = 0; i < totalSamples; ++i)
    {
        rig.AdvanceSample();
        double cur = rig.MasterPhasor();

        // Monotone increasing except at a wrap (where it drops by ~1).
        //
        bool wrapped = cur < prev;
        if (wrapped)
        {
            ++wraps;
            // A wrap is a near-full-circle backwards jump, not noise.
            //
            DOCTEST_CHECK((prev - cur) > 0.5);
        }
        else
        {
            DOCTEST_CHECK(cur >= prev - 1e-9);
        }

        if (rig.MasterTop())
        {
            ++topCount;
        }

        prev = cur;
    }

    DOCTEST_CHECK(wraps >= 4);
    // Top should fire about once per wrap (one master-loop top per cycle).
    //
    DOCTEST_CHECK(topCount >= wraps);
    DOCTEST_CHECK(topCount <= wraps + 2);
}

DOCTEST_TEST_CASE("TimeRig: child loop completes `mult` cycles per parent cycle")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    rig.SetMasterPeriodSamples(256.0);

    // Loop 4's parent is the master loop (index 5). Give it multiplier 3.
    //
    const int mult = 3;
    rig.SetMultiplier(4, mult);
    rig.SetRunning(true);

    // Prime: let topology settle (loop sizes are set on the running edge).
    //
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    // Count master tops and child(loop 4) tops over several master periods.
    // Tops are single-control-sample events that can fire at any slot within a
    // micro block, so we must inspect every sample's slot, not just slot 0.
    //
    int masterTops = 0;
    int childTops = 0;

    const size_t numSamples = 256 * 8; // ~8 master periods
    for (size_t i = 0; i < numSamples; ++i)
    {
        rig.AdvanceSample();
        if (rig.MasterTop())
        {
            ++masterTops;
        }
        if (rig.Top(4))
        {
            ++childTops;
        }
    }

    DOCTEST_CHECK(masterTops >= 6);
    // Child completes `mult` cycles per master cycle: childTops ~= mult*masterTops.
    // Allow a small boundary tolerance.
    //
    DOCTEST_CHECK(childTops >= mult * masterTops - mult);
    DOCTEST_CHECK(childTops <= mult * masterTops + mult);
}

DOCTEST_TEST_CASE("TimeRig: doubling tempo halves the master period")
{
    auto MeasurePeriod = [](double periodSamples) -> double
    {
        GlobalEnv::ResetPerTest();
        TimeRig rig;
        rig.SetMasterPeriodSamples(periodSamples);
        rig.SetRunning(true);

        // Prime.
        //
        rig.AdvanceControlFrame();

        // Find first top, then measure samples to the next top.
        //
        auto AdvanceToNextTop = [&](size_t maxSamples) -> long
        {
            for (size_t i = 0; i < maxSamples; ++i)
            {
                rig.AdvanceSample();
                if (rig.MasterTop())
                {
                    return static_cast<long>(SampleTimer::GetSample());
                }
            }
            return -1;
        };

        long firstTop = AdvanceToNextTop(static_cast<size_t>(periodSamples) * 4);
        long secondTop = AdvanceToNextTop(static_cast<size_t>(periodSamples) * 4);
        DOCTEST_REQUIRE(firstTop > 0);
        DOCTEST_REQUIRE(secondTop > firstTop);
        return static_cast<double>(secondTop - firstTop);
    };

    double slow = MeasurePeriod(256.0);
    double fast = MeasurePeriod(128.0);

    // Period is quantized to control frames (8 samples), so allow a frame of slop.
    //
    DOCTEST_CHECK(slow == doctest::Approx(256.0).epsilon(0.06));
    DOCTEST_CHECK(fast == doctest::Approx(128.0).epsilon(0.06));
    // Doubling tempo (halving period) halves measured period.
    //
    DOCTEST_CHECK(fast == doctest::Approx(slow / 2.0).epsilon(0.1));
}

DOCTEST_TEST_CASE("TimeRig: not running -> phasors hold at zero")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    // Never call SetRunning(true).
    //
    DOCTEST_CHECK(rig.IsRunning() == false);

    for (size_t i = 0; i < 200; ++i)
    {
        rig.AdvanceSample();
        // Stopped-state contract: ProcessRunning never runs, the master phasor
        // input is forced to 0, and the loops are not advanced. All loop phasors
        // read 0.
        //
        DOCTEST_CHECK(rig.MasterPhasor() == doctest::Approx(0.0));
        DOCTEST_CHECK(rig.Phasor(4) == doctest::Approx(0.0));
        DOCTEST_CHECK(rig.IsRunning() == false);
    }
}

DOCTEST_TEST_CASE("TimeRig: stopped multiplier changes recompute loop sizes before first run")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    rig.SetMultiplier(4, 3);
    rig.AdvanceControlFrame();

    DOCTEST_CHECK(rig.IsRunning() == false);
    DOCTEST_CHECK(rig.AnyChangeInMicroBlock() == true);
    DOCTEST_CHECK(rig.AnyChange(0) == true);
    DOCTEST_CHECK(rig.LoopSize(4) == 32);
    DOCTEST_CHECK(rig.LoopSize(TimeRig::x_masterLoop) == 96);
    DOCTEST_CHECK(rig.Position(4) == 0);
    DOCTEST_CHECK(rig.PrevPosition(4) == 0);
    DOCTEST_CHECK(rig.Gate(4) == false);
    DOCTEST_CHECK(rig.Phasor(4) == doctest::Approx(0.0));
    DOCTEST_CHECK(rig.DirectPhasor(4) == doctest::Approx(0.0));
}

DOCTEST_TEST_CASE("TimeRig: stopping a running rig halts the phasor")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(128.0);
    rig.SetMultiplier(4, 3);
    rig.SetRunning(true);

    // Advance partway into a cycle.
    //
    rig.AdvanceSamples(40);
    DOCTEST_CHECK(rig.IsRunning() == true);

    rig.SetRunning(false);
    // One control frame to let the stop take effect.
    //
    rig.AdvanceControlFrame();
    DOCTEST_CHECK(rig.IsRunning() == false);
    DOCTEST_CHECK(rig.AnyChangeInMicroBlock() == true);
    DOCTEST_CHECK(rig.AnyChange(0) == true);
    DOCTEST_CHECK(rig.LoopSize(4) == 32);
    DOCTEST_CHECK(rig.LoopSize(TimeRig::x_masterLoop) == 96);
    DOCTEST_CHECK(rig.Position(4) == 0);
    DOCTEST_CHECK(rig.PrevPosition(4) == 0);
    DOCTEST_CHECK(rig.Gate(4) == false);

    // After Stop(), topology-derived loop sizes are preserved while motion,
    // gate, and phasor state hold at 0.
    //
    for (size_t i = 0; i < 64; ++i)
    {
        rig.AdvanceSample();
        DOCTEST_CHECK(rig.MasterPhasor() == doctest::Approx(0.0));
    }
}

DOCTEST_TEST_CASE("TimeRig: determinism - two fresh rigs give identical traces")
{
    GlobalEnv::ResetPerTest();
    TimeRig rigA;
    rigA.SetMasterPeriodSamples(200.0);
    rigA.SetMultiplier(3, 2);
    rigA.SetMultiplier(4, 3);
    rigA.SetRunning(true);
    std::vector<double> traceA = TraceMasterPhasor(rigA, 2000);

    GlobalEnv::ResetPerTest();
    TimeRig rigB;
    rigB.SetMasterPeriodSamples(200.0);
    rigB.SetMultiplier(3, 2);
    rigB.SetMultiplier(4, 3);
    rigB.SetRunning(true);
    std::vector<double> traceB = TraceMasterPhasor(rigB, 2000);

    DOCTEST_REQUIRE(traceA.size() == traceB.size());
    bool identical = true;
    for (size_t i = 0; i < traceA.size(); ++i)
    {
        if (traceA[i] != traceB[i])
        {
            identical = false;
            break;
        }
    }
    DOCTEST_CHECK(identical);
}

DOCTEST_TEST_CASE("TimeRig: drives an AHD envelope to nonzero and back to ~0")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(256.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    // Wire an AHD against loop 4 fed by the rig's TheoryOfTime.
    //
    AHD ahd;
    AHD::Input input;
    input.m_theoryOfTime = rig.Get();
    input.m_loopIndex = 4;

    // The AHD measures elapsed envelope time as
    //   samples = circleTracker.Distance() * envelopeTimeSamples
    // where Distance() is the loop phasor distance travelled since the trigger.
    // InputSetter::Set maps attack/decay via Update(1 - value): value=0.0 yields
    // the FASTEST increment, value~1.0 the slowest. We pick fast attack+decay
    // and a large envelopeTimeSamples so the envelope opens and fully closes
    // within a fraction of a loop cycle (well inside our window).
    //
    AHD::InputSetter setter;
    setter.Set(/*attack*/ 0.0f, /*hold*/ 0.0f, /*decay*/ 0.0f,
               /*amplitude*/ 1.0f, /*amplitudePolarity*/ true, input);

    input.m_envelopeTimeSamples = 4000.0;

    // Trigger: mirror the control path. m_trig/circleTracker reset happens via
    // Input::Set against an AHDControl.
    //
    AHD::AHDControl control;
    control.m_trig = true;
    control.m_envelopeTimeSamples = input.m_envelopeTimeSamples;
    input.Set(control);

    float peak = 0.0f;
    float last = 1.0f;

    // First process call with m_trig set starts the envelope.
    //
    for (size_t i = 0; i < 4000; ++i)
    {
        input.m_samplePosition = static_cast<float>(rig.CurrentUBlockIndex());
        float out = ahd.Process(input);
        DOCTEST_CHECK(std::isfinite(out));
        peak = std::max(peak, out);
        last = out;

        // After the first Process, clear the trig (it is a one-shot edge).
        //
        if (i == 0)
        {
            control.m_trig = false;
            input.Set(control);
        }

        rig.AdvanceSample();
    }

    // Envelope must have opened (nonzero peak) and returned toward 0.
    //
    DOCTEST_CHECK(peak > 0.1f);
    DOCTEST_CHECK(last < 0.1f);
}
