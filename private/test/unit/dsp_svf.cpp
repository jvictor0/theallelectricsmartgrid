// dsp_svf.cpp -- unit tests for LinearStateVariableFilter (private/src/StateVariableFilter.hpp)
//
// Trapezoidal (TPT/ZDF) SVF. Exposes static LP/HP/BP FrequencyResponse(g, k, freq).
// Note: Process(float) returns void; outputs are read via GetLowPass/GetHighPass/GetBandPass.
//
// Tests (LinearStateVariableFilter only -- NonlinearSVF is covered by dsp_ladder.cpp style
// if needed, but that is nonlinear; LinearSVF4PoleHP has its own check):
//   1. LP at low resonance: measured |H| matches analytic LowPassFrequencyResponse (1.5 dB tol).
//   2. HP at low resonance: measured matches analytic.
//   3. BP at low resonance: measured matches analytic.
//   4. High resonance: single sine at cutoff produces bounded output (no blow-up).
//   5. Parameter sweep: NaN-clean.
//   6. LinearSVF4PoleHP: passband near 0 dB above cutoff, measured vs analytic.

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/Continuity.hpp"
#include "../support/NanScan.hpp"

#include "StateVariableFilter.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize1024;
} // namespace

// ---------------------------------------------------------------------------
// Helper: measure LP/HP/BP transfer functions simultaneously for LinearSVF.
// ---------------------------------------------------------------------------
struct SVFOutputs { std::vector<float> LP, HP, BP; };

static SVFOutputs measureSVFTF(float cutoffCps, float resonance,
                                std::uint64_t seed, int frames = 64)
{
    const std::size_t N = kN;
    const std::size_t half = N / 2;

    LinearStateVariableFilter svf;
    svf.SetCutoff(cutoffCps);
    svf.SetResonance(resonance);

    std::vector<float> in(N), outLP(N), outHP(N), outBP(N);
    TestSignal::WhiteNoise noise(seed);

    std::vector<float> avgIn(half, 0.0f);
    std::vector<float> avgLP(half, 0.0f), avgHP(half, 0.0f), avgBP(half, 0.0f);

    // Warmup frame.
    for (std::size_t i = 0; i < N; ++i)
    {
        svf.Process(noise.Next());
    }

    for (int f = 0; f < frames; ++f)
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            float x = noise.Next();
            in[i] = x;
            svf.Process(x);
            outLP[i] = svf.GetLowPass();
            outHP[i] = svf.GetHighPass();
            outBP[i] = svf.GetBandPass();
        }
        auto mIn = MagnitudeSpectrum(in.data(),   N, kN);
        auto mLP = MagnitudeSpectrum(outLP.data(), N, kN);
        auto mHP = MagnitudeSpectrum(outHP.data(), N, kN);
        auto mBP = MagnitudeSpectrum(outBP.data(), N, kN);

        for (std::size_t k = 0; k < half; ++k)
        {
            avgIn[k] += mIn[k];
            avgLP[k] += mLP[k];
            avgHP[k] += mHP[k];
            avgBP[k] += mBP[k];
        }
    }

    SVFOutputs r;
    r.LP.resize(half); r.HP.resize(half); r.BP.resize(half);
    for (std::size_t k = 0; k < half; ++k)
    {
        r.LP[k] = (avgIn[k] > 1e-12f) ? avgLP[k] / avgIn[k] : 0.0f;
        r.HP[k] = (avgIn[k] > 1e-12f) ? avgHP[k] / avgIn[k] : 0.0f;
        r.BP[k] = (avgIn[k] > 1e-12f) ? avgBP[k] / avgIn[k] : 0.0f;
    }
    return r;
}

// ---------------------------------------------------------------------------
// 1. LP at low resonance matches analytic.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("LinearSVF: LP measured response matches analytic (1.5 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 2000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 0.0f;  // Butterworth-ish Q

    // Get g and k from a filter instance.
    LinearStateVariableFilter svf;
    svf.SetCutoff(cutoffCps);
    svf.SetResonance(resonance);
    float g = svf.m_g;
    float k = svf.m_k;

    auto outs = measureSVFTF(cutoffCps, resonance, 0x5AFE000100000001ull);

    auto expected = [g, k](double hz) -> double {
        return LinearStateVariableFilter::LowPassFrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    std::size_t lo = FreqBin(100.0,  kN, kSR);
    std::size_t hi = FreqBin(22000.0, kN, kSR);
    AssertResponseMatches(outs.LP, expected, kSR, kN, 1.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 2. HP at low resonance matches analytic.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("LinearSVF: HP measured response matches analytic (1.5 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 1000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 0.0f;

    LinearStateVariableFilter svf;
    svf.SetCutoff(cutoffCps);
    svf.SetResonance(resonance);
    float g = svf.m_g;
    float k = svf.m_k;

    auto outs = measureSVFTF(cutoffCps, resonance, 0x5AFE000200000001ull);

    auto expected = [g, k](double hz) -> double {
        return LinearStateVariableFilter::HighPassFrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    // Skip below ~300 Hz: deep HP stopband has < -40 dB expected gain,
    // Welch estimator noise floor causes > 1.5 dB error at these bins.
    std::size_t lo = FreqBin(300.0,  kN, kSR);
    std::size_t hi = FreqBin(22000.0, kN, kSR);
    AssertResponseMatches(outs.HP, expected, kSR, kN, 1.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 3. BP at low resonance matches analytic.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("LinearSVF: BP measured response matches analytic (2 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 3000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 0.0f;

    LinearStateVariableFilter svf;
    svf.SetCutoff(cutoffCps);
    svf.SetResonance(resonance);
    float g = svf.m_g;
    float k = svf.m_k;

    auto outs = measureSVFTF(cutoffCps, resonance, 0x5AFE000300000001ull);

    auto expected = [g, k](double hz) -> double {
        return LinearStateVariableFilter::BandPassFrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    std::size_t lo = FreqBin(200.0,  kN, kSR);
    std::size_t hi = FreqBin(20000.0, kN, kSR);
    AssertResponseMatches(outs.BP, expected, kSR, kN, 2.0, lo, hi);
}

// ---------------------------------------------------------------------------
// 4. High resonance: sustained sine at cutoff -- output stays bounded.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("LinearSVF: high resonance output bounded (stability)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 1000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 1.0f;  // max resonance

    LinearStateVariableFilter svf;
    svf.SetCutoff(cutoffCps);
    svf.SetResonance(resonance);

    // Drive with sine at cutoff for many samples.
    const std::size_t nSamples = 48000;
    TestSignal::Sine sine(cutoffHz, kSR, 0.1f);  // small amplitude sine

    std::vector<float> out(nSamples);
    float maxAbs = 0.0f;

    for (std::size_t i = 0; i < nSamples; ++i)
    {
        svf.Process(sine.Next());
        float lp = svf.GetLowPass();
        out[i] = lp;
        float a = std::abs(lp);
        if (a > maxAbs) maxAbs = a;
    }

    DOCTEST_INFO("max |LP output| at high resonance: " << maxAbs);
    // Must stay bounded; even resonant sine should be below amplitude 50.
    DOCTEST_CHECK(maxAbs < 50.0f);
    TestNan::AssertClean(out.data(), nSamples);
}

// ---------------------------------------------------------------------------
// 5. Parameter sweep: NaN-clean.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("LinearSVF: NaN-clean across parameter sweep")
{
    GlobalEnv::ResetPerTest();

    const std::size_t nSamples = 1024;
    TestSignal::WhiteNoise noise(0x5AFE000400000001ull);
    std::vector<float> out(nSamples);

    const float cutoffs[]   = { 0.001f, 0.01f, 0.1f, 0.25f, 0.45f };
    const float resonances[] = { 0.0f, 0.3f, 0.7f, 1.0f };

    for (float cps : cutoffs)
    {
        for (float res : resonances)
        {
            LinearStateVariableFilter svf;
            svf.SetCutoff(cps);
            svf.SetResonance(res);
            for (std::size_t i = 0; i < nSamples; ++i)
            {
                svf.Process(noise.Next());
                out[i] = svf.GetLowPass();
            }
            DOCTEST_INFO("cps=" << cps << " res=" << res);
            TestNan::AssertClean(out.data(), nSamples);
        }
    }
}

// ---------------------------------------------------------------------------
// 6. LinearSVF4PoleHP: passband near 0 dB above cutoff.
// ---------------------------------------------------------------------------
DOCTEST_TEST_CASE("LinearSVF4PoleHP: passband near 0 dB above cutoff")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 2000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    LinearSVF4PoleHighPass hp4;
    hp4.SetCutoff(cutoffCps);
    hp4.SetResonance(0.0f);

    float g = hp4.m_stage1.m_g;
    float k = hp4.m_stage1.m_k;

    std::function<float(float)> proc = [&hp4](float x) {
        hp4.Process(x);
        return hp4.GetOutput();
    };

    auto H = MeasureTransferFunction(proc, kN, kSR, 0x5AFE000500000001ull, 64, 2);

    auto expected = [g, k](double hz) -> double {
        return LinearSVF4PoleHighPass::FrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    // Skip below ~500 Hz where the 4-pole HP stopband is < -80 dB and
    // the Welch estimator noise floor prevents accurate comparison.
    std::size_t lo = FreqBin(500.0,  kN, kSR);
    std::size_t hi = FreqBin(22000.0, kN, kSR);
    AssertResponseMatches(H, expected, kSR, kN, 2.0, lo, hi);
}
