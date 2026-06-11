// dsp_samplesource.cpp  --  unit tests for SampleSource
//
// SampleSource drives:
//   1. PhasorPlayHead -- maps TheoryOfTime phasor -> normalised position [0,1)
//   2. GrainManager<AudioBufferBank> -- launches Resynthesizer grains every
//      Resynthesizer::x_H (= 1024) samples, reads from the buffer via
//      AudioBufferBank::ReadRealTime / GetRealTime.
//   3. Upsampler (x4) -- upsamples the 8-sample micro-block to 32 samples.
//
// In-memory wiring: we construct a real AudioBufferBank by directly pushing a
// shared_ptr<AudioBuffer> with a known waveform (no file I/O required).
//
// Wiring (from SquiggleBoySource.hpp):
//   m_sampleSource.SetAudioBufferBank(input.m_audioBufferBank);
//   input.m_sampleSourceInput.m_theoryOfTime = input.m_theoryOfTime;
//   m_sampleSource.ProcessUBlock(input.m_sampleSourceInput);
//   m_uBlockOutput = m_sampleSource.m_uBlockOutput;
//
// ProcessUBlock operates per "micro-block" = 8 audio samples. We must call it
// once per control frame (= one micro-block). TimeRig already drives
// SampleTimer correctly per control frame, so we call ProcessUBlock once per
// AdvanceControlFrame().
//
// NOTE: Grain synthesis uses Resynthesizer which requires the buffer to have
// enough data for one Resynthesizer::x_tableSize = 4096 window before a grain
// launches. We allocate at least 4096 * 2 samples in our test buffer.
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES; use DOCTEST_ prefixes.

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"
#include "../support/Signal.hpp"
#include "../support/TimeRig.hpp"

#include "AudioBuffer.hpp"
#include "DualSampleSource.hpp"
#include "SampleTimer.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Build an AudioBufferBank with a single AudioBuffer filled with the given
// vector of samples (in-memory, no file I/O).
//
std::shared_ptr<AudioBufferBank> MakeBufferBank(std::vector<float> samples)
{
    auto buf = std::make_shared<AudioBuffer>();
    buf->m_buffer = std::move(samples);
    buf->ComputeSectionExtrema();

    auto bank = std::make_shared<AudioBufferBank>();
    bank->m_audioBuffers.push_back(buf);
    bank->m_bankPosition = 0.0f;
    return bank;
}

// Build a ramp buffer: sample[i] = i / (N-1)  =>  value in [0, 1].
//
std::vector<float> MakeRamp(size_t N)
{
    std::vector<float> v(N);
    for (size_t i = 0; i < N; ++i)
    {
        v[i] = static_cast<float>(i) / static_cast<float>(N > 1 ? N - 1 : 1);
    }
    return v;
}

// Build a sine-wave buffer of N samples at freqHz / sampleRate.
//
std::vector<float> MakeSine(size_t N, double freqHz, double sampleRate)
{
    std::vector<float> v(N);
    const double phaseInc = freqHz / sampleRate;
    for (size_t i = 0; i < N; ++i)
    {
        v[i] = static_cast<float>(std::sin(2.0 * M_PI * phaseInc * i));
    }
    return v;
}

// Build a minimal SampleSource::Input: TheoryOfTime wired, loopIndex set,
// speed=1, start=0, length=1.
//
SampleSource::Input MakeBasicInput(TheoryOfTime* tot, int loopIndex = 0)
{
    SampleSource::Input inp;
    inp.m_theoryOfTime = tot;
    inp.m_phasorPlayHeadInput.m_theoryOfTime = tot;
    inp.m_phasorPlayHeadInput.m_loopIndex = loopIndex;
    inp.m_phasorPlayHeadInput.m_start  = 0.0f;
    inp.m_phasorPlayHeadInput.m_length = 1.0f;
    inp.m_phasorPlayHeadInput.m_speed  = 1.0f;
    // GrainManager input left as default.
    //
    return inp;
}

// The upsampled output block size.
//
constexpr size_t kUBlockSize = SampleTimer::x_controlFrameRate * SampleSource::x_oversample; // 8*4=32

}  // namespace

// ---------------------------------------------------------------------------
// 1. No AudioBufferBank -> output is all zeros (no grains launched).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("SampleSource: no buffer -> silent output")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(512.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();  // prime

    // SampleSource is several MB — heap-allocate to avoid blowing the stack
    // (stack-allocating it overflows depending on stack-base placement).
    //
    auto ssHolder = std::make_unique<SampleSource>();
    SampleSource& ss = *ssHolder;
    // No SetAudioBufferBank call -> m_grainManager.m_audioBuffer == nullptr.

    SampleSource::Input inp = MakeBasicInput(rig.Get(), 0);

    const int nFrames = 32;
    for (int f = 0; f < nFrames; ++f)
    {
        ss.ProcessUBlock(inp);

        TestNan::AssertClean(ss.m_uBlockOutput, kUBlockSize);

        for (size_t i = 0; i < kUBlockSize; ++i)
        {
            DOCTEST_INFO("no-buf output[" << i << "]=" << ss.m_uBlockOutput[i]);
            DOCTEST_CHECK(ss.m_uBlockOutput[i] == 0.0f);
        }

        rig.AdvanceControlFrame();
    }
}

// ---------------------------------------------------------------------------
// 2. Ramp buffer wired: output finite, bounded in [0, 1] after warmup,
//    PhasorPlayHead position makes sense.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("SampleSource: ramp buffer -> finite bounded output")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(4096.0);  // slow cycle so playhead moves gently
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    // 4096 * 2 samples so we have enough for grain launch.
    //
    auto bank = MakeBufferBank(MakeRamp(8192));

    // SampleSource is several MB — heap-allocate to avoid blowing the stack
    // (stack-allocating it overflows depending on stack-base placement).
    //
    auto ssHolder = std::make_unique<SampleSource>();
    SampleSource& ss = *ssHolder;
    ss.SetAudioBufferBank(bank.get());

    SampleSource::Input inp = MakeBasicInput(rig.Get(), 0);

    bool anyBad = false;

    // Run 4 seconds worth of frames to get grains launching.
    //
    const int nFrames = 4 * static_cast<int>(SampleTimer::x_sampleRate / SampleTimer::x_controlFrameRate);
    for (int f = 0; f < nFrames; ++f)
    {
        ss.ProcessUBlock(inp);

        for (size_t i = 0; i < kUBlockSize; ++i)
        {
            float v = ss.m_uBlockOutput[i];
            if (std::isnan(v) || std::isinf(v))
            {
                anyBad = true;
            }
        }

        rig.AdvanceControlFrame();
    }

    DOCTEST_CHECK(!anyBad);

    // Final frame output must be bounded (ramp values are in [0,1] so
    // resynthesized output should be reasonably small; generous bound ±5).
    //
    for (size_t i = 0; i < kUBlockSize; ++i)
    {
        DOCTEST_CHECK(std::abs(ss.m_uBlockOutput[i]) < 5.0f);
    }
}

// ---------------------------------------------------------------------------
// 3. Playback: PhasorPlayHead drives position through buffer; at neutral
//    speed=1 the position should increase with the phasor.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("SampleSource: phasor playhead position increases with phasor")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(2048.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();
    rig.AdvanceControlFrame();

    auto bank = MakeBufferBank(MakeRamp(8192));
    // SampleSource is several MB — heap-allocate to avoid blowing the stack
    // (stack-allocating it overflows depending on stack-base placement).
    //
    auto ssHolder = std::make_unique<SampleSource>();
    SampleSource& ss = *ssHolder;
    ss.SetAudioBufferBank(bank.get());
    SampleSource::Input inp = MakeBasicInput(rig.Get(), /*loopIndex=*/TimeRig::x_masterLoop);
    inp.m_phasorPlayHeadInput.m_speed  = 1.0f;
    inp.m_phasorPlayHeadInput.m_start  = 0.0f;
    inp.m_phasorPlayHeadInput.m_length = 1.0f;

    double prevPos = 0.0;
    double maxPos  = 0.0;
    bool posEverMoved = false;

    const int nFrames = 300;
    for (int f = 0; f < nFrames; ++f)
    {
        ss.ProcessUBlock(inp);
        double pos = ss.m_previousTotalPhaseTime;

        if (pos > prevPos)
        {
            posEverMoved = true;
        }
        if (pos > maxPos)
        {
            maxPos = pos;
        }
        prevPos = pos;

        rig.AdvanceControlFrame();
    }

    DOCTEST_INFO("maxPos=" << maxPos << " posEverMoved=" << posEverMoved);
    DOCTEST_CHECK(posEverMoved);
    DOCTEST_CHECK(maxPos > 0.0);
}

// ---------------------------------------------------------------------------
// 4. Stress: random seeded parameter modulation + ToT tempo changes.
//    Assert output finite, bounded, no index excursions.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("SampleSource: stress - random speed/start/length + ToT tempo changes")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(1024.0);
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    auto bank = MakeBufferBank(MakeSine(8192, 220.0, 48000.0));
    // SampleSource is several MB — heap-allocate to avoid blowing the stack
    // (stack-allocating it overflows depending on stack-base placement).
    //
    auto ssHolder = std::make_unique<SampleSource>();
    SampleSource& ss = *ssHolder;
    ss.SetAudioBufferBank(bank.get());

    TestSignal::WhiteNoise rng(0xABCDEF1234567890ULL);

    SampleSource::Input inp = MakeBasicInput(rig.Get(), /*loopIndex=*/0);

    bool anyBad = false;
    float maxAbs = 0.0f;

    // 2 seconds of control frames.
    //
    const int nFrames = 2 * static_cast<int>(SampleTimer::x_sampleRate / SampleTimer::x_controlFrameRate);

    for (int f = 0; f < nFrames; ++f)
    {
        // Every 64 frames: randomise speed, start, length, and tempo.
        //
        if (f % 64 == 0)
        {
            float r = rng.Next() * 0.5f + 0.5f;  // [0, 1]

            // Speed from the real-system table: pick one of the common values.
            //
            static const float speeds[] = { 0.25f, 0.5f, 1.0f, 2.0f };
            int speedIdx = static_cast<int>(r * 4.0f) % 4;
            inp.m_phasorPlayHeadInput.m_speed = speeds[speedIdx];

            // Start in [0, 0.5], length in [0.1, 1].
            //
            float startR = rng.Next() * 0.5f + 0.5f;
            inp.m_phasorPlayHeadInput.m_start  = startR * 0.5f;
            float lenR = rng.Next() * 0.5f + 0.5f;
            inp.m_phasorPlayHeadInput.m_length = 0.1f + lenR * 0.9f;

            // Modulate TheoryOfTime tempo (simulate BPM change).
            //
            double newPeriod = 512.0 + r * 4096.0;
            rig.SetMasterPeriodSamples(newPeriod);
        }

        ss.ProcessUBlock(inp);

        for (size_t i = 0; i < kUBlockSize; ++i)
        {
            float v = ss.m_uBlockOutput[i];
            if (std::isnan(v) || std::isinf(v))
            {
                anyBad = true;
            }
            else
            {
                maxAbs = std::max(maxAbs, std::abs(v));
            }
        }

        rig.AdvanceControlFrame();
    }

    DOCTEST_INFO("stress maxAbs=" << maxAbs);
    DOCTEST_CHECK(!anyBad);
    // Grain synthesis can boost a bit, but input amplitude was 1; bound at 5.
    //
    DOCTEST_CHECK(maxAbs < 5.0f);
}

// ---------------------------------------------------------------------------
// 5. Looping: run for many master-loop periods and check output stays finite
//    across loop boundaries (wrapping must not introduce NaN/Inf).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("SampleSource: loop wrap - stays finite across boundaries")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;
    rig.SetMasterPeriodSamples(512.0);  // fast loop so we get many wraps
    rig.SetRunning(true);
    rig.AdvanceControlFrame();

    auto bank = MakeBufferBank(MakeRamp(8192));
    // SampleSource is several MB — heap-allocate to avoid blowing the stack
    // (stack-allocating it overflows depending on stack-base placement).
    //
    auto ssHolder = std::make_unique<SampleSource>();
    SampleSource& ss = *ssHolder;
    ss.SetAudioBufferBank(bank.get());
    SampleSource::Input inp = MakeBasicInput(rig.Get(), TimeRig::x_masterLoop);
    inp.m_phasorPlayHeadInput.m_speed  = 1.0f;
    inp.m_phasorPlayHeadInput.m_start  = 0.0f;
    inp.m_phasorPlayHeadInput.m_length = 1.0f;

    bool anyBad = false;

    // 20 master loop periods worth of frames.
    //
    const int nFrames = 20 * 512 / static_cast<int>(SampleTimer::x_controlFrameRate);
    for (int f = 0; f < nFrames; ++f)
    {
        ss.ProcessUBlock(inp);

        for (size_t i = 0; i < kUBlockSize; ++i)
        {
            float v = ss.m_uBlockOutput[i];
            if (std::isnan(v) || std::isinf(v))
            {
                anyBad = true;
            }
        }

        rig.AdvanceControlFrame();
    }

    DOCTEST_CHECK(!anyBad);
}
