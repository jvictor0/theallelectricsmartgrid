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
#include "TrioUtils.hpp"

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

// Keep the established damping scenarios at the same cascade half-power
// frequency while expressing their tuning through the new stage corner.
//
float StageCornerFromHalfPower(float cutoff)
{
    const double ratio = std::sqrt(std::pow(2.0, 0.25) - 1);
    return static_cast<float>(std::asin(std::sin(M_PI * cutoff) / ratio) / M_PI);
}

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
    filt.SetCutoffAndResonance(cutoffCps, 0.0f);

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
        const float output = filt.Process(x);
        sumOut += output * output;
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
        filt.SetCutoffAndResonance(cutoffCps, 0.0f);
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
        filt2.SetCutoffAndResonance(cutoffCps, 0.0f);
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
    filt.SetCutoffAndResonance(cutoffCps, 0.0f);

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
    filt.SetCutoffAndResonance(cutoffCps, 0.95f);  // near maximum resonance

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
            filt.SetCutoffAndResonance(cps, res);
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
    reference.SetCutoffAndResonance(x_cutoff, 0.0f);
    double alpha = reference.m_stage4.m_alpha;
    double ratio = alpha / (1.0 - alpha);
    float center = static_cast<float>(std::acos(1.0 - 0.5 * ratio * ratio) / (2.0 * M_PI));
    float frequencies[] = {center * 0.1f, center, center * 10.0f};
    float detectedPower[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        LadderFilterLP filter;
        filter.SetCutoffAndResonance(x_cutoff, 0.0f);
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
    filter.SetCutoffAndResonance(StageCornerFromHalfPower(0.002f), 1.0f);
    filter.SetSaturationGain(2.0f);

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
        configured->SetCutoffAndResonance(StageCornerFromHalfPower(0.002f), 1.0f);
        configured->SetSaturationGain(2.0f);
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
        configured->SetCutoffAndResonance(StageCornerFromHalfPower(0.002f), 1.0f);
        configured->SetSaturationGain(2.0f);
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
    for (float cutoff : {0.0007f, 0.0073f, 0.049f, 0.09f, 0.25f})
    {
        filter.SetCutoffAndResonance(cutoff, 0.0f);
        const float alpha = filter.m_stage4.m_alpha;
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

DOCTEST_TEST_CASE("LadderFilterLP: tuning preserves history and refreshes feedback on every parameter change")
{
    using Vector = TrioLadderFilterLP::Vector;
    TrioLadderFilterLP filter;
    Vector cutoff(0.002f, 0.04f, 0.2f);
    Vector resonance(0.9f, 0.7f, 0.8f);
    Vector drive(0.5f, 2.0f, 4.0f);

    for (size_t change = 0; change < 6; ++change)
    {
        if (change == 1)
        {
            resonance = Vector(0.4f, 0.95f, 0.1f);
        }
        else if (change == 2)
        {
            drive = Vector(2.0f, 0.5f, 1.0f);
        }
        else if (change == 3)
        {
            cutoff = Vector(0.02f, 0.1f, 0.005f);
        }
        else if (change == 4)
        {
            cutoff = Vector(0.04f, 0.003f, 0.4f);
            resonance = Vector(0.8f, 0.4f, 1.0f);
            drive = Vector(3.0f, 1.0f, 0.25f);
        }

        const auto before = filter;
        if (change % 2 == 0)
        {
            filter.SetSaturationGain(drive);
            filter.SetCutoffAndResonance(cutoff, resonance);
        }
        else
        {
            filter.SetCutoffAndResonance(cutoff, resonance);
            filter.SetSaturationGain(drive);
        }

        DOCTEST_CHECK(filter.m_stage1.m_output == before.m_stage1.m_output);
        DOCTEST_CHECK(filter.m_stage2.m_output == before.m_stage2.m_output);
        DOCTEST_CHECK(filter.m_stage3.m_output == before.m_stage3.m_output);
        DOCTEST_CHECK(filter.m_stage4.m_output == before.m_stage4.m_output);
        DOCTEST_CHECK(filter.m_inputEnvelope.GetPower() == before.m_inputEnvelope.GetPower());
        DOCTEST_CHECK(filter.m_feedbackEnvelope.GetPower() == before.m_feedbackEnvelope.GetPower());

        // A newly configured filter must agree regardless of parameter history
        // and setter order, including a repeated identical tuning request.
        //
        TrioLadderFilterLP fresh;
        fresh.SetCutoffAndResonance(cutoff, resonance);
        fresh.SetSaturationGain(drive);
        DOCTEST_CHECK(filter.m_kSafe == fresh.m_kSafe);
        DOCTEST_CHECK(filter.m_baseFeedback == fresh.m_baseFeedback);
        DOCTEST_CHECK(filter.m_referencePowerGain == fresh.m_referencePowerGain);
        DOCTEST_CHECK(filter.m_dampingScale == fresh.m_dampingScale);

        // Use controlled previous-sample powers to exercise active damping.
        // Check the feedback applied to the delayed, unsaturated stage state.
        //
        filter.m_inputEnvelope.m_power = Vector(0.01f, 0.0001f, 0.1f);
        filter.m_feedbackEnvelope.m_power = Vector(1.0f, 0.2f, 0.3f);
        const Vector previousStage4 = filter.m_stage4.m_output;
        Vector expectedFeedback;
        for (size_t lane = 0; lane < 3; ++lane)
        {
            const double alpha = filter.m_stage4.m_alpha[lane];
            const double slope = drive[lane] / filter.m_saturationTanhGain[lane];
            const double safe = 0.9 / (std::pow(alpha * slope / (2 - alpha), 4) + 1e-8);
            const double base = std::min(3.9 * resonance[lane], safe);
            const double inputPower = filter.m_inputEnvelope.GetPower()[lane];
            const double feedbackPower = filter.m_feedbackEnvelope.GetPower()[lane];
            const double referencePower = inputPower / std::pow(filter.m_criticalFeedback[lane] - base, 2);
            const double excess = std::max(0.0, feedbackPower - referencePower) / (inputPower + 1e-8);
            expectedFeedback[lane] = static_cast<float>(base / (1 + 1e-4 * std::pow(slope, 3) * excess));
        }

        const Vector input(0.2f, -0.1f, 0.4f);
        filter.Process(input);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(filter.m_kEff[lane] == doctest::Approx(expectedFeedback[lane]).epsilon(2e-6));
            DOCTEST_CHECK(filter.m_input[lane] ==
                doctest::Approx(input[lane] - expectedFeedback[lane] * previousStage4[lane]).epsilon(2e-6));
        }
    }
}

DOCTEST_TEST_CASE("LadderFilterLP: cutoff is the stage half-power point and cascade quarter-amplitude point")
{
    LadderFilterLP filter;
    for (float cutoff : {0.000001f, 0.0001f, 0.005f, 0.01f, 0.04f, 0.08f, 0.125f, 0.25f, 0.499f, 0.5f})
    {
        filter.SetCutoffAndResonance(cutoff, 0.0f);
        const double alpha = filter.m_stage4.m_alpha;
        const auto zInverse = std::polar(1.0, -2 * M_PI * cutoff);
        const auto stage = alpha / (1.0 - (1.0 - alpha) * zInverse);
        DOCTEST_INFO("cutoff: " << cutoff);
        // Allow float coefficient rounding, including at sub-Hz cutoffs.
        //
        DOCTEST_CHECK(std::norm(stage) == doctest::Approx(0.5).epsilon(0.0002));
        DOCTEST_CHECK(std::pow(std::norm(stage), 4) == doctest::Approx(0.0625).epsilon(0.0002));

        const double halfPower = std::asin(std::sqrt(std::pow(2.0, 0.25) - 1) * std::sin(M_PI * cutoff)) / M_PI;
        const auto halfPowerZ = std::polar(1.0, -2 * M_PI * halfPower);
        const auto halfPowerStage = alpha / (1.0 - (1.0 - alpha) * halfPowerZ);
        DOCTEST_CHECK(std::pow(std::norm(halfPowerStage), 4) == doctest::Approx(0.5).epsilon(0.0002));
        DOCTEST_CHECK(filter.m_inputEnvelope.m_powerAlpha[0] ==
            doctest::Approx(-std::expm1(-halfPower / 2)).epsilon(0.00002));
        DOCTEST_CHECK(filter.m_feedbackEnvelope.m_powerAlpha == filter.m_inputEnvelope.m_powerAlpha);
    }
}

DOCTEST_TEST_CASE("LadderFilterLP: stage cutoff and nominal oscillation center stay distinct")
{
    LadderFilterLP filter;
    for (float cutoff : {0.005f, 0.0625f, 0.125f, 0.195f, 0.197f, 0.25f, 0.499f})
    {
        filter.SetCutoffAndResonance(cutoff, 1.0f);
        const double alpha = filter.m_stage4.m_alpha;
        const double ratio = alpha / (1 - alpha);
        const double center = ratio < 2 ? std::asin(ratio / 2) / M_PI : 0.5;
        const double clampedCenter = std::min(center, 0.499);
        for (const auto* follower : {&filter.m_inputEnvelope, &filter.m_feedbackEnvelope})
        {
            for (const auto* stage : {&follower->m_stage1, &follower->m_stage2})
            {
                const double actualCenter = std::atan(stage->MakeSnapshot().m_g[0]) / M_PI;
                DOCTEST_CHECK(actualCenter == doctest::Approx(clampedCenter).epsilon(0.00002));
            }
        }

        const auto zInverse = std::polar(1.0, -2 * M_PI * center);
        const auto stage = alpha / (1.0 - (1.0 - alpha) * zInverse);
        const auto criticalLoop = zInverse * std::pow(stage, 4) * static_cast<double>(filter.m_criticalFeedback);
        DOCTEST_CHECK(criticalLoop.real() == doctest::Approx(-1).epsilon(0.00002));
        DOCTEST_CHECK(std::abs(criticalLoop.imag()) < 0.00002);
    }

    // 24 kHz stage corner at a 192 kHz engine: the detector is around 36 kHz,
    // and the Nyquist guard does not clamp requested feedback at default drive.
    //
    filter.SetCutoffAndResonance(0.125f, 1.0f);
    DOCTEST_CHECK(filter.m_criticalFeedback[0] == doctest::Approx(9.68696).epsilon(0.00002));
    DOCTEST_CHECK(filter.m_kSafe[0] == doctest::Approx(41.5484).epsilon(0.00002));
    DOCTEST_CHECK(filter.m_baseFeedback == filter.m_feedback);
    filter.SetSaturationGain(2.0f);
    DOCTEST_CHECK(filter.m_kSafe[0] == doctest::Approx(3.23352).epsilon(0.00002));
    DOCTEST_CHECK(filter.m_baseFeedback == filter.m_kSafe);

    filter.SetCutoffAndResonance(2.0f, 1.0f);
    DOCTEST_CHECK(filter.m_cutoff[0] == 0.5f);
    DOCTEST_CHECK(filter.m_stage4.m_alpha[0] == doctest::Approx(2 * (std::sqrt(2.0) - 1)).epsilon(0.000002));
    DOCTEST_CHECK(filter.m_criticalFeedback[0] == doctest::Approx(4.0).epsilon(0.000002));
}

DOCTEST_TEST_CASE("Ladder envelope: generated band-pass cascade retains unity center gain and reset")
{
    BandPassedEnvelopeFollowerGeneric<float, 3> follower;
    const TrioFloat cutoff(0.005f, 0.03f, 0.2f);
    follower.SetCutoff(cutoff, TrioFloat::Splat(0.01f));
    double sums[3]{};
    for (size_t sample = 0; sample < 10000; ++sample)
    {
        TrioFloat input;
        for (size_t lane = 0; lane < 3; ++lane)
        {
            input[lane] = 0.1f * std::sin(2 * M_PI * cutoff[lane] * sample);
        }

        const auto power = follower.Process(input);
        if (sample >= 5000)
        {
            for (size_t lane = 0; lane < 3; ++lane)
            {
                sums[lane] += power[lane] / 5000;
            }
        }
    }

    for (size_t lane = 0; lane < 3; ++lane)
    {
        DOCTEST_CHECK(sums[lane] == doctest::Approx(0.005).epsilon(0.005));
    }

    follower.Reset();
    DOCTEST_CHECK(follower.GetPower() == TrioFloat());
    DOCTEST_CHECK(follower.Process(TrioFloat()) == TrioFloat());
    DOCTEST_CHECK(follower.m_stage1.MakeSnapshot().m_k == TrioFloat::Splat(1));
    DOCTEST_CHECK(follower.m_stage2.MakeSnapshot().m_k == TrioFloat::Splat(1));
}
