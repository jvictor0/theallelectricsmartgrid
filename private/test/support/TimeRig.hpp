#pragma once

// Drives the real clock at the host cadence, one theory of time sample per
// audio sample. Phase queries return absolute cycles in the selected domain.
// Call GlobalEnv::ResetPerTest before constructing a fixture.
//

#include <cassert>
#include <cstddef>

#include "SampleTimer.hpp"
#include "TheoryOfTime.hpp"
#include "MessageOut.hpp"

struct TimeRig
{
    static constexpr size_t x_numLoops = TheoryOfTimeBase::x_numLoops;       // 6
    static constexpr int    x_globalLoop = TheoryOfTimeBase::x_globalLoop;   // 5

    TheoryOfTime m_tot;
    TheoryOfTime::Input m_input;
    SmartGrid::MessageOutBuffer m_messageOut;

    // Construct a SampleTimer-coherent rig. Requires GlobalEnv::Init() /
    // ResetPerTest() to have created SampleTimer::s_instance first. Does NOT
    // reset SampleTimer (the caller owns the reset point via ResetPerTest).
    //
    // Defaults: Internal clock, not running. The TheoryOfTime::Input default
    // m_freq (1/4 phase-per-sample) is far too fast for tests, so we pick a
    // gentler global period below. Children default to parentMult = 2.
    //
    TimeRig()
    {
        assert(SampleTimer::s_instance != nullptr &&
               "TimeRig requires SampleTimer::Init (call GlobalEnv::ResetPerTest first)");

        // TheoryOfTime dereferences m_messageOutBuffer on start/stop/clock
        // events, so it must always be wired before Process runs.
        //
        m_tot.SetupMessageOutBuffer(&m_messageOut);

        m_input.m_running = false;

        // Default: global loop period of 256 samples.
        // m_freq is the phase advanced per audio sample.
        //
        m_input.m_freq = 1.0 / 256.0;

        // Mirror the real Input default: each child loop's parent is the next
        // loop up, with multiplier 2.
        //
        for (int i = 0; i < static_cast<int>(x_numLoops); ++i)
        {
            m_input.m_input[i].m_parentIndex = i + 1;
            m_input.m_input[i].m_parentMult = 2;
        }
    }

    // --- Controls ------------------------------------------------------------

    void SetRunning(bool running)
    {
        m_input.m_running = running;
    }

    bool IsRunning() const
    {
        return m_tot.m_sample.m_running;
    }

    // Set the global phase advanced per audio sample (Internal clock). The
    // global loop period in samples is 1/freq. freq must be > 0 and < 1.
    //
    void SetFreqPerSample(double freqPerSample)
    {
        m_input.m_freq = freqPerSample;
    }

    // Natural-tempo helper: set the global loop period directly in samples.
    //
    void SetGlobalPeriodSamples(double samples)
    {
        assert(samples > 0.0);
        m_input.m_freq = 1.0 / samples;
    }

    // Convenience: set global tempo in Hz (cycles/second) at 48 kHz.
    //
    void SetTempoHz(double hz)
    {
        assert(hz > 0.0);
        m_input.m_freq = hz / static_cast<double>(SampleTimer::x_sampleRate);
    }

    // Set the parent multiplier for a child loop. loop must be in [0, x_numLoops-1].
    // The global loop (x_globalLoop) has no parent, so setting it has no effect on
    // the topology. Multiplier semantics: the child completes `mult` cycles per
    // parent cycle. Real input uses small integers (default 2); valid range is
    // mult >= 1 (LCM-based loop sizing assumes positive multipliers).
    //
    void SetMultiplier(size_t loop, int mult)
    {
        assert(loop < x_numLoops);
        assert(mult >= 1);
        m_input.m_input[loop].m_parentMult = mult;
    }

    // Set the parent index for a child loop (advanced topology control). The
    // default chain is parentIndex[i] = i+1. parentIndex >= x_numLoops means
    // "no parent" (the loop becomes a root). Loop changes only take effect at a
    // parent top while running (see TimeLoop::HandleInput).
    //
    void SetParentIndex(size_t loop, int parentIndex)
    {
        assert(loop < x_numLoops);
        m_input.m_input[loop].m_parentIndex = parentIndex;
    }

    // --- Advancing -----------------------------------------------------------

    // Advance exactly one audio sample, then drain the message buffer. The
    // MessageOutBuffer has a fixed capacity of 16 and never self-clears; the
    // real host drains it every frame.
    //
    void AdvanceSample()
    {
        SampleTimer::IncrementSample();
        m_tot.Process(m_input);
        m_messageOut.Clear();
    }

    void AdvanceSamples(size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            AdvanceSample();
        }
    }

    // --- Accessors -----------------------------------------------------------

    // Raw TheoryOfTime, for wiring into DSP Input structs (e.g.
    // AHD::Input::m_theoryOfTime = rig.Get()).
    //
    TheoryOfTime* Get()
    {
        return &m_tot;
    }

    TheoryOfTime::Input& GetInput()
    {
        return m_input;
    }

    SmartGrid::MessageOutBuffer& GetMessageOut()
    {
        return m_messageOut;
    }

    double GlobalPeriodSamples() const
    {
        return m_tot.m_globalPeriodSamples;
    }

    double GetPhase(size_t loop, PhaseDomain domain = PhaseDomain::Modulated) const
    {
        return m_tot.GetPhase(loop, domain);
    }

    double GlobalPhase() const
    {
        return GetPhase(x_globalLoop);
    }

    bool CycleCrossed(size_t loop, PhaseDomain domain = PhaseDomain::Modulated) const
    {
        return m_tot.CrossedCycleBoundary(loop, domain).m_triggered;
    }

    bool Gate(size_t loop) const
    {
        return m_tot.GetLoop(loop).m_gate;
    }

    int64_t GetPeriodTicks(size_t loop) const
    {
        return m_tot.GetPeriodTicks(loop);
    }

    int64_t GetGlobalTickPosition(PhaseDomain domain) const
    {
        return m_tot.GetGlobalTickPosition(domain);
    }

    int64_t GetGlobalTickPosition() const
    {
        return GetGlobalTickPosition(PhaseDomain::Modulated);
    }

    bool AnyChange() const
    {
        return m_tot.AnyChange();
    }
};
