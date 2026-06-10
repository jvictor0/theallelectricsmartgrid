// dsp_comb.cpp -- unit tests for CombFilterWithOnePole (private/src/CombFilterWithOnePole.hpp)
//
// Comb filter with one-pole LP in the feedback loop.
// Signal flow: output = input + feedback * OnePole(delay[compensatedDelay])
//
// Note: CombFilterWithOnePole uses ParamSlew internally -- SetParams() sets
// targets that ramp toward their values over time. We use enough warmup samples
// to let the slews settle before measuring.
//
// Tests:
//   1. Zero-feedback: output equals input (allpass).
//   2. Positive feedback: peaks at harmonic multiples of fundamental.
//   3. NaN-clean under various configurations.
//   4. Negative feedback: notches at odd harmonics (sign inversion).

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/NanScan.hpp"

#include "CombFilterWithOnePole.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize4096;  // larger FFT for better frequency resolution

// Large warmup to let all slews settle after SetParams.
constexpr std::size_t kWarmup = 48000;
} // namespace

// ---------------------------------------------------------------------------
// Helper: measure transfer function of CombFilterWithOnePole.
// Returns H[k] = avgOut[k] / avgIn[k].
// ---------------------------------------------------------------------------
static std::vector<float>
measureCombTF(float combFreqNorm, float alpha, float feedback,
              std::uint64_t seed, int frames = 32)
{
    const std::size_t N = kN;
    const std::size_t half = N / 2;

    CombFilterWithOnePole comb;
    comb.SetParams(combFreqNorm, alpha);
    comb.m_feedbackSlew.Update(feedback);
    // Force slews to settle immediately.
    comb.m_feedbackSlew.m_filter.m_output = feedback;
    comb.m_compensatedDelaySlew.m_filter.m_output = comb.m_compensatedDelaySlew.m_target;

    std::vector<float> in(N), out(N);
    TestSignal::WhiteNoise noise(seed);

    // Warmup.
    for (std::size_t i = 0; i < kWarmup; ++i)
    {
        float x = noise.Next();
        comb.Process(x);
    }

    std::vector<float> avgIn(half, 0.0f), avgOut(half, 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            float x = noise.Next();
            in[i]  = x;
            out[i] = comb.Process(x);
        }
        auto mIn  = MagnitudeSpectrum(in.data(),  N, kN);
        auto mOut = MagnitudeSpectrum(out.data(), N, kN);
        for (std::size_t k = 0; k < half; ++k)
        {
            avgIn[k]  += mIn[k];
            avgOut[k] += mOut[k];
        }
    }

    std::vector<float> H(half);
    for (std::size_t k = 0; k < half; ++k)
    {
        H[k] = (avgIn[k] > 1e-12f) ? avgOut[k] / avgIn[k] : 0.0f;
    }
    return H;
}

// ---------------------------------------------------------------------------
// 1. Zero feedback: output is effectively allpass (flat or near-flat).
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("CombFilterWithOnePole: zero feedback is pass-through")
{
    GlobalEnv::ResetPerTest();

    const float combFreq = 100.0f / static_cast<float>(kSR);  // ~100 Hz
    const float alpha    = 0.5f;
    const float feedback = 0.0f;

    auto H = measureCombTF(combFreq, alpha, feedback, 0xC0B1C2D300000001ull);

    // With zero feedback the one-pole LP in the feedback path is inactive;
    // output = input + 0 * ... = input. Expect near-unity gain.
    std::size_t lo = FreqBin(200.0,  kN, kSR);
    std::size_t hi = FreqBin(18000.0, kN, kSR);
    AssertResponseMatches(H, [](double) { return 1.0; }, kSR, kN, 1.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 2. Positive feedback: peaks at harmonic multiples of fundamental.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("CombFilterWithOnePole: peaks at harmonics for positive feedback")
{
    GlobalEnv::ResetPerTest();

    // Fundamental at ~200 Hz; 5 harmonics in the first 1 kHz.
    const float fundHz   = 200.0f;
    const float combFreq = fundHz / static_cast<float>(kSR);
    const float alpha    = 0.9f;   // strong damping
    const float feedback = 0.8f;

    auto H = measureCombTF(combFreq, alpha, feedback, 0xC0B1C2D300000002ull, 48);

    // For positive feedback the comb resonates at multiples of combFreq.
    // Check that bins near the first 3 harmonics (200, 400, 600 Hz) are louder
    // than bins between them.
    const float harmonics[] = { fundHz, 2 * fundHz, 3 * fundHz };
    const float midpoints[]  = { 1.5f * fundHz, 2.5f * fundHz };

    for (float hf : harmonics)
    {
        std::size_t hBin = FreqBin(hf, kN, kSR);
        // Neighbourhood max to handle slight bin offset from delay compensation.
        float peakMag = 0.0f;
        for (int d = -3; d <= 3; ++d)
        {
            std::size_t b = static_cast<std::size_t>(static_cast<int>(hBin) + d);
            if (b < H.size()) peakMag = std::max(peakMag, H[b]);
        }

        for (float mf : midpoints)
        {
            if (std::fabs(mf - hf) < fundHz * 0.3f) continue;
            std::size_t mBin = FreqBin(mf, kN, kSR);
            float midMag = (mBin < H.size()) ? H[mBin] : 0.0f;
            DOCTEST_INFO("harmonic " << hf << " Hz: peakMag=" << peakMag
                         << "  midpoint " << mf << " Hz: midMag=" << midMag);
            DOCTEST_CHECK(peakMag > midMag);
        }
    }
}

// ---------------------------------------------------------------------------
// 3. NaN-clean under various configurations.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("CombFilterWithOnePole: NaN-clean under varied parameters")
{
    GlobalEnv::ResetPerTest();

    const std::size_t nSamples = 4096;
    TestSignal::WhiteNoise noise(0xC0B1C2D300000003ull);
    std::vector<float> out(nSamples);

    struct Params { float combFreq; float alpha; float feedback; };
    const Params configs[] = {
        { 200.0f / 48000.0f,  0.9f,  0.8f },
        { 500.0f / 48000.0f,  0.5f,  0.5f },
        { 100.0f / 48000.0f,  0.3f, -0.7f },  // negative feedback
        { 1000.0f / 48000.0f, 0.99f, 0.95f }, // high feedback
    };

    for (auto& p : configs)
    {
        CombFilterWithOnePole comb;
        comb.SetParams(p.combFreq, p.alpha);
        comb.m_feedbackSlew.Update(p.feedback);
        comb.m_feedbackSlew.m_filter.m_output = p.feedback;
        comb.m_compensatedDelaySlew.m_filter.m_output = comb.m_compensatedDelaySlew.m_target;

        for (std::size_t i = 0; i < nSamples; ++i)
        {
            out[i] = comb.Process(noise.Next());
        }
        DOCTEST_INFO("combFreq=" << p.combFreq << " alpha=" << p.alpha
                     << " fb=" << p.feedback);
        TestNan::AssertClean(out.data(), nSamples);
    }
}

// ---------------------------------------------------------------------------
// 4. Negative feedback: notches at odd harmonics relative to a positive-fb comb.
// This tests that sign inversion changes the peak/notch pattern.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("CombFilterWithOnePole: negative feedback creates notch pattern")
{
    GlobalEnv::ResetPerTest();

    const float fundHz   = 300.0f;
    const float combFreq = fundHz / static_cast<float>(kSR);
    const float alpha    = 0.8f;
    const float feedbackPos = 0.85f;
    const float feedbackNeg = -0.85f;

    auto Hpos = measureCombTF(combFreq, alpha, feedbackPos, 0xC0B1C2D30000004Aull, 32);
    auto Hneg = measureCombTF(combFreq, alpha, feedbackNeg, 0xC0B1C2D30000004Bull, 32);

    // At the fundamental (1st harmonic), positive feedback peaks and negative notches.
    std::size_t bin1 = FreqBin(fundHz, kN, kSR);
    // Neighbourhood max for pos.
    float posNearFund = 0.0f, negNearFund = 0.0f;
    for (int d = -4; d <= 4; ++d)
    {
        std::size_t b = static_cast<std::size_t>(static_cast<int>(bin1) + d);
        if (b < Hpos.size())
        {
            posNearFund = std::max(posNearFund, Hpos[b]);
            negNearFund = std::max(negNearFund, Hneg[b]);
        }
    }
    DOCTEST_INFO("At fund: positive fb |H|=" << posNearFund << "  negative fb |H|=" << negNearFund);
    // Positive feedback should boost the fundamental more than negative.
    DOCTEST_CHECK(posNearFund > negNearFund * 1.5f);
}
