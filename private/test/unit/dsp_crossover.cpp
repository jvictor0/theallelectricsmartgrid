#include "doctest.h"
#include "LinkwitzRileyCrossover.hpp"
#include "MultibandSaturator.hpp"
#include "LinkwitzRileySplitter4BandGeneric.hpp"
#include "SampleTimer.hpp"
#include <cmath>
#include <complex>

namespace
{
// Independent double direct-form reference for the original fixed response.
//
struct CrossoverReferenceSection
{
    double m_b0, m_b1, m_b2, m_a1, m_a2;
    double m_x1 = 0, m_x2 = 0, m_y1 = 0, m_y2 = 0;

    CrossoverReferenceSection(double cutoff, double q, bool high)
    {
        double omega = 2 * M_PI * cutoff;
        double cosine = std::cos(omega);
        double alpha = std::sin(omega) / (2 * q);
        m_b0 = m_b2 = (high ? 1 + cosine : 1 - cosine) / (2 * (1 + alpha));
        m_b1 = (high ? -2 : 2) * m_b0;
        m_a1 = -2 * cosine / (1 + alpha);
        m_a2 = (1 - alpha) / (1 + alpha);
    }

    double Process(double input)
    {
        double output = m_b0 * input + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1;
        m_x1 = input;
        m_y2 = m_y1;
        m_y1 = output;
        return output;
    }
};
}

DOCTEST_TEST_CASE("Crossover: low cutoff DC gain stays unity at engine rate")
{
    using Crossover = LinkwitzRileyCrossoverGeneric<float, 4>;
    for (size_t step = 0; step < 128; ++step)
    {
        float cutoffHz = 23.4375f + 76.5625f * step / 127;
        Crossover crossover;
        crossover.SetCyclesPerSample(SampleTimer::HzToCycPerSamp(cutoffHz));
        Crossover::Vector input(0.1f, -0.2f, 0.3f, -0.4f);
        Crossover::CrossoverOutput output;
        for (size_t sample = 0; sample < SampleTimer::x_sampleRate / 2; ++sample)
        {
            output = crossover.Process(input);
        }

        DOCTEST_INFO("cutoff Hz=" << cutoffHz);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(std::abs(output.m_lowPass[lane] / input[lane] - 1) < 0.0003f);
            DOCTEST_CHECK(std::abs(output.m_highPass[lane]) < 0.00003f);
        }
    }
}

DOCTEST_TEST_CASE("Crossover: fixed response matches double direct form including low cutoffs and Nyquist")
{
    using Crossover = LinkwitzRileyCrossoverGeneric<float, 4>;
    for (float cutoff : {0.0001f, 23.59139f / 192000, 25.86836f / 192000,
        40.0f / 192000, 74.4094f / 192000, 0.01f, 0.1f, 0.25f, 0.45f, 0.499f})
    {
        Crossover crossover;
        crossover.SetCyclesPerSample(cutoff);
        double error = 0;
        bool finite = true;
        for (size_t lane = 0; lane < 4; ++lane)
        {
            crossover.Reset();
            CrossoverReferenceSection low1(cutoff, crossover.Q1(), false);
            CrossoverReferenceSection low2(cutoff, crossover.Q2(), false);
            CrossoverReferenceSection high1(cutoff, crossover.Q1(), true);
            CrossoverReferenceSection high2(cutoff, crossover.Q2(), true);
            double frequency = lane == 0 ? 0 : lane == 1 ? cutoff * 0.8 : lane == 2 ? cutoff : 0.49;
            for (size_t sample = 0; sample < 192000; ++sample)
            {
                double phase = 2 * M_PI * frequency * sample;
                float input = static_cast<float>(0.2 * std::cos(phase));
                auto output = crossover.Process(Crossover::Vector::Splat(input));
                double low = low2.Process(low1.Process(input));
                double high = high2.Process(high1.Process(input));
                finite = finite && std::isfinite(output.m_lowPass[lane]) && std::isfinite(output.m_highPass[lane]);
                if (sample >= 96000)
                {
                    error = std::max(error, std::abs(output.m_lowPass[lane] - low));
                    error = std::max(error, std::abs(output.m_highPass[lane] - high));
                }
            }
        }

        DOCTEST_INFO("cutoff=" << cutoff << " maximum sample error=" << error);
        DOCTEST_CHECK(finite);
        DOCTEST_CHECK(error < 0.0002);
    }
}

DOCTEST_TEST_CASE("Crossover: response model retains existing Butterworth branches")
{
    for (float cutoff : {0.0001f, 0.001f, 0.1f, 0.49f})
    {
        auto dc = LinkwitzRileyCrossover::TransferFunction(cutoff, 0);
        DOCTEST_CHECK(std::abs(dc.m_lowPass - std::complex<float>(1, 0)) < 1e-6f);
        DOCTEST_CHECK(std::abs(dc.m_highPass) < 1e-6f);
        auto center = LinkwitzRileyCrossover::TransferFunction(cutoff, cutoff);
        DOCTEST_CHECK(std::abs(center.m_lowPass) == doctest::Approx(std::sqrt(0.5)).epsilon(1e-6));
        DOCTEST_CHECK(std::abs(center.m_lowPass - center.m_highPass) < 1e-6f);
    }
}

DOCTEST_TEST_CASE("Crossover: captured coefficients agree with the lookup response")
{
    LinkwitzRileyCrossover filter;
    for (float cutoff : {0.0001f, 0.001f, 0.1f, 0.25f, 0.49f, 0.499f})
    {
        filter.SetCyclesPerSample(cutoff);
        auto snapshot = filter.MakeSnapshot();
        for (float frequency : {0.0f, cutoff * 0.8f, cutoff, 0.49f, 0.5f})
        {
            auto expected = filter.TransferFunction(cutoff, frequency);
            auto actual = snapshot.Responses(ResponseFrequency::CyclesPerSample(frequency));
            DOCTEST_CHECK(std::abs(actual.m_lowPass - expected.m_lowPass) < 0.001f);
            DOCTEST_CHECK(std::abs(actual.m_highPass - expected.m_highPass) < 0.001f);
        }
    }
}

DOCTEST_TEST_CASE("Crossover: audio-rate cutoff modulation stays bounded and settles after silence")
{
    using Crossover = LinkwitzRileyCrossoverGeneric<float, 4>;
    for (bool jumps : {false, true})
    {
        Crossover crossover;
        double peak = 0;
        bool finite = true;
        for (size_t sample = 0; sample < 192000; ++sample)
        {
            float modulation = 0.5f + 0.5f * std::sin(2 * M_PI * 400 * sample / 192000);
            float cutoff = jumps ? ((sample / 257) % 2 == 0 ? 0.0001f : 0.499f)
                : 0.0001f * std::pow(1000.0f, modulation);
            crossover.SetCyclesPerSample(cutoff);
            Crossover::Vector input(0.2f, 0.2f * std::sin(0.003f * sample),
                0.2f * std::cos(0.7f * sample), sample % 1000 == 0 ? 0.2f : 0.0f);
            auto output = crossover.Process(input);
            for (size_t lane = 0; lane < 4; ++lane)
            {
                finite = finite && std::isfinite(output.m_lowPass[lane]) && std::isfinite(output.m_highPass[lane]);
                peak = std::max(peak, static_cast<double>(std::abs(output.m_lowPass[lane])));
                peak = std::max(peak, static_cast<double>(std::abs(output.m_highPass[lane])));
            }
        }

        DOCTEST_INFO("extreme jumps=" << jumps << " peak=" << peak);
        DOCTEST_CHECK(finite);
        DOCTEST_CHECK(peak < 2);
        crossover.SetCyclesPerSample(0.0001f);
        Crossover::CrossoverOutput tail;
        for (size_t sample = 0; sample < 192000; ++sample)
        {
            tail = crossover.Process(Crossover::Vector());
        }

        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(std::abs(tail.m_lowPass[lane]) < 1e-6f);
            DOCTEST_CHECK(std::abs(tail.m_highPass[lane]) < 1e-6f);
        }
    }
}

namespace
{
template<size_t Channels>
void CheckGeneratedMastering(bool mono)
{
    using Saturator = MultibandSaturator<4, Channels>;
    using Vector = typename Saturator::Vector;
    Saturator saturator;
    typename Saturator::Input controls;
    LinkwitzRileySplitter4BandGeneric<float, 1> reference[Channels]{};
    for (size_t sample = 0; sample < 4096; ++sample)
    {
        controls.m_bassFreq.m_expParam = sample < 2048 ? 0.002f : 0.004f;
        controls.m_crossoverFreqFactor[0].m_expParam = 5;
        controls.m_crossoverFreqFactor[1].m_expParam = 5;
        controls.m_gain[0].m_expParam = 3;
        controls.m_gain[1].m_expParam = 0;
        controls.m_gain[2].m_expParam = 0.5f;
        controls.m_gain[3].m_expParam = sample % 257 == 0 ? 0 : 2;
        controls.m_masterGain.m_expParam = sample % 513 == 0 ? 0 : 1.4f;
        Vector input;
        float raw[4][Channels];
        float mean = 0;
        for (size_t lane = 0; lane < Channels; ++lane)
        {
            input[lane] = std::sin(0.031f * sample + lane) + 0.3f * std::sin(0.73f * sample);
            using Reference = LinkwitzRileySplitter4BandGeneric<float, 1>;
            typename Reference::LogicalSpec spec{};
            spec.m_tree.m_low.m_value = controls.m_bassFreq.m_expParam;
            spec.m_tree.m_value = spec.m_tree.m_low.m_value * controls.m_crossoverFreqFactor[0].m_expParam;
            spec.m_tree.m_high.m_value = spec.m_tree.m_value * controls.m_crossoverFreqFactor[1].m_expParam;
            reference[lane].ComputeCoefficients(spec);
            typename Reference::Input x{input[lane]};
            reference[lane].Process(x);
            for (size_t band = 0; band < 4; ++band)
            {
                raw[band][lane] = reference[lane].m_state.m_bands[band][0];
            }

            mean += raw[0][lane];
        }

        mean /= Channels;
        auto output = saturator.Process(controls, input, mono);
        for (size_t lane = 0; lane < Channels; ++lane)
        {
            float sum = 0;
            float sub = 0;
            for (size_t band = 0; band < 4; ++band)
            {
                const float before = (mono && band == 0 ? mean : raw[band][lane]) * controls.m_gain[band].m_expParam;
                const auto after = TanhSaturatorGeneric<float, 1, false>::Tanh(MultiChannelFloat<1>::Splat(before))[0];
                DOCTEST_CHECK(saturator.m_core.m_output.m_bands[band].m_before[lane] == doctest::Approx(before).epsilon(1e-6).scale(1));
                DOCTEST_CHECK(saturator.m_core.m_output.m_bands[band].m_after[lane] == doctest::Approx(after).epsilon(1e-6).scale(1));
                if (mono && band == 0)
                {
                    sub = after;
                }
                else
                {
                    sum += after;
                }
            }

            const auto before = (sum + sub) * controls.m_masterGain.m_expParam;
            const auto expected = TanhSaturatorGeneric<float, 1, false>::Tanh(MultiChannelFloat<1>::Splat(before))[0];
            DOCTEST_CHECK(output[lane] == doctest::Approx(expected).epsilon(1e-6).scale(1));
            DOCTEST_CHECK(saturator.m_sub == doctest::Approx(sub).epsilon(1e-6).scale(1));
        }

        for (const auto& meter : saturator.m_meter)
        {
            DOCTEST_CHECK(meter.m_frameSamples == (sample + 1) % SampleTimer::x_oversample);
        }

        DOCTEST_CHECK(saturator.m_masterMeter.m_frameSamples == (sample + 1) % SampleTimer::x_oversample);
    }
}
}

DOCTEST_TEST_CASE("MultibandSaturator: generated LR audio, packed cutoffs, meter taps and mono routing")
{
    CheckGeneratedMastering<2>(false);
    CheckGeneratedMastering<2>(true);
    CheckGeneratedMastering<4>(false);
    CheckGeneratedMastering<4>(true);
}

DOCTEST_TEST_CASE("MultibandSaturator: sampled power matches small common-channel input")
{
    using Saturator = MultibandSaturator<4, 2>;
    Saturator::Input input;
    input.m_bassFreq.m_expParam = 0.002f;
    input.m_crossoverFreqFactor[0].m_expParam = 5;
    input.m_crossoverFreqFactor[1].m_expParam = 5;
    input.m_gain[0].m_expParam = 0.5f;
    input.m_gain[1].m_expParam = 2;
    input.m_gain[2].m_expParam = 0;
    input.m_gain[3].m_expParam = 3;
    input.m_masterGain.m_expParam = 1.2f;
    for (bool mono : {false, true})
    {
        for (double frequency : {1.0 / 2048, 17.0 / 2048, 71.0 / 2048, 713.0 / 2048})
        {
            Saturator filter;
            std::complex<double> measured{};
            for (size_t k = 0; k < 16384; ++k)
            {
                const double angle = 2 * M_PI * frequency * k;
                const float x = static_cast<float>(1e-5 * std::cos(angle));
                const float y = filter.Process(input, Saturator::Vector::Splat(x), mono)[0];
                if (8192 <= k)
                {
                    measured += static_cast<double>(y) * std::complex<double>(std::cos(angle), -std::sin(angle));
                }
            }

            measured *= 2.0 / (8192 * 1e-5);
            const auto snapshot = filter.MakeSnapshot();
            const auto power = snapshot.MagnitudeSquaredResponse(ResponseFrequency::CyclesPerSample(frequency), 0);
            DOCTEST_CHECK(power == doctest::Approx(std::norm(measured)).epsilon(0.002).scale(1));
        }
    }
}

DOCTEST_TEST_CASE("Meter: externally computed saturation preserves frame boundaries and reduction")
{
    MeterGeneric<float, 4> direct;
    MeterGeneric<float, 4> recorded;
    for (size_t k = 0; k < 1003; ++k)
    {
        QuadFloat before(0, 1e-15f, -0.1f * (k % 40), 0.03f * (k % 100));
        QuadFloat reduction;
        const auto after = direct.ProcessAndSaturate(before, &reduction);
        const auto externalReduction = recorded.RecordSaturated(before, after);
        DOCTEST_CHECK(reduction == externalReduction);
        DOCTEST_CHECK(direct.m_rms == recorded.m_rms);
        DOCTEST_CHECK(direct.m_peak == recorded.m_peak);
        DOCTEST_CHECK(direct.m_reduction == recorded.m_reduction);
        DOCTEST_CHECK(direct.m_frameSquares == recorded.m_frameSquares);
        DOCTEST_CHECK(direct.m_frameMagnitude == recorded.m_frameMagnitude);
        DOCTEST_CHECK(direct.m_frameReduction == recorded.m_frameReduction);
        DOCTEST_CHECK(direct.m_samplesSincePeak == recorded.m_samplesSincePeak);
        DOCTEST_CHECK(recorded.m_frameSamples == (k + 1) % SampleTimer::x_oversample);
    }
}
