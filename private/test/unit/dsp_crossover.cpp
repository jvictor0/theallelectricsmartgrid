#include "doctest.h"
#include "LinkwitzRileyCrossover.hpp"
#include "MultibandSaturator.hpp"
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

DOCTEST_TEST_CASE("MultibandSaturator: vector band split matches per-channel mono crossovers through cutoff changes")
{
    using Saturator = MultibandSaturator<4, 2>;
    Saturator saturator;
    LinkwitzRileyCrossover mono[2][3];
    for (size_t sample = 0; sample < 4096; ++sample)
    {
        float cutoffs[3] = {0.002f, 0.01f, 0.05f};
        if (2048 <= sample)
        {
            cutoffs[0] = 0.004f;
            cutoffs[1] = 0.02f;
            cutoffs[2] = 0.08f;
        }

        float in[2] = {
            std::sin(0.031f * static_cast<float>(sample)) + 0.3f * std::sin(0.73f * static_cast<float>(sample)),
            ((sample / 61) % 2 == 0 ? 0.5f : -0.5f) + 0.2f * std::sin(0.0071f * static_cast<float>(sample)),
        };

        for (size_t j = 0; j < 3; ++j)
        {
            saturator.m_linkwitzRileyCrossover[j].SetCyclesPerSample(cutoffs[j]);
        }

        Saturator::Vector bands[4];
        saturator.BuildBands<4, 0>(bands, Saturator::Vector(in));
        for (size_t channel = 0; channel < 2; ++channel)
        {
            for (size_t j = 0; j < 3; ++j)
            {
                mono[channel][j].SetCyclesPerSample(cutoffs[j]);
            }

            // Four bands split at the middle crossover, then each half.
            //
            LinkwitzRileyCrossover::CrossoverOutput middle = mono[channel][1].Process(in[channel]);
            LinkwitzRileyCrossover::CrossoverOutput low = mono[channel][0].Process(middle.m_lowPass);
            LinkwitzRileyCrossover::CrossoverOutput high = mono[channel][2].Process(middle.m_highPass);
            float expected[4] = {low.m_lowPass, low.m_highPass, high.m_lowPass, high.m_highPass};
            for (size_t j = 0; j < 4; ++j)
            {
                DOCTEST_CHECK(bands[j][channel] == doctest::Approx(expected[j]).epsilon(0.00001).scale(1.0));
            }
        }
    }
}
