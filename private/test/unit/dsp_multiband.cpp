#include "doctest.h"
#include "MultibandSaturator.hpp"
#include "LinkwitzRileySplitter4BandGeneric.hpp"
#include "SampleTimer.hpp"
#include <cmath>
#include <complex>

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
