#pragma once

// Drives the real clock at the host cadence: slots 1 through 8 are prepared at
// each control boundary, slot 8 rolls into slot 0, and consumers read the current
// SampleTimer slot. Phase queries return absolute cycles in the selected domain.
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
    static constexpr size_t x_controlFrameRate = SampleTimer::x_controlFrameRate; // 8

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

        // Default: global loop period of 256 samples (32 control frames).
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
        return m_tot.m_samples[CurrentUBlockIndex()].m_running;
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

    // Advance exactly one audio sample, mirroring the real host:
    //   1. SampleTimer::IncrementSample()  (m_sample advances by 1)
    //   2. on a control-frame boundary, run one TheoryOfTime control frame.
    //
    void AdvanceSample()
    {
        SampleTimer::IncrementSample();

        if (SampleTimer::IsControlFrame())
        {
            RunControlFrame();
        }
    }

    void AdvanceSamples(size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            AdvanceSample();
        }
    }

    // Advance one full control frame (8 samples). Lands on a control-frame
    // boundary (assuming you started on one, which you do right after
    // ResetPerTest()).
    //
    void AdvanceControlFrame()
    {
        AdvanceSamples(x_controlFrameRate);
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

    // The slot index for "now" = m_sample % 8, in [0, 7].
    //
    size_t CurrentUBlockIndex() const
    {
        return static_cast<size_t>(SampleTimer::GetUBlockIndex());
    }

    double GlobalPeriodSamples() const
    {
        return m_tot.m_globalPeriodSamples;
    }

    double GetPhase(size_t loop, PhaseDomain domain = PhaseDomain::Modulated) const
    {
        return m_tot.GetPhase(loop, CurrentUBlockIndex(), domain);
    }

    double GlobalPhase() const
    {
        return GetPhase(x_globalLoop);
    }

    bool CycleCrossed(size_t loop, PhaseDomain domain = PhaseDomain::Modulated) const
    {
        return m_tot.CrossedCycleBoundary(loop, CurrentUBlockIndex(), domain);
    }

    bool Gate(size_t loop) const
    {
        return m_tot.GetLoop(loop, CurrentUBlockIndex()).m_gate;
    }

    int64_t GetPeriodTicks(size_t loop) const
    {
        return m_tot.GetPeriodTicks(loop, CurrentUBlockIndex());
    }

    int64_t GetPosition(PhaseDomain domain = PhaseDomain::Modulated) const
    {
        return m_tot.GetPosition(CurrentUBlockIndex(), domain);
    }

    bool AnyChangeInMicroBlock() const
    {
        return m_tot.AnyChangeInMicroBlock();
    }

    bool AnyChange(size_t j) const
    {
        assert(j < TheoryOfTimeBase::x_microBlockBufferSize);
        return m_tot.m_samples[j].m_anyChange;
    }

private:
    // Run a single control frame exactly like TheNonagonInternal::Process:
    // rollover, then Process(j) for j = 1..8, then drain the message buffer
    // (the real system drains it through its MIDI/message consumer).
    //
    void RunControlFrame()
    {
        m_tot.RolloverMicroblockBuffer();

        for (size_t j = 1; j < TheoryOfTimeBase::x_microBlockBufferSize; ++j)
        {
            m_tot.Process(j, m_input);
        }

        // The MessageOutBuffer has a fixed capacity of 16 and never self-clears;
        // TheoryOfTime pushes Start/Stop/Clock messages into it. The real host
        // drains it every frame; mirror that so it never silently saturates.
        //
        m_messageOut.Clear();
    }
};
