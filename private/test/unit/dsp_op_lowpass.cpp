// dsp_op_lowpass.cpp -- unit tests for OPLowPassFilter (private/src/Filter.hpp)
//
// One-pole IIR: y[n] = alpha*x[n] + (1-alpha)*y[n-1]
// Transfer function: H(z) = alpha / (1 - (1-alpha)*z^-1)
// The class exposes its own static FrequencyResponse(alpha, freq) method
// (freq in cycles per sample) -- we compare measured |H| to that.
//
// Tests:
//   1. DC gain is ~1 for a wide range of alphas.
//   2. Measured |H| matches FrequencyResponse over mid-band bins (tol 1.5 dB).
//   3. Monotone rolloff (each octave in the stop band is lower than the previous).
//   4. NaN-clean under extreme alphas (0-epsilon, 1, tiny positive).
//   5. SetAlphaFromNatFreq: measured -3 dB point near the specified cutoff.

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/NanScan.hpp"

#include "Filter.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize1024;

// Analytic |H(f)| for the one-pole LP (freq = cycles per sample in [0, 0.5)).
double analyticLP(float alpha, double normFreq)
{
    return static_cast<double>(OPLowPassFilter::FrequencyResponse(alpha, static_cast<float>(normFreq)));
}

} // namespace

// ---------------------------------------------------------------------------
// 1. DC gain is ~1 for various alphas.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("OPLowPassFilter: DC gain near 1 for valid alphas")
{
    GlobalEnv::ResetPerTest();

    // DC input: feed 1.0 for many samples and read settled output.
    const std::size_t warmup = 8192;
    for (float alpha : {0.01f, 0.1f, 0.5f, 0.9f, 0.99f})
    {
        OPLowPassFilter f;
        f.m_alpha = alpha;
        float out = 0.0f;
        for (std::size_t i = 0; i < warmup; ++i)
        {
            out = f.Process(1.0f);
        }
        DOCTEST_INFO("alpha=" << alpha << " DC output=" << out);
        DOCTEST_CHECK(out == doctest::Approx(1.0f).epsilon(0.01));
    }
}

// ---------------------------------------------------------------------------
// 2. Measured |H| matches analytic over mid-band bins for several alphas.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("OPLowPassFilter: measured response matches analytic (1.5 dB tol)")
{
    GlobalEnv::ResetPerTest();

    // Test three different alphas covering low / mid / high cutoffs.
    // alpha ~ 1 - exp(-2*pi*f_c) gives f_c ~ alpha/(2*pi) for small alpha.
    struct TestCase { float alpha; std::size_t loBin; std::size_t hiBin; };
    const TestCase cases[] = {
        { 0.05f, 3, 300 },   // very low cutoff -- passband + much of stopband
        { 0.3f,  5, 400 },   // mid cutoff
        { 0.7f,  8, 460 },   // higher cutoff
    };

    for (auto& tc : cases)
    {
        OPLowPassFilter filt;
        filt.m_alpha = tc.alpha;

        std::function<float(float)> proc = [&filt](float x) {
            return filt.Process(x);
        };

        auto H = MeasureTransferFunction(proc, kN, kSR, 0xA11C0DE, 64, 2);

        auto expected = [&tc](double hz) -> double {
            double normFreq = hz / kSR;
            return analyticLP(tc.alpha, normFreq);
        };

        DOCTEST_INFO("alpha=" << tc.alpha);
        AssertResponseMatches(H, expected, kSR, kN, 1.5, tc.loBin, tc.hiBin);
    }
}

// ---------------------------------------------------------------------------
// 3. Monotone rolloff: response decreases across octaves in the stopband.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("OPLowPassFilter: monotone rolloff")
{
    GlobalEnv::ResetPerTest();

    // Use alpha=0.1 (low cutoff ~700 Hz).
    OPLowPassFilter filt;
    filt.m_alpha = 0.1f;

    std::function<float(float)> proc = [&filt](float x) {
        return filt.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xB0077019, 64, 2);

    // At normalised freqs 0.05, 0.1, 0.2, 0.4 we expect |H| strictly decreasing.
    const double freqs[] = {0.01, 0.05, 0.1, 0.2, 0.4};
    float prev = 2.0f;
    for (double f : freqs)
    {
        std::size_t bin = FreqBin(f * kSR, kN, kSR);
        if (bin >= H.size()) break;
        float cur = H[bin];
        DOCTEST_INFO("normFreq=" << f << " bin=" << bin << " |H|=" << cur);
        DOCTEST_CHECK(cur < prev);
        prev = cur;
    }
}

// ---------------------------------------------------------------------------
// 4. NaN-clean under extreme alphas.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("OPLowPassFilter: NaN-clean under extreme alphas")
{
    GlobalEnv::ResetPerTest();

    const std::size_t nSamples = 1024;
    std::vector<float> out(nSamples);
    TestSignal::WhiteNoise noise(0xDEAD);

    for (float alpha : {1e-6f, 0.5f, 1.0f})
    {
        OPLowPassFilter f;
        f.m_alpha = alpha;
        for (std::size_t i = 0; i < nSamples; ++i)
        {
            out[i] = f.Process(noise.Next());
        }
        DOCTEST_INFO("alpha=" << alpha);
        TestNan::AssertClean(out.data(), nSamples);
    }
}

// ---------------------------------------------------------------------------
// 5. SetAlphaFromNatFreq: -3 dB point near specified cutoff.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("OPLowPassFilter: SetAlphaFromNatFreq places -3dB correctly")
{
    GlobalEnv::ResetPerTest();

    // Cutoff at 1000 Hz -> cyclesPerSample = 1000/48000.
    const float cutoffHz = 1000.0f;
    const float cps = cutoffHz / static_cast<float>(kSR);

    OPLowPassFilter filt;
    filt.SetAlphaFromNatFreq(cps);

    std::function<float(float)> proc = [&filt](float x) {
        return filt.Process(x);
    };
    auto H = MeasureTransferFunction(proc, kN, kSR, 0xC00FF1C000000001ull, 64, 2);

    // DC bin should be near 1.0 (0 dB).
    // Bin at cutoff should be near 0.707 (-3 dB).
    std::size_t dcBin = FreqBin(10.0, kN, kSR);         // ~10 Hz as proxy for DC
    std::size_t fcBin = FreqBin(cutoffHz, kN, kSR);

    DOCTEST_INFO("DC bin " << dcBin << " |H|=" << H[dcBin]);
    DOCTEST_CHECK(H[dcBin] > 0.9f);  // passband gain close to 1

    float hAtCutoff = H[fcBin];
    DOCTEST_INFO("cutoff bin " << fcBin << " (f=" << cutoffHz << " Hz) |H|=" << hAtCutoff);
    // Allow ±3 dB around the theoretical -3 dB point (0.707).
    DOCTEST_CHECK(hAtCutoff > 0.4f);
    DOCTEST_CHECK(hAtCutoff < 0.9f);
}
