// dsp_quaddelay.cpp  --  unit tests for QuadDelay
//
// QuadDelay is a complex, stateful delay (LFO mod, grains / Resynthesizer,
// tape heads, feedback, damping). We drive it by setting Input fields directly
// (writing head/read head as simple monotone counters) rather than going
// through QuadDelayInputSetter (which requires full TapeHead / TheoryOfTime
// wiring). This matches how the real system works at the low level once
// QuadDelayInputSetter has computed the positions.
//
// Wiring discovered:
//   - Input::m_writeHeadPosition[4] and m_readHeadPosition[4] are QuadDouble:
//     "warped time" (monotonically increasing write position, read = write - delay).
//   - Input::m_relativeWriteHeadPosition / m_relativeReadHeadPosition are
//     QuadFloat normalised [0,1]; used for UI only.
//   - Input::m_input  = external audio input, m_return = feedback audio return.
//   - feedback <= 0 disables the feedback path.
//   - m_lfoInput.m_freq  -- LFO frequency per channel; 0 = no LFO.
//   - m_modDepth  -- 0 = no modulation.
//   - m_grainManagerInput is heavy; keep as default (resynthesis inactive).
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is active; use DOCTEST_ prefixes.

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"
#include "../support/Signal.hpp"

#include <memory>

#include "QuadDelay.hpp"
#include "OLA.hpp"
#include "SampleTimer.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Advance write/read positions by one sample (monotone counters).
// writePos tracks the absolute "warped time" of the write head.
// readPos  = writePos - delayLength (lag by a fixed delay).
//
void AdvanceHeads(QuadDouble& writePos, QuadDouble& readPos, double delayLen)
{
    for (size_t i = 0; i < 4; ++i)
    {
        writePos[i] += 1.0;
        readPos[i]   = writePos[i] - delayLen;
    }
}

// Build a minimal, valid QuadDelay::Input for a given write/read position pair
// with zero feedback and no modulation. Grain-manager input left as default.
//
QuadDelay::Input MakeBasicInput(QuadDouble writePos, QuadDouble readPos,
                                QuadFloat externalIn, float feedback = 0.0f,
                                float modDepth = 0.0f)
{
    QuadDelay::Input inp;
    inp.m_input              = externalIn;
    inp.m_return             = QuadFloat(0.0f, 0.0f, 0.0f, 0.0f);
    inp.m_feedback           = QuadFloat(feedback, feedback, feedback, feedback);
    inp.m_rotate             = QuadFloat(0.0f, 0.0f, 0.0f, 0.0f);
    inp.m_modDepth           = QuadFloat(modDepth, modDepth, modDepth, modDepth);
    inp.m_writeHeadPosition  = writePos;
    inp.m_readHeadPosition   = readPos;
    // Default relative positions (UI, does not affect audio path).
    //
    inp.m_relativeWriteHeadPosition = QuadFloat(-1.0f, -1.0f, -1.0f, -1.0f);
    inp.m_relativeReadHeadPosition  = QuadFloat(-1.0f, -1.0f, -1.0f, -1.0f);
    // Wide-open damping filter: pass everything.
    //
    inp.m_bffBase  = QuadFloat(1.0f/4096.0f, 1.0f/4096.0f, 1.0f/4096.0f, 1.0f/4096.0f);
    inp.m_bffWidth = QuadFloat(4096.0f, 4096.0f, 4096.0f, 4096.0f);
    // QuadLFO::Process calls m_slew[i].SetAlphaFromNatFreq(freq * 2), so freq
    // must be > 0.  Use a very slow LFO (~0.5 Hz at 48 kHz) that has negligible
    // effect on the delay output but satisfies the assertion.
    //
    for (size_t i = 0; i < 4; ++i)
    {
        inp.m_lfoInput.m_freq[i]      = 0.5f / 48000.0f;
        inp.m_lfoInput.m_phaseKnob[i] = 0.0f;
    }
    return inp;
}

constexpr double kDelayLen = 2048.0;  // samples delay

}  // namespace

// ---------------------------------------------------------------------------
// 1. Silence in -> silence out (after warmup).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadDelay: silence in -> silence out")
{
    GlobalEnv::ResetPerTest();

    // QuadDelay is ~800 MB on the heap (16M-sample delay line × 4 channels ×
    // float + double arrays). Must NOT be stack-allocated.
    //
    auto qdPtr = std::make_unique<QuadDelay>();
    QuadDelay& qd = *qdPtr;

    QuadDouble writePos(kDelayLen, kDelayLen, kDelayLen, kDelayLen);
    QuadDouble readPos(0.0, 0.0, 0.0, 0.0);

    const int warmup = static_cast<int>(kDelayLen) + 64;
    const int measure = 256;
    const int N = warmup + measure;

    std::vector<float> ch0out(static_cast<size_t>(N));

    for (int i = 0; i < N; ++i)
    {
        QuadFloat in(0.0f, 0.0f, 0.0f, 0.0f);
        QuadDelay::Input inp = MakeBasicInput(writePos, readPos, in);
        QuadFloat out = qd.Process(inp);
        ch0out[static_cast<size_t>(i)] = out[0];
        AdvanceHeads(writePos, readPos, kDelayLen);
    }

    // After warmup, output must be silent (all zeros in -> all zeros out).
    //
    for (int i = warmup; i < N; ++i)
    {
        DOCTEST_INFO("silence@" << i << "=" << ch0out[static_cast<size_t>(i)]);
        DOCTEST_CHECK(std::abs(ch0out[static_cast<size_t>(i)]) < 1e-4f);
    }

    TestNan::AssertClean(ch0out.data(), ch0out.size());
}

// ---------------------------------------------------------------------------
// 2. Impulse in -> energy emerges at some point; everything finite.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadDelay: warmed impulse produces finite nonzero output")
{
    GlobalEnv::ResetPerTest();
    auto delay = std::make_unique<QuadDelay>();
    constexpr int x_delay = 8192;
    constexpr int x_impulseSample = 16384 + 317;
    double energy = 0;
    double preImpulseEnergy = 0;
    for (int sample = 0; sample < 32768; ++sample)
    {
        float impulse = sample == x_impulseSample ? 1.0f : 0.0f;
        auto input = MakeBasicInput(
            QuadDouble(sample, sample, sample, sample),
            QuadDouble(sample - x_delay, sample - x_delay, sample - x_delay, sample - x_delay),
            QuadFloat(impulse, 0, 0, 0));
        QuadFloat output = delay->Process(input);
        for (int channel = 0; channel < 4; ++channel)
        {
            DOCTEST_REQUIRE(std::isfinite(output[channel]));
            DOCTEST_REQUIRE(std::abs(output[channel]) < 2.0f);
        }

        energy += output[0] * output[0];
        if (sample < x_impulseSample)
        {
            preImpulseEnergy += output[0] * output[0];
        }
    }

    DOCTEST_CHECK(energy > 1e-4);
    DOCTEST_CHECK(energy < 2.0);
    DOCTEST_CHECK(preImpulseEnergy < 1e-12);
}

DOCTEST_TEST_CASE("QuadDelay: steady tones retain gain and shift by the requested octave")
{
    for (bool shifted : {false, true})
    {
        GlobalEnv::ResetPerTest();
        auto delay = std::make_unique<QuadDelay>();
        OLA::Buffer buffer;
        double energy = 0;
        for (int sample = 0; sample < 32768; ++sample)
        {
            float tone = 0.01f * std::cos(2.0 * M_PI * 18.0 * sample / 4096.0);
            auto input = MakeBasicInput(
                QuadDouble(sample, sample, sample, sample),
                QuadDouble(sample - 8192, sample - 8192, sample - 8192, sample - 8192),
                QuadFloat(tone, 0, 0, 0));
            input.m_bffBase = QuadFloat(1.0f / 65536.0f, 1.0f / 65536.0f, 1.0f / 65536.0f, 1.0f / 65536.0f);
            input.m_bffWidth = QuadFloat(65536, 65536, 65536, 65536);
            auto& resynthesis = input.m_grainManagerInput.m_input[0].m_resynthInput;
            resynthesis.m_shift[0] = Q(2, 1);
            resynthesis.m_fade[0] = shifted ? 1.0f : 0.0f;
            float output = delay->Process(input)[0];
            if (sample >= 28672)
            {
                buffer.m_table[sample - 28672] = output;
                energy += output * output;
            }
        }

        OLA::DFT spectrum;
        spectrum.Transform(buffer);
        size_t targetBin = shifted ? 36 : 18;
        DOCTEST_CHECK(std::abs(spectrum.m_components[targetBin]) > 0.005f);
        DOCTEST_CHECK(std::abs(spectrum.m_components[targetBin]) < 0.0056f);
        DOCTEST_CHECK(energy / 4096.0 > 0.00005);
        DOCTEST_CHECK(energy / 4096.0 < 0.000063);
        for (size_t bin = 1; bin < OLA::x_maxComponents; ++bin)
        {
            if (bin != targetBin)
            {
                DOCTEST_REQUIRE(std::abs(spectrum.m_components[bin]) < 1e-5f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 3. Stress: random-but-seeded parameter modulation for several simulated
//    seconds. Assert NaN-clean, bounded, no runaway feedback.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadDelay: stress - random seeded modulation, finite+bounded")
{
    GlobalEnv::ResetPerTest();

    auto qdPtr = std::make_unique<QuadDelay>();
    QuadDelay& qd = *qdPtr;

    TestSignal::WhiteNoise rng(0xDEADBEEF1337ULL);
    TestSignal::WhiteNoise audioNoise(0xCAFEBABE4242ULL);

    // Simulate ~2 seconds at 48 kHz.
    //
    const int N = 2 * 48000;

    QuadDouble writePos(kDelayLen, kDelayLen, kDelayLen, kDelayLen);
    QuadDouble readPos(0.0, 0.0, 0.0, 0.0);

    float maxAbs = 0.0f;
    bool anyNan = false;
    QuadFloat previousOutput;
    float feedback = 0.0f;
    float modDepth = 0.0f;

    for (int i = 0; i < N; ++i)
    {
        // Modulate parameters every 128 samples.
        //
        if (i % 128 == 0)
        {
            // Feedback in [0, 0.7] to stay stable.
            //
            feedback = (rng.Next() * 0.5f + 0.5f) * 0.7f;
            modDepth = (rng.Next() * 0.5f + 0.5f) * 0.1f;
        }

        // Soft white noise input scaled to ~0.1.
        //
        float inSample = audioNoise.Next() * 0.1f;
        QuadFloat qin(inSample, inSample, inSample, inSample);

        QuadDelay::Input inp = MakeBasicInput(writePos, readPos, qin,
                                              feedback, modDepth);
        inp.m_return = previousOutput;
        QuadFloat out = qd.Process(inp);
        previousOutput = out;
        AdvanceHeads(writePos, readPos, kDelayLen);

        for (size_t ch = 0; ch < 4; ++ch)
        {
            float v = out[ch];
            if (std::isnan(v) || std::isinf(v))
            {
                anyNan = true;
            }
            else
            {
                maxAbs = std::max(maxAbs, std::abs(v));
            }
        }
    }

    DOCTEST_CHECK(maxAbs > 1e-4f);
    DOCTEST_INFO("stress maxAbs=" << maxAbs);
    DOCTEST_CHECK(!anyNan);
    // Generous bound: feedback < 1 so signal should not blow up beyond ±10.
    //
    DOCTEST_CHECK(maxAbs < 10.0f);
}

// ---------------------------------------------------------------------------
// 4. Stress: aggressive TheoryOfTime modulation (via TimeRig).
//    We skip full QuadDelayInputSetter wiring but still run the delay under
//    varying write/read positions that simulate tempo changes (by scaling the
//    position increment).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("QuadDelay: stress - varying write/read speed (ToT simulation)")
{
    GlobalEnv::ResetPerTest();

    auto qdPtr = std::make_unique<QuadDelay>();
    QuadDelay& qd = *qdPtr;

    TestSignal::WhiteNoise rng(0xFEEDFACEULL);
    TestSignal::WhiteNoise audioNoise(0xABCD1234ULL);

    const int N = 48000;  // 1 second

    // Start with a normal delay; every 512 samples change the "tempo"
    // by altering how fast the write head advances (simulates tempo ratio
    // changes of 0.5x to 2x).
    //
    double posIncrement = 1.0;
    double writeRaw = kDelayLen;

    float maxAbs = 0.0f;
    bool anyNan = false;
    QuadFloat previousOutput;

    for (int i = 0; i < N; ++i)
    {
        if (i % 512 == 0)
        {
            // Random speed multiplier in [0.5, 2.0].
            //
            float r = rng.Next() * 0.5f + 0.5f;  // in [0, 1]
            posIncrement = 0.5 + static_cast<double>(r) * 1.5;
        }

        writeRaw += posIncrement;
        // Read head is always kDelayLen behind the write head.
        //
        double readPos = writeRaw - kDelayLen;

        QuadDouble wp(writeRaw, writeRaw, writeRaw, writeRaw);
        QuadDouble rp(readPos,  readPos,  readPos,  readPos);

        float inSample = audioNoise.Next() * 0.05f;
        QuadFloat qin(inSample, inSample, inSample, inSample);

        QuadDelay::Input inp = MakeBasicInput(wp, rp, qin, 0.5f, 0.0f);
        inp.m_return = previousOutput;
        QuadFloat out = qd.Process(inp);
        previousOutput = out;

        for (size_t ch = 0; ch < 4; ++ch)
        {
            float v = out[ch];
            if (std::isnan(v) || std::isinf(v))
            {
                anyNan = true;
            }
            else
            {
                maxAbs = std::max(maxAbs, std::abs(v));
            }
        }
    }

    DOCTEST_CHECK(maxAbs > 1e-4f);
    DOCTEST_INFO("ToT-sim maxAbs=" << maxAbs);
    DOCTEST_CHECK(!anyNan);
    DOCTEST_CHECK(maxAbs < 10.0f);
}
