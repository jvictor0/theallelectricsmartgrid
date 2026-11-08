#include "doctest.h"
#include "Wavefolder.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace
{

double TanhReference(double x)
{
    return std::clamp(x * (27 + x * x) / (27 + 9 * x * x), -1.0, 1.0);
}

std::array<double, 7> Landmarks(double x)
{
    double t = TanhReference(x);
    double s = std::sin(x);
    return {t, t - t * t * t, t - t * t * s, t - t * s * s, t - s * s * s, s - s * s * s, s};
}

double RawOutput(float gain, float shape, float input)
{
    Wavefolder folder;
    folder.SetGain(gain);
    folder.Process(input, shape);
    return folder.m_dcBlocker.m_prevInput;
}

}

DOCTEST_TEST_CASE("Wavefolder: normalized landmarks and continuous adjacent morphs")
{
    for (float gain : {0.125f, 0.5f, 0.578f, 0.598f, 0.621f, 1.0f, static_cast<float>(M_PI / 2),
        2.0f, 2.5f, static_cast<float>(M_PI), 4.0f, static_cast<float>(1.5 * M_PI), 8.0f})
    {
        // Independently scan the full reachable interval, including interior
        // peaks and later lobes of the mixed folds.
        //
        std::array<double, 7> peaks{};
        for (int sample = 0; sample <= 16384; ++sample)
        {
            auto values = Landmarks(gain * sample / 16384.0);
            for (size_t shape = 0; shape < peaks.size(); ++shape)
            {
                peaks[shape] = std::max(peaks[shape], std::abs(values[shape]));
            }
        }

        for (float input : {-1.0f, -0.7f, -0.2f, 0.0f, 0.2f, 0.7f, 1.0f})
        {
            auto values = Landmarks(gain * input);
            for (size_t shape = 0; shape < peaks.size(); ++shape)
            {
                const float position = static_cast<float>(shape) / 6;
                const double output = RawOutput(gain, position, input);
                DOCTEST_INFO("gain=" << gain << " shape=" << shape << " input=" << input);
                DOCTEST_CHECK(output == doctest::Approx(values[shape] / peaks[shape]).epsilon(0.00003).scale(1.0));
                for (float offset : {-0.000001f, 0.000001f})
                {
                    const double neighbor = RawOutput(gain, std::clamp(position + offset, 0.0f, 1.0f), input);
                    DOCTEST_CHECK(std::abs(output - neighbor) < 0.00003);
                }

                if (shape + 1 < peaks.size())
                {
                    double midpoint = RawOutput(gain, (static_cast<float>(shape) + 0.5f) / 6, input);
                    double expected = 0.5 * (values[shape] / peaks[shape] + values[shape + 1] / peaks[shape + 1]);
                    DOCTEST_CHECK(midpoint == doctest::Approx(expected).epsilon(0.00003).scale(1.0));
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("Wavefolder: silent modulation stays within numerical residue and controls preserve the DC tail")
{
    Wavefolder folder;
    for (int sample = 0; sample < 1000; ++sample)
    {
        folder.SetGain(0.125f + 7.875f * sample / 999);
        DOCTEST_CHECK(std::abs(folder.Process(0.0f, static_cast<float>(sample % 101) / 100)) < 0.00001f);
    }

    folder.SetGain(1.0f);
    for (size_t sample = 0; sample < 128; ++sample)
    {
        folder.Process(1.0f, 0.0f);
    }

    const double alpha = std::exp(-2 * M_PI * 5 / SampleTimer::x_sampleRate);
    DOCTEST_CHECK(folder.m_dcBlocker.m_alpha == doctest::Approx(alpha).epsilon(0.0000001));
    double tail = folder.Process(0.0f, 0.0f);
    DOCTEST_REQUIRE(std::abs(tail) > 0.0001);
    const size_t samples = static_cast<size_t>(SampleTimer::SecondsToSamples(0.1));
    for (size_t sample = 0; sample < samples; ++sample)
    {
        folder.SetGain(sample % 2 == 0 ? 8.0f : 0.125f);
        folder.Process(0.0f, static_cast<float>(sample % 101) / 100);
    }

    DOCTEST_CHECK(folder.m_output == doctest::Approx(tail * std::exp(-M_PI)).epsilon(0.002));
}

DOCTEST_TEST_CASE("Wavefolder: odd landmarks retain the fundamental at low gain")
{
    constexpr size_t x_period = SampleTimer::x_sampleRate / 100;
    constexpr size_t x_settle = SampleTimer::x_sampleRate;
    constexpr size_t x_capture = 8 * x_period;
    for (size_t shape = 0; shape < Wavefolder::x_numShapes; ++shape)
    {
        for (float gain : {0.125f, 2.0f, 8.0f})
        {
            Wavefolder folder;
            folder.SetGain(gain);
            std::vector<double> signal(x_capture);
            double mean = 0;
            double rms = 0;
            for (size_t sample = 0; sample < x_settle + x_capture; ++sample)
            {
                float input = static_cast<float>(std::sin(2 * M_PI * (sample % x_period) / x_period));
                double output = folder.Process(input, static_cast<float>(shape) / 6);
                DOCTEST_REQUIRE(std::isfinite(output));
                if (sample >= x_settle)
                {
                    signal[sample - x_settle] = output;
                    mean += output / x_capture;
                    rms += output * output / x_capture;
                }
            }

            double oddEnergy = 0;
            double evenEnergy = 0;
            double fundamentalEnergy = 0;
            for (size_t harmonic = 1; harmonic <= 32; ++harmonic)
            {
                double real = 0;
                double imag = 0;
                for (size_t sample = 0; sample < x_capture; ++sample)
                {
                    double phase = 2 * M_PI * harmonic * (sample % x_period) / x_period;
                    real += signal[sample] * std::cos(phase) / x_capture;
                    imag += signal[sample] * std::sin(phase) / x_capture;
                }

                double energy = real * real + imag * imag;
                (harmonic % 2 == 0 ? evenEnergy : oddEnergy) += energy;
                if (harmonic == 1)
                {
                    fundamentalEnergy = energy;
                }
            }

            DOCTEST_INFO("shape=" << shape << " gain=" << gain << " mean=" << mean);
            DOCTEST_CHECK(std::abs(mean) < 0.0005);
            DOCTEST_CHECK(rms > 0.01);
            DOCTEST_CHECK(evenEnergy < 0.000001 * oddEnergy);
            DOCTEST_CHECK(oddEnergy > 0.01);
            if (gain == 0.125f)
            {
                DOCTEST_CHECK(fundamentalEnergy > 0.99 * oddEnergy);
            }
        }
    }
}

DOCTEST_TEST_CASE("Wavefolder: trio lanes match mono processors during independent modulation")
{
    TrioWavefolder trio;
    Wavefolder mono[3];
    double difference[3]{};
    double magnitude[3]{};
    for (size_t sample = 0; sample < 12000; ++sample)
    {
        TrioWavefolder::Vector input;
        TrioWavefolder::Vector gain;
        TrioWavefolder::Vector shape;
        for (size_t lane = 0; lane < 3; ++lane)
        {
            input[lane] = static_cast<float>(std::sin(0.017 * (lane + 1) * sample));
            gain[lane] = 0.125f + 7.875f * static_cast<float>((sample / 31 + 23 * lane) % 101) / 100;
            shape[lane] = static_cast<float>((sample / 17 + 37 * lane) % 101) / 100;
        }

        trio.SetGain(gain);
        auto output = trio.Process(input, shape);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            mono[lane].SetGain(gain[lane]);
            double expected = mono[lane].Process(input[lane], shape[lane]);
            DOCTEST_REQUIRE(std::isfinite(output[lane]));
            difference[lane] = std::max(difference[lane], std::abs(output[lane] - expected));
            magnitude[lane] = std::max(magnitude[lane], std::abs(expected));
        }
    }

    for (size_t lane = 0; lane < 3; ++lane)
    {
        DOCTEST_CHECK(difference[lane] < 0.00002);
        DOCTEST_CHECK(magnitude[lane] > 0.1);
    }
}
