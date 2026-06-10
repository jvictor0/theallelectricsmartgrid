#pragma once

// =============================================================================
// TimeRig - reusable test fixture that owns and drives a TheoryOfTime exactly
// like the running synth does, so DSP unit tests (AHD, LFOs, SampleSource,
// QuadDelay, MultiPhasorGate, ...) can be exercised in isolation with a
// realistic clock.
//
// API FROZEN after WP-3. Other test-writing agents build on this.
//
// -----------------------------------------------------------------------------
// THE DRIVING CADENCE (discovered by reading the real host; reference doc)
// -----------------------------------------------------------------------------
// SampleTimer (private/src/SampleTimer.hpp) is the global clock:
//   * x_sampleRate             = 48000
//   * x_samplesPerProcessFrame = 512  (audio block boundary -> UI ProcessFrame)
//   * x_controlFrameRate       = 8    (control / "micro block" boundary)
//   * GetUBlockIndex()         = m_sample % 8   (the "j" index into TheoryOfTime)
//   * IsControlFrame()         = m_sample % 8 == 0
//   * IncrementSample()        does ++m_sample FIRST, returns true every 512.
//
// The real per-sample host loop (NonagonWrapper::Process and
// TheNonagonSquiggleBoy::ProcessSample) is:
//
//     for each audio sample:
//         if (SampleTimer::IncrementSample())   // every 512 samples
//             ProcessFrame();                   // UI only; irrelevant to clock
//         ProcessSample(audioIn);               // <-- always
//
// ProcessSample then does (TheNonagonSquiggleBoy::ProcessSample, line ~1297):
//
//     if (SampleTimer::IsControlFrame())        // every 8 samples
//         m_nonagon.Process(m_state);           // <-- the control-frame work
//
// And TheNonagonInternal::Process (line ~307) does, per control frame:
//
//     SetTheoryOfTimeInput(input);
//     m_theoryOfTime.RolloverMicroblockBuffer();      // copy slot 8 -> slot 0
//     ... consume slot 0 (sequencer logic, MultiPhasorGate reads j=0) ...
//     for (size_t j = 1; j < x_microBlockBufferSize; ++j)   // j = 1..8
//         m_theoryOfTime.Process(j, input.m_theoryOfTimeInput);
//
// So the contract is:
//
//   * TheoryOfTime::Process(j, input) is called for j = 1,2,...,8 ONCE per
//     control frame (every 8 audio samples), in a burst, at the control-frame
//     boundary. It is NOT called once per sample.
//
//   * Each Process(j) call (ClockMode::Internal) advances the master phasor by
//     input.m_freq:  input.m_phasor += input.m_freq.  So across one control
//     frame the master phasor advances by 8 * m_freq -- i.e. m_freq is "phase
//     per audio sample", and the master loop period is 1/m_freq samples.
//
//   * The 9-slot per-control-sample arrays (x_microBlockBufferSize = 9, indices
//     0..8) are a sliding window: slot j holds the TheoryOfTime state for the
//     audio sample at offset j within the micro block. Slot 8 is the first
//     sample of the *next* micro block, computed one block early so that
//     interpolation always has accurate boundary samples. At block start
//     RolloverMicroblockBuffer copies slot 8 into slot 0.
//
//   * Per-sample DSP consumers read the slot for "now" using
//     j = SampleTimer::GetUBlockIndex() = m_sample % 8  (range 0..7). See
//     SourceMixer / AHD::m_samplePosition etc. MultiPhasorGate, which runs only
//     at the control-frame boundary, always reads slot 0 (GetIndirectPhasor(0,
//     x_masterLoop)).
//
// TimeRig mirrors this EXACTLY:
//   AdvanceSample():
//       1. SampleTimer::IncrementSample()                 (m_sample advances)
//       2. if IsControlFrame():  run one control frame:
//              RolloverMicroblockBuffer();
//              for j=1..8: m_tot.Process(j, m_input);
//              drain the MessageOutBuffer (the real system drains it via MIDI).
//
//   So after AdvanceSample, CurrentUBlockIndex() == m_sample % 8 is the slot
//   that holds "now", and Phasor(loop)/Top(loop)/Gate(loop) read that slot.
//
// -----------------------------------------------------------------------------
// SampleTimer coherence / interaction with other fixtures
// -----------------------------------------------------------------------------
// TimeRig::AdvanceSample() calls SampleTimer::IncrementSample(), keeping the
// global SampleTimer::s_instance sample counter advancing in lockstep with the
// TheoryOfTime it drives. Classes like AHD call SampleTimer statics, so they
// stay coherent when fed Get().
//
// Because TimeRig drives SampleTimer directly, a test using TimeRig MUST call
// GlobalEnv::ResetPerTest() (which re-Init's SampleTimer to a fresh m_sample=0)
// BEFORE constructing the TimeRig, and must NOT interleave with the
// system-level SystemFixture::RunFrame() (which also drives SampleTimer). A
// TimeRig test and a SystemFixture test should not run within the same advance
// loop. The TimeRig constructor does NOT reset SampleTimer itself (so the
// caller controls the reset point); it merely asserts the singleton exists.
// =============================================================================

#include <cassert>
#include <cstddef>

#include "SampleTimer.hpp"
#include "TheoryOfTime.hpp"
#include "MessageOut.hpp"

struct TimeRig
{
    static constexpr size_t x_numLoops = TheoryOfTimeBase::x_numLoops;       // 6
    static constexpr int    x_masterLoop = TheoryOfTimeBase::x_masterLoop;   // 5
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
    // gentler master period below. Children default to parentMult = 2.
    TimeRig()
    {
        assert(SampleTimer::s_instance != nullptr &&
               "TimeRig requires SampleTimer::Init (call GlobalEnv::ResetPerTest first)");

        // TheoryOfTime dereferences m_messageOutBuffer on start/stop/clock
        // events, so it must always be wired before Process runs.
        m_tot.SetupMessageOutBuffer(&m_messageOut);

        m_input.m_clockMode = TheoryOfTime::ClockMode::Internal;
        m_input.m_running = false;

        // Default: master loop period of 256 samples (32 control frames).
        // m_freq is the phase advanced per audio sample.
        m_input.m_freq = 1.0 / 256.0;

        // Mirror the real Input default: each child loop's parent is the next
        // loop up, with multiplier 2.
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
        return m_tot.m_running;
    }

    // Set the master phase advanced per audio sample (Internal clock). The
    // master loop period in samples is 1/freq. freq must be > 0 and < 1.
    void SetFreqPerSample(double freqPerSample)
    {
        m_input.m_freq = freqPerSample;
    }

    // Natural-tempo helper: set the master loop period directly in samples.
    void SetMasterPeriodSamples(double samples)
    {
        assert(samples > 0.0);
        m_input.m_freq = 1.0 / samples;
    }

    // Convenience: set master tempo in Hz (cycles/second) at 48 kHz.
    void SetTempoHz(double hz)
    {
        assert(hz > 0.0);
        m_input.m_freq = hz / static_cast<double>(SampleTimer::x_sampleRate);
    }

    // Set the parent multiplier for a child loop. loop must be in [0, x_numLoops-1].
    // The master loop (x_masterLoop) has no parent, so setting it has no effect on
    // the topology. Multiplier semantics: the child completes `mult` cycles per
    // parent cycle. Real input uses small integers (default 2); valid range is
    // mult >= 1 (LCM-based loop sizing assumes positive multipliers).
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
    void SetParentIndex(size_t loop, int parentIndex)
    {
        assert(loop < x_numLoops);
        m_input.m_input[loop].m_parentIndex = parentIndex;
    }

    // --- Advancing -----------------------------------------------------------

    // Advance exactly one audio sample, mirroring the real host:
    //   1. SampleTimer::IncrementSample()  (m_sample advances by 1)
    //   2. on a control-frame boundary, run one TheoryOfTime control frame.
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
    void AdvanceControlFrame()
    {
        AdvanceSamples(x_controlFrameRate);
    }

    // --- Accessors -----------------------------------------------------------

    // Raw TheoryOfTime, for wiring into DSP Input structs (e.g.
    // AHD::Input::m_theoryOfTime = rig.Get()).
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
    size_t CurrentUBlockIndex() const
    {
        return static_cast<size_t>(SampleTimer::GetUBlockIndex());
    }

    // Master loop period in samples as TheoryOfTime currently sees it.
    double MasterLoopSamples() const
    {
        return m_tot.m_masterLoopSamples;
    }

    // Indirect (dependent) phasor of a loop at the current uBlock slot, in [0,1).
    // This is the phasor the DSP layer (AHD, MultiPhasorGate) consumes.
    double Phasor(size_t loop) const
    {
        return m_tot.GetIndirectPhasor(CurrentUBlockIndex(), loop);
    }

    // Direct (independent) phasor of a loop at the current slot, in [0,1).
    double DirectPhasor(size_t loop) const
    {
        return m_tot.GetDirectPhasor(CurrentUBlockIndex(), loop);
    }

    // Master-loop indirect phasor at current slot.
    double MasterPhasor() const
    {
        return Phasor(static_cast<size_t>(x_masterLoop));
    }

    // Top / wrap event of a loop at the current slot (dependent topology).
    bool Top(size_t loop) const
    {
        return m_tot.GetIndirectTop(CurrentUBlockIndex(), loop);
    }

    // Master-loop top at current slot.
    bool MasterTop() const
    {
        return Top(static_cast<size_t>(x_masterLoop));
    }

    // Independent (direct) top of a loop at current slot.
    bool DirectTop(size_t loop) const
    {
        return m_tot.GetDirectTop(CurrentUBlockIndex(), loop);
    }

    // Gate (first-half-of-loop) state of a loop at the current slot.
    bool Gate(size_t loop) const
    {
        assert(loop < x_numLoops);
        return m_tot.m_loops[loop].m_gate[CurrentUBlockIndex()];
    }

private:
    // Run a single control frame exactly like TheNonagonInternal::Process:
    // rollover, then Process(j) for j = 1..8, then drain the message buffer
    // (the real system drains it through its MIDI/message consumer).
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
        m_messageOut.Clear();
    }
};
