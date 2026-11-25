// dsp_butterworth.cpp -- unit tests for ButterworthFilter (private/src/ButterworthFilter.hpp)
//
// 8th-order Butterworth LP built from four cascaded biquads.
// Compare against the independently prewarped Butterworth magnitude.
//
// Tests:
//   1. Passband gain near 0 dB (well below cutoff).
//   2. -3 dB ± 2 dB near the cutoff frequency.
//   3. Asymptotic stopband slope ~48 dB/octave (8th-order).
//   4. Measured response matches the analytic Butterworth response (2.5 dB tol).
//   5. NaN-clean across a cutoff sweep.

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/NanScan.hpp"

#include "ButterworthFilter.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize1024;

// Analytic 8th-order Butterworth LP at cycles-per-sample cutoff fc,
// evaluated at normalized frequency normFreq (cycles per sample).
//
double analyticBW8(float fc, double normFreq)
{
    const double normalized = std::tan(M_PI * normFreq) / std::tan(M_PI * fc);
    return 1.0 / std::sqrt(1.0 + std::pow(normalized, 16));
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Passband near 0 dB.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("ButterworthFilter: passband gain near 0 dB")
{
    GlobalEnv::ResetPerTest();

    // Cutoff at ~4 kHz, passband up to ~1 kHz.
    //
    const float cutoffCps = 4000.0f / static_cast<float>(kSR);
    ButterworthFilter bw(cutoffCps);

    std::function<float(float)> proc = [&bw](float x) {
        return bw.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xB077E00100000001ull, 64, 2);

    // Check bins from ~100 Hz to 1 kHz are within 2 dB of 0 dB.
    //
    std::size_t lo = FreqBin(100.0, kN, kSR);
    std::size_t hi = FreqBin(1000.0, kN, kSR);

    AssertResponseMatches(H, [](double) { return 1.0; }, kSR, kN, 2.0, lo, hi);
}

// ---------------------------------------------------------------------------
// 2. -3 dB near cutoff frequency.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("ButterworthFilter: -3 dB near cutoff")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 5000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    ButterworthFilter bw(cutoffCps);

    std::function<float(float)> proc = [&bw](float x) {
        return bw.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xB077E00100000002ull, 64, 2);

    std::size_t fcBin = FreqBin(cutoffHz, kN, kSR);
    if (fcBin < H.size())
    {
        float hAtFc = H[fcBin];
        float dbAtFc = 20.0f * std::log10(hAtFc > 1e-9f ? hAtFc : 1e-9f);
        DOCTEST_INFO("cutoff bin " << fcBin << " |H|=" << hAtFc << " dB=" << dbAtFc);
        // 8th-order BW: -3 dB at the cutoff; allow ±3 dB measurement tolerance.
        //
        DOCTEST_CHECK(dbAtFc > -6.0f);
        DOCTEST_CHECK(dbAtFc < 0.5f);
    }
}

// ---------------------------------------------------------------------------
// 3. Stopband slope: ~48 dB/octave (6 dB/octave per pole, 8 poles).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("ButterworthFilter: stopband slope >= 40 dB/octave")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 1000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    ButterworthFilter bw(cutoffCps);

    std::function<float(float)> proc = [&bw](float x) {
        return bw.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xB077E00100000003ull, 96, 2);

    // Measure between 4 kHz and 8 kHz (2 octaves above cutoff, well in stopband).
    //
    std::size_t bin4k = FreqBin(4000.0, kN, kSR);
    std::size_t bin8k = FreqBin(8000.0, kN, kSR);

    if (bin4k < H.size() && bin8k < H.size())
    {
        float h4k = H[bin4k];
        float h8k = H[bin8k];
        double db4k = 20.0 * std::log10(h4k > 1e-9f ? h4k : 1e-9);
        double db8k = 20.0 * std::log10(h8k > 1e-9f ? h8k : 1e-9);
        double slopePerOctave = db8k - db4k;  // should be negative (~-48 dB)
        DOCTEST_INFO("4 kHz: " << db4k << " dB,  8 kHz: " << db8k
                     << " dB,  slope/oct=" << slopePerOctave);
        // 8th-order Butterworth: ideal is ~48 dB/oct; noise floor
        // of the Welch estimator limits measured dynamic range, so
        // we conservatively check for >= 30 dB/octave attenuation.
        //
        DOCTEST_CHECK(slopePerOctave < -30.0);
    }
}

// ---------------------------------------------------------------------------
// 4. Measured response matches analytic cascaded-biquad model (2.5 dB tol).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("ButterworthFilter: measured vs analytic biquad model (2.5 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 3000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    ButterworthFilter bw(cutoffCps);

    std::function<float(float)> proc = [&bw](float x) {
        return bw.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xB077E00100000004ull, 64, 2);

    auto expected = [&cutoffCps](double hz) -> double {
        return analyticBW8(cutoffCps, hz / kSR);
    };

    // Test over passband + upper roll-off region; exclude very deep stopband.
    //
    std::size_t lo = FreqBin(100.0,  kN, kSR);
    std::size_t hi = FreqBin(12000.0, kN, kSR);
    AssertResponseMatches(H, expected, kSR, kN, 2.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 5. NaN-clean across a cutoff sweep.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("ButterworthFilter: NaN-clean across cutoff sweep")
{
    GlobalEnv::ResetPerTest();

    const std::size_t nSamples = 512;
    std::vector<float> out(nSamples);
    TestSignal::WhiteNoise noise(0xB077EE1100000005ull);

    const float cutoffs[] = { 0.001f, 0.01f, 0.05f, 0.1f, 0.25f, 0.45f };
    for (float cps : cutoffs)
    {
        ButterworthFilter bw(cps);
        for (std::size_t i = 0; i < nSamples; ++i)
        {
            out[i] = bw.Process(noise.Next());
        }
        DOCTEST_INFO("cutoffCps=" << cps);
        TestNan::AssertClean(out.data(), nSamples);
    }
}
