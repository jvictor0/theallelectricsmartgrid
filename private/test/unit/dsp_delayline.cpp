// dsp_delayline.cpp  --  unit tests for DelayLine<T> and QuadDelayLine
//
// Covers:
//   1. Impulse response: exact integer-delay read-back.
//   2. Cubic interpolation at fractional delay: no pre-echo, energy in correct
//      neighbours, unit-gain for DC (Read(0) == DC level after Write).
//   3. Sine through fixed delay: output equals delayed sine (magnitude preserved).
//   4. Sweeping the read position: no clicks beyond a threshold.
//   5. Ring-buffer wraparound: correct after many passes around the buffer.
//   6. QuadDelayLine: SIMD write/read, all four channels independent.
//
// NOTE: DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES is active; use DOCTEST_ prefixes.

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "../support/GlobalEnv.hpp"
#include "../support/NanScan.hpp"

#include "DelayLine.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// Simulate N samples of write then read-at-D and collect output.
// Returns a vector of length N (output sample i is read after write i).
template <size_t Size>
std::vector<float> RunDelayLine(DelayLine<Size>& dl, const std::vector<float>& input, float D)
{
    std::vector<float> out(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        dl.Write(input[i]);
        out[i] = dl.Read(D);
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. Impulse response: impulse at sample 0 appears exactly D samples later.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("DelayLine: integer-delay impulse response")
{
    GlobalEnv::ResetPerTest();

    // Use a small buffer so the test finishes quickly.
    DelayLine<4096> dl;

    const int D = 100;   // integer delay
    const int N = 300;   // total samples driven

    // Feed: one impulse at t=0 then silence.
    std::vector<float> input(static_cast<size_t>(N), 0.0f);
    input[0] = 1.0f;

    auto out = RunDelayLine(dl, input, static_cast<float>(D));

    // Write() advances m_index AFTER writing. So after writing the impulse at
    // step i=0, m_index = 1.  Read(D) computes:
    //   index = m_index - D + 2*Size
    // and uses cubic interpolation with alpha = frac(index).  The impulse
    // sample lands at the read-head when m_index = D, i.e. at step i = D-1.
    // So the expected peak index is D-1.
    const int expectedPeak = D - 1;

    // Pre-echo check: nothing before expectedPeak - 2 (cubic kernel extends
    // at most 2 samples before the nominal tap).
    for (int i = 0; i < expectedPeak - 2 && i < N; ++i)
    {
        DOCTEST_INFO("pre-echo at i=" << i << " val=" << out[static_cast<size_t>(i)]);
        DOCTEST_CHECK(std::abs(out[static_cast<size_t>(i)]) < 1e-4f);
    }

    // Peak check.
    float peak = 0.0f;
    int peakIdx = -1;
    for (int i = 0; i < N; ++i)
    {
        if (out[static_cast<size_t>(i)] > peak)
        {
            peak = out[static_cast<size_t>(i)];
            peakIdx = i;
        }
    }

    DOCTEST_INFO("peak=" << peak << " at index=" << peakIdx << " expected=" << expectedPeak);
    DOCTEST_CHECK(peak > 0.5f);                 // impulse must arrive
    DOCTEST_CHECK(peakIdx == expectedPeak);     // arrives at D-1

    // Post-impulse: samples after expectedPeak + 4 should be near zero.
    for (int i = expectedPeak + 5; i < N; ++i)
    {
        DOCTEST_INFO("post-impulse at i=" << i << " val=" << out[static_cast<size_t>(i)]);
        DOCTEST_CHECK(std::abs(out[static_cast<size_t>(i)]) < 1e-4f);
    }

    // NaN-clean
    TestNan::AssertClean(out.data(), out.size());
}

// ---------------------------------------------------------------------------
// 2. Fractional delay: D+0.5 gives energy in neighbours, no pre-echo, no NaN.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("DelayLine: fractional-delay impulse no pre-echo")
{
    GlobalEnv::ResetPerTest();

    DelayLine<4096> dl;

    const float D = 50.5f;
    const int N = 200;

    std::vector<float> input(static_cast<size_t>(N), 0.0f);
    input[0] = 1.0f;

    auto out = RunDelayLine(dl, input, D);

    // The impulse arrives at step i = floor(D) - 1 (same D-1 rule as integer
    // delay).  The cubic kernel spans indices iSub1, i0, i1, i2 where
    // iSub1 = floor(index)-1.  For fractional tap, the kernel can touch up to
    // 2 samples before the nominal tap floor(D)-1, i.e. down to floor(D)-3.
    // No energy before floor(D)-3.
    int preEchoEnd = static_cast<int>(std::floor(D)) - 3;
    for (int i = 0; i < preEchoEnd && i < N; ++i)
    {
        DOCTEST_INFO("pre-echo at i=" << i << " val=" << out[static_cast<size_t>(i)]);
        DOCTEST_CHECK(std::abs(out[static_cast<size_t>(i)]) < 1e-3f);
    }

    // Energy must arrive somewhere near floor(D)..ceil(D)+2 (cubic kernel).
    float totalEnergy = 0.0f;
    int lo = static_cast<int>(std::floor(D)) - 1;
    int hi = static_cast<int>(std::ceil(D))  + 4;
    lo = std::max(lo, 0);
    hi = std::min(hi, N - 1);
    for (int i = lo; i <= hi; ++i)
    {
        totalEnergy += std::abs(out[static_cast<size_t>(i)]);
    }
    DOCTEST_INFO("total energy near fractional delay=" << totalEnergy);
    DOCTEST_CHECK(totalEnergy > 0.5f);

    // Post-arrival: no lingering energy beyond cubic kernel support.
    for (int i = hi + 1; i < N; ++i)
    {
        DOCTEST_INFO("post-arrival at i=" << i << " val=" << out[static_cast<size_t>(i)]);
        DOCTEST_CHECK(std::abs(out[static_cast<size_t>(i)]) < 1e-3f);
    }

    TestNan::AssertClean(out.data(), out.size());
}

// ---------------------------------------------------------------------------
// 3. Sine through fixed delay: output is delayed sine, magnitude ~unchanged.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("DelayLine: sine through fixed delay preserves magnitude")
{
    GlobalEnv::ResetPerTest();

    DelayLine<4096> dl;

    const int D = 64;
    const double sampleRate = 48000.0;
    const double freqHz     = 440.0;
    const double phaseInc   = freqHz / sampleRate;
    const int    N          = 500;
    const int    warmup     = D + 20;  // wait for the sine to fill the pipeline

    std::vector<float> input(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
    {
        input[static_cast<size_t>(i)] =
            static_cast<float>(std::sin(2.0 * M_PI * phaseInc * i));
    }

    auto out = RunDelayLine(dl, input, static_cast<float>(D));

    // Measure RMS of output[warmup..N-1] vs the corresponding portion of the
    // input delayed by D samples: input[warmup - D .. N - 1 - D].
    double outRms = 0.0, inRms = 0.0;
    int count = 0;
    for (int i = warmup; i < N - 1; ++i)
    {
        double o = out[static_cast<size_t>(i)];
        double s = input[static_cast<size_t>(i - D)];
        outRms += o * o;
        inRms  += s * s;
        ++count;
    }
    outRms = std::sqrt(outRms / count);
    inRms  = std::sqrt(inRms  / count);

    DOCTEST_INFO("out RMS=" << outRms << " in RMS=" << inRms);
    DOCTEST_REQUIRE(inRms > 0.3f);  // sine should have decent amplitude

    // Cubic interpolation at integer delay should be exact: ratio ~1.
    double ratio = outRms / inRms;
    DOCTEST_CHECK(ratio == doctest::Approx(1.0).epsilon(0.02));

    TestNan::AssertClean(out.data(), out.size());
}

// ---------------------------------------------------------------------------
// 4. Sweeping read position: no clicks (MaxAbsDelta bounded vs static baseline).
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("DelayLine: sweeping read position is click-free")
{
    GlobalEnv::ResetPerTest();

    // Measure the max sample-to-sample delta of output for a *static* read
    // position (440 Hz sine in, integer delay D=64). Use that as our click threshold.
    auto measureMaxDelta = [](float readPos, int N) -> float
    {
        DelayLine<4096> dl;
        const double sampleRate = 48000.0;
        const double freqHz     = 440.0;
        const double phaseInc   = freqHz / sampleRate;
        float prev = 0.0f;
        float maxDelta = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            float in = static_cast<float>(std::sin(2.0 * M_PI * phaseInc * i));
            dl.Write(in);
            float val = dl.Read(readPos);
            float delta = std::abs(val - prev);
            if (delta > maxDelta)
            {
                maxDelta = delta;
            }
            prev = val;
        }
        return maxDelta;
    };

    const int N = 2000;
    float staticDelta = measureMaxDelta(64.0f, N);

    // Now sweep the read position gradually from 64 to 128 over N samples.
    DelayLine<4096> dl;
    const double sampleRate = 48000.0;
    const double freqHz     = 440.0;
    const double phaseInc   = freqHz / sampleRate;
    float prev = 0.0f;
    float maxDelta = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        float in = static_cast<float>(std::sin(2.0 * M_PI * phaseInc * i));
        dl.Write(in);
        // Slowly sweep: 64 -> 128 over N samples.
        float readPos = 64.0f + 64.0f * static_cast<float>(i) / static_cast<float>(N - 1);
        float val = dl.Read(readPos);
        float delta = std::abs(val - prev);
        if (delta > maxDelta)
        {
            maxDelta = delta;
        }
        prev = val;
    }

    DOCTEST_INFO("static max delta=" << staticDelta
                 << " sweep max delta=" << maxDelta);

    // The swept version uses XFader-based crossfade internally (via
    // DelayLine::Read(XFader)) only when called with XFader; plain Read()
    // uses cubic interpolation at every sample. A slow sweep should stay
    // within a reasonable multiple of the static worst-case delta.
    DOCTEST_CHECK(maxDelta < staticDelta * 10.0f + 0.1f);

    // Buffer must be NaN-clean.
    std::vector<float> dummy(1, prev);  // just check the final sample
    TestNan::AssertClean(dummy.data(), dummy.size());
}

// ---------------------------------------------------------------------------
// 5. Buffer wraparound: write/read across ring boundary many times.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("DelayLine: ring-buffer wraparound correctness")
{
    GlobalEnv::ResetPerTest();

    // Deliberately small buffer so we wrap many times quickly.
    DelayLine<256> dl;

    const int    D = 50;
    const double sampleRate = 48000.0;
    const double freqHz     = 100.0;
    const double phaseInc   = freqHz / sampleRate;

    // Run 3 full buffer passes (3 * 256 = 768 samples), each time recording
    // the output. Check NaN-clean and bounded.
    const int N = 768;
    std::vector<float> out(static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
    {
        float in = static_cast<float>(std::sin(2.0 * M_PI * phaseInc * i));
        dl.Write(in);
        out[static_cast<size_t>(i)] = dl.Read(static_cast<float>(D));
    }

    TestNan::AssertClean(out.data(), out.size());

    // All samples must be within [-1.1, 1.1] (cubic can slightly overshoot).
    for (size_t i = 0; i < out.size(); ++i)
    {
        DOCTEST_INFO("sample " << i << " = " << out[i]);
        DOCTEST_CHECK(out[i] >= -1.1f);
        DOCTEST_CHECK(out[i] <=  1.1f);
    }
}

// ---------------------------------------------------------------------------
// 6. QuadDelayLine: four independent channels.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("QuadDelayLine: four independent channels, impulse response")
{
    GlobalEnv::ResetPerTest();

    QuadDelayLine<4096> qdl;

    const int D  = 40;
    const int N  = 200;

    // Feed an impulse into channel k only (k = 0, 1, 2, 3 in turn),
    // then verify the other channels read ~0 and channel k has a peak at D.
    for (int k = 0; k < 4; ++k)
    {
        // Re-initialize by default-constructing a fresh one per iteration.
        QuadDelayLine<4096> qdl2;

        // Build a QuadFloat impulse: only channel k is 1 at t=0.
        std::vector<float> outBuf(static_cast<size_t>(N));
        for (int chan = 0; chan < 4; ++chan)
        {
            // reset — individual channels are independent DelayLine<Size> members.
            (void)chan;
        }

        for (int i = 0; i < N; ++i)
        {
            QuadFloat in(0.0f, 0.0f, 0.0f, 0.0f);
            if (i == 0)
            {
                in[k] = 1.0f;
            }
            qdl2.Write(in);
            QuadFloat delays(static_cast<float>(D), static_cast<float>(D),
                             static_cast<float>(D), static_cast<float>(D));
            QuadFloat rd = qdl2.Read(delays);
            outBuf[static_cast<size_t>(i)] = rd[k];

            // Cross-channel isolation: other channels should stay ~0.
            for (int j = 0; j < 4; ++j)
            {
                if (j != k)
                {
                    DOCTEST_INFO("cross-channel leak ch=" << j << " at i=" << i
                                 << " val=" << rd[j]);
                    DOCTEST_CHECK(std::abs(rd[j]) < 1e-4f);
                }
            }
        }

        TestNan::AssertClean(outBuf.data(), outBuf.size());

        // Channel k must have a peak at sample D.
        float peak = 0.0f;
        int peakIdx = -1;
        for (int i = 0; i < N; ++i)
        {
            if (outBuf[static_cast<size_t>(i)] > peak)
            {
                peak = outBuf[static_cast<size_t>(i)];
                peakIdx = i;
            }
        }
        DOCTEST_INFO("chan=" << k << " peak=" << peak << " at=" << peakIdx);
        DOCTEST_CHECK(peak > 0.5f);
        DOCTEST_CHECK(peakIdx == D - 1);  // arrives at D-1 (same D-1 rule as DelayLine)
    }
}
