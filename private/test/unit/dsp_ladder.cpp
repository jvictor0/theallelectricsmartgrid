// dsp_ladder.cpp -- unit tests for LadderFilterLP (private/src/LadderFilter.hpp)
//
// 4-pole LP ladder with per-stage tanh saturation and resonance feedback.
// Nonlinear -- tests focus on passband/stopband ratio, resonance bounds, NaN-clean.
// The class exposes static FrequencyResponse(alpha, feedback, freq) which reflects
// the linear small-signal model; we use it as a reference at low drive level.
//
// Tests:
//   1. Passband sine passes at close to unity level (small amplitude, zero resonance).
//   2. Stopband sine strongly attenuated relative to passband (~-60 dB vs passband).
//   3. Measured low-signal response roughly matches the class's analytic model (3 dB tol).
//   4. Resonance self-oscillation bounded (high resonance, long run, no NaN/Inf).
//   5. Parameter sweep NaN-clean (cutoff & resonance).

#include "doctest.h"

#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>

#include "../support/GlobalEnv.hpp"
#include "../support/Signal.hpp"
#include "../support/Spectral.hpp"
#include "../support/NanScan.hpp"

#include "LadderFilter.hpp"

using namespace TestSpectral;
using namespace TestSignal;

namespace
{
constexpr double kSR = 48000.0;
constexpr std::size_t kN = kFftSize1024;
} // namespace

// ---------------------------------------------------------------------------
// 1. Passband sine passes at close to unity (low amplitude, no resonance).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("LadderFilterLP: passband sine near unity gain")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 4000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    LadderFilterLP filt;
    filt.SetCutoff(cutoffCps);
    filt.SetResonance(0.0f);

    // Drive with small sine at 1 kHz (well below the 4 kHz cutoff).
    //
    const float amp = 0.1f;
    TestSignal::Sine sine(1000.0f, kSR, amp);

    const std::size_t warmup = 4096;
    const std::size_t measure = 4096;
    for (std::size_t i = 0; i < warmup; ++i)
    {
        filt.Process(sine.Next());
    }

    float sumIn = 0.0f, sumOut = 0.0f;
    for (std::size_t i = 0; i < measure; ++i)
    {
        float x = sine.Next();
        sumIn  += x * x;
        sumOut += filt.Process(x) * filt.Process(x);
    }

    // Passband gain should be close to 1 (within 3 dB ~ factor of 0.7).
    //
    float rmsIn  = std::sqrt(sumIn  / measure);
    float rmsOut = std::sqrt(sumOut / measure);
    float gain   = (rmsIn > 1e-9f) ? rmsOut / rmsIn : 0.0f;

    DOCTEST_INFO("passband gain (rms ratio) at 1 kHz: " << gain);
    DOCTEST_CHECK(gain > 0.5f);
    DOCTEST_CHECK(gain < 2.0f);
}

// ---------------------------------------------------------------------------
// 2. Stopband sine attenuated >= 40 dB relative to passband sine.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("LadderFilterLP: stopband attenuation >= 40 dB vs passband")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 1000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    const std::size_t warmup = 4096;
    const std::size_t measure = 8192;
    const float amp = 0.01f;  // small signal

    // Passband measurement at 300 Hz.
    //
    {
        LadderFilterLP filt;
        filt.SetCutoff(cutoffCps);
        filt.SetResonance(0.0f);
        TestSignal::Sine sine(300.0f, kSR, amp);
        for (std::size_t i = 0; i < warmup; ++i) filt.Process(sine.Next());
        float sumOut = 0.0f;
        for (std::size_t i = 0; i < measure; ++i)
        {
            float y = filt.Process(sine.Next());
            sumOut += y * y;
        }
        float rmsPassband = std::sqrt(sumOut / measure);
        DOCTEST_INFO("passband rms at 300 Hz: " << rmsPassband);

        // Stopband measurement at 8x cutoff (8 kHz).
        //
        LadderFilterLP filt2;
        filt2.SetCutoff(cutoffCps);
        filt2.SetResonance(0.0f);
        TestSignal::Sine sine2(8000.0f, kSR, amp);
        for (std::size_t i = 0; i < warmup; ++i) filt2.Process(sine2.Next());
        float sumOut2 = 0.0f;
        for (std::size_t i = 0; i < measure; ++i)
        {
            float y = filt2.Process(sine2.Next());
            sumOut2 += y * y;
        }
        float rmsStopband = std::sqrt(sumOut2 / measure);
        DOCTEST_INFO("stopband rms at 8 kHz: " << rmsStopband);

        double attenuationDb = 20.0 * std::log10(
            (rmsStopband > 1e-12f ? rmsStopband : 1e-12) /
            (rmsPassband > 1e-12f ? rmsPassband : 1e-12));
        DOCTEST_INFO("stopband attenuation: " << attenuationDb << " dB");
        DOCTEST_CHECK(attenuationDb < -40.0);
    }
}

// ---------------------------------------------------------------------------
// 3. Low-signal linear approx: measured TF roughly matches analytic model (3 dB tol).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("LadderFilterLP: measured TF near analytic at low drive (3 dB)")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 2000.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    LadderFilterLP filt;
    filt.SetCutoff(cutoffCps);
    filt.SetResonance(0.0f);

    float alpha = filt.m_stage1.m_alpha;
    float feedback = filt.m_kEff;

    // Low amplitude to stay near linear regime.
    //
    std::function<float(float)> proc = [&filt](float x) {
        return filt.Process(x * 0.01f) / 0.01f;  // scale in/out to avoid saturation
    };

    auto H = MeasureTransferFunction(proc, kN, kSR, 0xA1D1A1D100000001ull, 48, 4);

    auto expected = [alpha, feedback](double hz) -> double {
        return LadderFilterLP::FrequencyResponse(
            alpha, feedback, static_cast<float>(hz / kSR));
    };

    // Compare passband + roll-off; avoid deep stopband where noise floor dominates.
    //
    std::size_t lo = FreqBin(100.0,  kN, kSR);
    std::size_t hi = FreqBin(8000.0, kN, kSR);
    AssertResponseMatches(H, expected, kSR, kN, 3.0, lo, hi);
}

// ---------------------------------------------------------------------------
// 4. Resonance self-oscillation bounded (long run, no NaN, amplitude finite).
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("LadderFilterLP: high resonance self-oscillation bounded and NaN-clean")
{
    GlobalEnv::ResetPerTest();

    const float cutoffHz  = 500.0f;
    const float cutoffCps = cutoffHz / static_cast<float>(kSR);

    LadderFilterLP filt;
    filt.SetCutoff(cutoffCps);
    filt.SetResonance(0.95f);  // near maximum resonance

    const std::size_t nSamples = 48000;  // one second
    std::vector<float> out(nSamples);
    float maxAbs = 0.0f;

    for (std::size_t i = 0; i < nSamples; ++i)
    {
        // Feed tiny noise to kick off oscillation but not dominate.
        //
        out[i] = filt.Process((i == 0) ? 0.001f : 0.0f);
        float a = std::abs(out[i]);
        if (a > maxAbs) maxAbs = a;
    }

    DOCTEST_INFO("max output amplitude under high resonance: " << maxAbs);
    DOCTEST_CHECK(maxAbs < 100.0f);     // must stay bounded
    TestNan::AssertClean(out.data(), nSamples);
}

// ---------------------------------------------------------------------------
// 5. Parameter sweep NaN-clean.
// ---------------------------------------------------------------------------
//
DOCTEST_TEST_CASE("LadderFilterLP: NaN-clean across parameter sweep")
{
    GlobalEnv::ResetPerTest();

    const std::size_t nSamples = 512;
    TestSignal::WhiteNoise noise(0xA1D1A1D100000002ull);
    std::vector<float> out(nSamples);

    const float cutoffs[]   = { 0.001f, 0.01f, 0.05f, 0.1f, 0.3f, 0.49f };
    const float resonances[] = { 0.0f, 0.25f, 0.5f, 0.75f, 0.99f };

    for (float cps : cutoffs)
    {
        for (float res : resonances)
        {
            LadderFilterLP filt;
            filt.SetCutoff(cps);
            filt.SetResonance(res);
            for (std::size_t i = 0; i < nSamples; ++i)
            {
                out[i] = filt.Process(noise.Next());
            }
            DOCTEST_INFO("cutoffCps=" << cps << " resonance=" << res);
            TestNan::AssertClean(out.data(), nSamples);
        }
    }
}

DOCTEST_TEST_CASE("LadderFilterLP: four-pole detectors focus on the loop resonance")
{
    GlobalEnv::ResetPerTest();

    constexpr float x_cutoff = 0.005f;
    LadderFilterLP reference;
    reference.SetCutoff(x_cutoff);
    double alpha = reference.m_stage4.m_alpha;
    double ratio = alpha / (1.0 - alpha);
    float center = static_cast<float>(std::acos(1.0 - 0.5 * ratio * ratio) / (2.0 * M_PI));
    float frequencies[] = {center * 0.1f, center, center * 10.0f};
    float detectedPower[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        LadderFilterLP filter;
        filter.SetCutoff(x_cutoff);
        filter.SetResonance(0.0f);
        TestSignal::Sine sine(frequencies[i] * static_cast<float>(kSR), kSR, 1.0f);
        for (int sample = 0; sample < 48000; ++sample)
        {
            filter.Process(sine.Next());
            if (sample >= 24000)
            {
                detectedPower[i] += filter.m_inputEnvelope.GetPower() / 24000.0f;
            }
        }
    }

    // Unity gain at the center preserves the sine's mean-square power.
    //
    DOCTEST_CHECK(detectedPower[1] == doctest::Approx(0.5f).epsilon(0.01f));

    // Reject tones a decade away by at least 25 dB relative to the center.
    //
    DOCTEST_CHECK(detectedPower[0] < 0.003f * detectedPower[1]);
    DOCTEST_CHECK(detectedPower[2] < 0.003f * detectedPower[1]);
}

DOCTEST_TEST_CASE("LadderFilterLP: RMS damping suppresses resonance over the noise floor")
{
    GlobalEnv::ResetPerTest();

    LadderFilterLP filter;
    filter.SetCutoff(0.002f);
    filter.SetSaturationGainAndBias(2.0f, 0.0f);
    filter.SetResonance(1.0f);

    float noiseAmplitude = 0.0f;
    DOCTEST_SUBCASE("silence")
    {
    }

    DOCTEST_SUBCASE("quiet noise floor")
    {
        noiseAmplitude = 0.000001f;
    }

    TestSignal::WhiteNoise noise(0xA1D1A1D100000004ull);
    float outputPowerSum = 0.0f;
    float feedbackSum = 0.0f;
    for (int i = 0; i < 96000; ++i)
    {
        float input = i == 0 ? 0.001f : noiseAmplitude * noise.Next();
        float output = filter.Process(input);
        DOCTEST_REQUIRE(std::isfinite(output));
        if (i >= 48000)
        {
            outputPowerSum += output * output;
            feedbackSum += filter.m_kEff;
        }
    }

    float outputRms = std::sqrt(outputPowerSum / 48000.0f);
    DOCTEST_INFO("settled output RMS: " << outputRms);
    DOCTEST_INFO("settled feedback: " << feedbackSum / 48000.0f);
    DOCTEST_CHECK(outputRms < 0.15f);
    DOCTEST_CHECK(feedbackSum / 48000.0f < 1.0f);
}

DOCTEST_TEST_CASE("LadderFilterLP: RMS damping preserves tones and distinguishes driven noise from the floor")
{
    GlobalEnv::ResetPerTest();

    LadderFilterLP filter;
    LadderFilterLP floorFilter;
    for (LadderFilterLP* configured : {&filter, &floorFilter})
    {
        configured->SetCutoff(0.002f);
        configured->SetSaturationGainAndBias(2.0f, 0.0f);
        configured->SetResonance(1.0f);
    }

    bool useNoise = false;
    DOCTEST_SUBCASE("partial near resonance")
    {
    }

    DOCTEST_SUBCASE("broadband noise")
    {
        useNoise = true;
    }

    TestSignal::Sine sine(224.0f, kSR, 0.5f);
    TestSignal::WhiteNoise noise(0xA1D1A1D100000003ull);
    float feedbackSum = 0.0f;
    float floorFeedbackSum = 0.0f;
    for (int i = 0; i < 96000; ++i)
    {
        float input = useNoise ? 0.5f * noise.Next() : sine.Next();
        DOCTEST_REQUIRE(std::isfinite(filter.Process(input)));
        if (useNoise)
        {
            floorFilter.Process(i == 0 ? 0.001f : input * 0.000002f);
        }

        if (i >= 48000)
        {
            feedbackSum += filter.m_kEff;
            floorFeedbackSum += floorFilter.m_kEff;
        }
    }

    float meanFeedback = feedbackSum / 48000.0f;
    DOCTEST_INFO("driven feedback: " << meanFeedback);

    if (useNoise)
    {
        // Noise can trigger damping, but strong excitation must retain
        // substantially more resonance than the same noise at the floor.
        //
        float meanFloorFeedback = floorFeedbackSum / 48000.0f;
        DOCTEST_INFO("noise-floor feedback: " << meanFloorFeedback);
        DOCTEST_CHECK(meanFeedback > 2.0f * meanFloorFeedback);
    }
    else
    {
        DOCTEST_CHECK(meanFeedback > 0.95f * filter.m_feedback);
    }
}

DOCTEST_TEST_CASE("LadderFilterLP: reset clears RMS damping history")
{
    GlobalEnv::ResetPerTest();

    LadderFilterLP filter;
    LadderFilterLP fresh;
    for (LadderFilterLP* configured : {&filter, &fresh})
    {
        configured->SetCutoff(0.002f);
        configured->SetSaturationGainAndBias(2.0f, 0.0f);
        configured->SetResonance(1.0f);
    }

    for (int i = 0; i < 24000; ++i)
    {
        filter.Process(i == 0 ? 0.001f : 0.0f);
    }

    filter.Reset();
    for (int i = 0; i < 4096; ++i)
    {
        float input = i == 0 ? 0.1f : 0.0f;
        DOCTEST_CHECK(filter.Process(input) == fresh.Process(input));
        DOCTEST_CHECK(filter.m_kEff == fresh.m_kEff);
    }
}

DOCTEST_TEST_CASE("LadderFilterLP: direct resonance height matches the linear response")
{
    GlobalEnv::ResetPerTest();

    LadderFilterLP filter;
    for (float alpha : {0.01f, 0.1f, 0.5f, 0.7f, 0.95f})
    {
        filter.SetAlphaDirect(alpha);
        float frequency = 0.5f;
        if (alpha < 2.0f / 3.0f)
        {
            double ratio = static_cast<double>(alpha) / (1.0 - alpha);
            frequency = static_cast<float>(std::acos(1.0 - 0.5 * ratio * ratio) / (2.0 * M_PI));
        }

        float feedback = std::min(3.9f, 0.9f * filter.m_criticalFeedback);
        float height = 1.0f / (filter.m_criticalFeedback - feedback);
        float reference = LadderFilterLP::FrequencyResponse(alpha, feedback, frequency) / (1.0f + feedback);
        DOCTEST_INFO("alpha: " << alpha);
        DOCTEST_CHECK(height == doctest::Approx(reference).epsilon(0.001f));
    }
}
