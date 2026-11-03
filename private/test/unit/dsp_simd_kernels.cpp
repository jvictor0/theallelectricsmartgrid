#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "AllPassFilter.hpp"
#include "DelayLine.hpp"
#include "LinkwitzRileyCrossover.hpp"
#include "Metering.hpp"
#include "QuadReverb.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <memory>

namespace
{
template<typename Number, size_t Size>
void CheckFusedMultiplyAdd()
{
    using Vector = MultiChannelNumber<Number, Size>;
    Number epsilon = std::ldexp(Number(1), -std::numeric_limits<Number>::digits / 2 - 1);
    Vector a, b, c;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        a[lane] = 1 + epsilon;
        b[lane] = 1 - epsilon;
        c[lane] = -1;
    }

    Vector result = Vector::FusedMultiplyAdd(a, b, c);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        DOCTEST_CHECK(result[lane] == std::fma(a[lane], b[lane], c[lane]));
        DOCTEST_CHECK(result[lane] != 0);
    }
}
}

DOCTEST_TEST_CASE("SIMD FMA: scalar, stereo, padded trio and quad retain fused rounding")
{
    CheckFusedMultiplyAdd<float, 1>();
    CheckFusedMultiplyAdd<float, 2>();
    CheckFusedMultiplyAdd<float, 3>();
    CheckFusedMultiplyAdd<float, 4>();
    CheckFusedMultiplyAdd<double, 1>();
    CheckFusedMultiplyAdd<double, 3>();
    CheckFusedMultiplyAdd<double, 4>();
}

DOCTEST_TEST_CASE("All-pass: equal and unequal delay reads survive changes and ring wraps")
{
    auto packed = std::make_unique<QuadAllPassFilter>();
    auto mono = std::make_unique<std::array<AllPassFilter, 4>>();
    for (size_t sample = 0; sample < 9000; ++sample)
    {
        if (sample % 1500 == 0)
        {
            for (size_t lane = 0; lane < 4; ++lane)
            {
                int delay = sample % 3000 == 0 ? 37 : 37 + 11 * lane;
                packed->SetDelaySamples(lane, delay);
                (*mono)[lane].SetDelaySamples(0, delay);
            }
        }

        QuadFloat input;
        for (size_t lane = 0; lane < 4; ++lane)
        {
            input[lane] = std::sin(0.13 * sample + lane);
        }

        auto output = packed->Process(input);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(output[lane] == (*mono)[lane].Process(input[lane])[0]);
        }
    }
}

DOCTEST_TEST_CASE("Delay interpolation: packed cubic matches scalar and linear reads wrap and reflect")
{
    DelayLineGeneric<float, 4, 64> delay;
    for (size_t sample = 0; sample < 256; ++sample)
    {
        delay.Write(QuadFloat::Splat(std::sin(0.17 * sample)) + QuadFloat(0, 0.1f, -0.2f, 0.3f));
        QuadFloat times(-7.25f, 0.5f, 63.75f, 64.5f + sample);
        auto cubic = delay.ReadCubic(times);
        auto linear = delay.ReadLinear(times);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            float index = delay.m_index - delay.MirrorDelayTime(times[lane]) + 128;
            DOCTEST_CHECK(cubic[lane] == doctest::Approx(delay.ReadAtIndex(lane, index)).epsilon(2e-6).scale(1));
            size_t i = static_cast<size_t>(index) % 64;
            double fraction = index - std::floor(index);
            double expected = delay.m_delayLine[i][lane] * (1 - fraction)
                + delay.m_delayLine[(i + 1) % 64][lane] * fraction;
            DOCTEST_CHECK(linear[lane] == doctest::Approx(expected).epsilon(2e-7).scale(1));
        }
    }
}

DOCTEST_TEST_CASE("Crossover: packed cutoff coefficients match the double-precision filter formula across clamped range")
{
    using Crossover = LinkwitzRileyCrossoverGeneric<float, 4>;
    Crossover packed[3];
    LinkwitzRileyCrossover scalar;
    for (size_t step = 0; step < 8192; ++step)
    {
        float t = static_cast<float>(step) / 8191;
        MultiChannelNumber<float, 3> frequency(
            std::pow(10.0f, -5.0f + 5.0f * t), t * 0.5f, 0.499f - 0.499f * t);
        Crossover::SetCyclesPerSampleBatch(packed, frequency);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            scalar.m_cyclesPerSample = std::clamp(frequency[lane], scalar.x_minCyclesPerSample, scalar.x_maxCyclesPerSample);
            double omega = 2.0 * M_PI * scalar.m_cyclesPerSample;
            auto configure = [omega](auto& low, auto& high, double q)
            {
                double cosine = std::cos(omega);
                double alpha = std::sin(omega) / (2 * q);
                double denominator = 1 + alpha;
                low.m_b0 = low.m_b2 = (1 - cosine) * 0.5 / denominator;
                low.m_b1 = (1 - cosine) / denominator;
                high.m_b0 = high.m_b2 = (1 + cosine) * 0.5 / denominator;
                high.m_b1 = -(1 + cosine) / denominator;
                low.m_a1 = high.m_a1 = -2 * cosine / denominator;
                low.m_a2 = high.m_a2 = (1 - alpha) / denominator;
            };
            configure(scalar.m_lowBiquad1, scalar.m_highBiquad1, scalar.Q1());
            configure(scalar.m_lowBiquad2, scalar.m_highBiquad2, scalar.Q2());
            DOCTEST_CHECK(packed[lane].m_cyclesPerSample == scalar.m_cyclesPerSample);
            auto check = [](const auto& actual, const auto& expected)
            {
                DOCTEST_CHECK(actual.m_b0 == doctest::Approx(expected.m_b0).epsilon(2e-6).scale(1e-7));
                DOCTEST_CHECK(actual.m_b1 == doctest::Approx(expected.m_b1).epsilon(2e-6).scale(1e-7));
                DOCTEST_CHECK(actual.m_b2 == doctest::Approx(expected.m_b2).epsilon(2e-6).scale(1e-7));
                DOCTEST_CHECK(actual.m_a1 == doctest::Approx(expected.m_a1).epsilon(2e-6).scale(1));
                DOCTEST_CHECK(actual.m_a2 == doctest::Approx(expected.m_a2).epsilon(2e-6).scale(1));
                DOCTEST_CHECK(actual.m_a2 < 1.0f);
                DOCTEST_CHECK(actual.m_a1 > -1.0 - actual.m_a2);
                DOCTEST_CHECK(actual.m_a1 < 1.0 + actual.m_a2);
            };
            check(packed[lane].m_lowBiquad1, scalar.m_lowBiquad1);
            check(packed[lane].m_lowBiquad2, scalar.m_lowBiquad2);
            check(packed[lane].m_highBiquad1, scalar.m_highBiquad1);
            check(packed[lane].m_highBiquad2, scalar.m_highBiquad2);
        }
    }
}

DOCTEST_TEST_CASE("Meters: packed RMS and held peak ages match independent scalar recurrence")
{
    QuadMeter meter;
    float rms[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float peak[4] = {0.8f, 0.7f, 0.6f, 0.5f};
    uint32_t age[4] = {0, QuadMeter::x_peakHistorySize - 1, QuadMeter::x_peakHistorySize, 17};
    for (size_t lane = 0; lane < 4; ++lane)
    {
        meter.m_rms[lane] = rms[lane];
        meter.m_peak[lane] = peak[lane];
        meter.m_samplesSincePeak[lane] = age[lane];
    }

    for (size_t frame = 0; frame < 128; ++frame)
    {
        float squares[4] = {}, magnitude[4] = {};
        for (size_t sub = 0; sub < QuadMeter::x_oversample; ++sub)
        {
            QuadFloat input;
            for (size_t lane = 0; lane < 4; ++lane)
            {
                input[lane] = 0.75f * std::sin(0.11 * frame + 0.2 * sub + lane);
                squares[lane] += input[lane] * input[lane];
                magnitude[lane] = std::max(magnitude[lane], std::abs(input[lane]));
            }

            meter.Process(input);
        }

        for (size_t lane = 0; lane < 4; ++lane)
        {
            float squared = squares[lane] / QuadMeter::x_oversample;
            float alpha = rms[lane] < squared ? QuadMeter::x_smoothingAlphaUp : QuadMeter::x_smoothingAlphaDown;
            rms[lane] = std::fma(squared, alpha, rms[lane] * (1 - alpha));
            if (peak[lane] < magnitude[lane])
            {
                peak[lane] = magnitude[lane];
                age[lane] = 0;
            }
            else if (age[lane] < QuadMeter::x_peakHistorySize)
            {
                ++age[lane];
            }
            else
            {
                peak[lane] = std::fma(magnitude[lane], QuadMeter::x_peakSmoothingAlpha,
                    peak[lane] * (1 - QuadMeter::x_peakSmoothingAlpha));
            }

            DOCTEST_CHECK(meter.m_rms[lane] == doctest::Approx(rms[lane]).epsilon(1e-6));
            DOCTEST_CHECK(meter.m_peak[lane] == doctest::Approx(peak[lane]).epsilon(1e-6));
            DOCTEST_CHECK(meter.m_samplesSincePeak[lane] == age[lane]);
        }
    }
}

DOCTEST_TEST_CASE("Reverb delay offset: neutral default and unsmoothed per-sample bipolar control")
{
    GlobalEnv::ResetPerTest();
    QuadReverbInputSetter setter;
    QuadReverbInputSetter::Input input;
    QuadReverb::Input output;
    setter.Process(input, output);
    DOCTEST_CHECK(output.m_delayOffsetSamples == QuadFloat());
    input.m_delayOffsetKnob = QuadFloat(0, 0.25f, 0.75f, 1);
    setter.Process(input, output);
    DOCTEST_CHECK(output.m_delayOffsetSamples == QuadFloat(-1000, -500, 500, 1000) * SampleTimer::x_oversample);
    input.m_delayOffsetKnob = QuadFloat::Splat(0.5f);
    setter.Process(input, output);
    DOCTEST_CHECK(output.m_delayOffsetSamples == QuadFloat());
}

DOCTEST_TEST_CASE("Delay interpolation: a large write index preserves a small fractional delay")
{
    using Delay = DelayLineGeneric<float, 4, 1 << 18>;
    auto delay = std::make_unique<Delay>();
    delay->m_index = 100000;
    delay->m_delayLine[99999] = QuadFloat::Splat(0);
    delay->m_delayLine[100000] = QuadFloat::Splat(1);
    DOCTEST_CHECK(delay->ReadLinear(QuadFloat::Splat(0.03125f)) == QuadFloat::Splat(0.96875f));
}
