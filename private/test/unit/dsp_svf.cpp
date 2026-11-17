#include "doctest.h"
#include "TrioUtils.hpp"

#include <cmath>
#include <vector>
#include <functional>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/Continuity.hpp"
#include "../support/NanScan.hpp"

#include "TPTSVFFilter.hpp"
#include "../support/SVFReference.hpp"

namespace
{
float MorphOutput(float blend)
{
    MorphingTPTSVFFilterGeneric<float, 1> filter;
    filter.SetCutoffResonanceAndMorph(0.1f, 0.0f, blend);
    // Arrange one step with LP=2, BP=3 and HP=5 to isolate the morph kernel.
    //
    auto& state = filter.m_state.m_svf;
    state.m_a1 = 1;
    state.m_a2 = 0;
    state.m_a3 = 0;
    state.m_ic1 = 3;
    state.m_ic2 = 2;
    state.m_k = 1;
    filter.Process(10);
    return filter.GetOutput()[0];
}
}

DOCTEST_TEST_CASE("Generated SVF: morph follows positive quarter-circle LP BP HP weights")
{
    const float x_blends[] = {-0.2f, 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f, 1.2f};
    const float x_outputs[] = {2.0f, 2.0f, 2.99580936f, 3.535533906f, 3.53700546f, 3.0f, 4.68505576f, 5.65685425f, 5.76744796f, 5.0f, 5.0f};
    for (size_t i = 0; i < sizeof(x_blends) / sizeof(x_blends[0]); ++i)
    {
        DOCTEST_INFO("blend=" << x_blends[i]);
        DOCTEST_CHECK(MorphOutput(x_blends[i]) == doctest::Approx(x_outputs[i]).epsilon(0.00001));
    }
}

DOCTEST_TEST_CASE("Generated SVF: morph is continuous through the band-pass midpoint")
{
    DOCTEST_CHECK(MorphOutput(0.499999f) == doctest::Approx(3.0f).epsilon(0.00001));
    DOCTEST_CHECK(MorphOutput(0.500001f) == doctest::Approx(3.0f).epsilon(0.00001));
}

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
//
struct SVFOutputs { std::vector<float> LP, HP, BP; };

static SVFOutputs measureSVFTF(float cutoffCps, float resonance,
                                std::uint64_t seed, int frames)
{
    const std::size_t N = kN;
    const std::size_t half = N / 2;

    TPTSVFLowPassFilterGeneric<float, 1> svf;
    svf.SetCutoffAndResonance(cutoffCps, resonance);

    std::vector<float> in(N), outLP(N), outHP(N), outBP(N);
    TestSignal::WhiteNoise noise(seed);

    std::vector<float> avgIn(half, 0.0f);
    std::vector<float> avgLP(half, 0.0f), avgHP(half, 0.0f), avgBP(half, 0.0f);

    // Warmup frame.
    //
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
//
DOCTEST_TEST_CASE("Generated SVF: LP measured response matches analytic (1.5 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 2000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 0.0f;

    // Get g and k from a filter instance.
    //
    TPTSVFLowPassFilterGeneric<float, 1> svf;
    svf.SetCutoffAndResonance(cutoffCps, resonance);
    float g = svf.MakeSnapshot().m_g[0];
    float k = svf.MakeSnapshot().m_k[0];

    auto outs = measureSVFTF(cutoffCps, resonance, 0x5AFE000100000001ull, 64);

    auto expected = [g, k](double hz) -> double {
        return TestSVF::LowPassFrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    std::size_t lo = FreqBin(100.0,  kN, kSR);
    std::size_t hi = FreqBin(22000.0, kN, kSR);
    AssertResponseMatches(outs.LP, expected, kSR, kN, 1.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 2. HP at low resonance matches analytic.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("Generated SVF: HP measured response matches analytic (1.5 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 1000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 0.0f;

    TPTSVFLowPassFilterGeneric<float, 1> svf;
    svf.SetCutoffAndResonance(cutoffCps, resonance);
    float g = svf.MakeSnapshot().m_g[0];
    float k = svf.MakeSnapshot().m_k[0];

    auto outs = measureSVFTF(cutoffCps, resonance, 0x5AFE000200000001ull, 64);

    auto expected = [g, k](double hz) -> double {
        return TestSVF::HighPassFrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    // Skip below ~300 Hz: deep HP stopband has < -40 dB expected gain,
    // Welch estimator noise floor causes > 1.5 dB error at these bins.
    //
    std::size_t lo = FreqBin(300.0,  kN, kSR);
    std::size_t hi = FreqBin(22000.0, kN, kSR);
    AssertResponseMatches(outs.HP, expected, kSR, kN, 1.5, lo, hi);
}

// ---------------------------------------------------------------------------
// 3. BP at low resonance matches analytic.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("Generated SVF: BP measured response matches analytic (2 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 3000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 0.0f;

    TPTSVFLowPassFilterGeneric<float, 1> svf;
    svf.SetCutoffAndResonance(cutoffCps, resonance);
    float g = svf.MakeSnapshot().m_g[0];
    float k = svf.MakeSnapshot().m_k[0];

    auto outs = measureSVFTF(cutoffCps, resonance, 0x5AFE000300000001ull, 64);

    auto expected = [g, k](double hz) -> double {
        return TestSVF::BandPassFrequencyResponse(
            g, k, static_cast<float>(hz / kSR));
    };

    std::size_t lo = FreqBin(200.0,  kN, kSR);
    std::size_t hi = FreqBin(20000.0, kN, kSR);
    AssertResponseMatches(outs.BP, expected, kSR, kN, 2.0, lo, hi);
}

// ---------------------------------------------------------------------------
// 4. High resonance: sustained sine at cutoff -- output stays bounded.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("Generated SVF: high resonance output bounded (stability)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 1000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);
    const float resonance = 1.0f;

    TPTSVFLowPassFilterGeneric<float, 1> svf;
    svf.SetCutoffAndResonance(cutoffCps, resonance);

    // Drive with sine at cutoff for many samples.
    //
    const std::size_t nSamples = 48000;
    TestSignal::Sine sine(cutoffHz, kSR, 0.1f);

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
    //
    DOCTEST_CHECK(maxAbs < 50.0f);
    TestNan::AssertClean(out.data(), nSamples);
}

// ---------------------------------------------------------------------------
// 5. Parameter sweep: NaN-clean.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("Generated SVF: NaN-clean across parameter sweep")
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
            TPTSVFLowPassFilterGeneric<float, 1> svf;
            svf.SetCutoffAndResonance(cps, res);
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
// 6. Generated four-pole HP: passband near 0 dB above cutoff.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("Generated four-pole HP: passband near 0 dB above cutoff")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 2000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    TPTSVF4PoleHighPassFilterGeneric<float, 1> hp4;
    hp4.SetCutoffAndResonance(cutoffCps, 0.0f);

    float g = hp4.MakeSnapshot().m_stage1.m_g[0];
    float k = hp4.MakeSnapshot().m_stage1.m_k[0];

    std::function<float(float)> proc = [&hp4](float x) {
        hp4.Process(x);
        return hp4.GetOutput();
    };

    auto H = MeasureTransferFunction(proc, kN, kSR, 0x5AFE000500000001ull, 64, 2);

    auto expected = [g, k](double hz) -> double {
        const auto high = TestSVF::Transfer(g, k, hz / kSR).m_high;
        return std::norm(high);
    };

    // Skip below ~500 Hz where the 4-pole HP stopband is < -80 dB and
    // the Welch estimator noise floor prevents accurate comparison.
    //
    std::size_t lo = FreqBin(500.0,  kN, kSR);
    std::size_t hi = FreqBin(22000.0, kN, kSR);
    AssertResponseMatches(H, expected, kSR, kN, 2.0, lo, hi);
}

DOCTEST_TEST_CASE("Generated SVF: app tuning keeps natural cutoff and exponential Q")
{
    TPTSVFLowPassFilterGeneric<float, 3> low;
    TPTSVFHighPassFilterGeneric<float, 3> high;
    TPTSVF4PoleHighPassFilterGeneric<float, 3> cascade;
    const TrioFloat cutoff(0.0001f, 0.02f, 0.8f);
    const TrioFloat resonance(0, 0.5f, 1);
    low.SetCutoffAndResonance(cutoff, resonance);
    high.SetCutoffAndResonance(cutoff, resonance);
    cascade.SetCutoffAndResonance(cutoff, resonance);
    const auto snapshot = low.MakeSnapshot();
    for (size_t lane = 0; lane < 3; ++lane)
    {
        const double frequency = std::min(static_cast<double>(cutoff[lane]), static_cast<double>(0.499f));
        const double q = 0.5 * std::pow(11.0, resonance[lane]);
        DOCTEST_CHECK(snapshot.m_g[lane] == doctest::Approx(std::tan(M_PI * frequency)).epsilon(0.00002));
        DOCTEST_CHECK(snapshot.m_k[lane] == doctest::Approx(1 / q).epsilon(0.000002));
        const auto prepared = ResponseFrequency::CyclesPerSample(frequency);
        DOCTEST_CHECK(snapshot.MagnitudeSquaredResponse(prepared, lane) == doctest::Approx(q * q).epsilon(0.0001));
        DOCTEST_CHECK(high.MakeSnapshot().MagnitudeSquaredResponse(prepared, lane) == doctest::Approx(q * q).epsilon(0.0001));
        DOCTEST_CHECK(cascade.MakeSnapshot().MagnitudeSquaredResponse(prepared, lane) == doctest::Approx(q * q * q * q).epsilon(0.0002));
    }
}

DOCTEST_TEST_CASE("Generated SVF: retuning preserves memories and reset preserves coefficients")
{
    TPTSVFBandPassFilterGeneric<float, 3> filter;
    filter.SetCutoffAndQ(TrioFloat(0.01f, 0.1f, 0.4f), TrioFloat::Splat(1));
    filter.Process(TrioFloat(1, 0.5f, -1));
    const auto previous = filter.m_state;
    filter.SetCutoffAndQ(TrioFloat(0.02f, 0.2f, 0.3f), TrioFloat::Splat(1));
    DOCTEST_CHECK(filter.m_state.m_ic1 == previous.m_ic1);
    DOCTEST_CHECK(filter.m_state.m_ic2 == previous.m_ic2);
    const auto snapshot = filter.MakeSnapshot();
    const auto coefficients = filter.m_state;
    filter.Reset();
    DOCTEST_CHECK(filter.GetBandPass() == TrioFloat());
    DOCTEST_CHECK(filter.m_state.m_a1 == coefficients.m_a1);
    DOCTEST_CHECK(filter.m_state.m_a2 == coefficients.m_a2);
    DOCTEST_CHECK(filter.m_state.m_a3 == coefficients.m_a3);
    DOCTEST_CHECK(filter.MakeSnapshot().m_g == snapshot.m_g);
    filter.Process(TrioFloat());
    DOCTEST_CHECK(filter.GetBandPass() == TrioFloat());
    DOCTEST_CHECK(filter.GetLowPass() == TrioFloat());
    DOCTEST_CHECK(filter.GetHighPass() == TrioFloat());
}
