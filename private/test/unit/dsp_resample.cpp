// dsp_resample.cpp -- per-sample DownsamplerGeneric / UpsamplerGeneric
// (private/src/Oversample.hpp).
//

#include "doctest.h"

#include <cmath>
#include <random>
#include <vector>

#include "Oversample.hpp"
#include "../support/DirectFormIButterworth.hpp"

namespace
{
    constexpr size_t x_ratio = 4;
    constexpr double x_lowRate = 48000.0;
    constexpr double x_highRate = x_lowRate * x_ratio;

    // Amplitude of the component at cyclesPerSample over an integer number of
    // periods.
    //
    double Amplitude(const std::vector<float>& signal, size_t start, size_t count, double cyclesPerSample)
    {
        double re = 0.0;
        double im = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            double phase = 2.0 * M_PI * cyclesPerSample * static_cast<double>(start + i);
            re += signal[start + i] * std::cos(phase);
            im -= signal[start + i] * std::sin(phase);
        }

        return 2.0 * std::hypot(re, im) / static_cast<double>(count);
    }

    template<size_t Order>
    std::vector<float> Decimate(double inputHz)
    {
        DownsamplerGeneric<float, 1, Order, x_ratio> downsampler;
        std::vector<float> output;
        for (size_t i = 0; i < 9600 * x_ratio; ++i)
        {
            float input = static_cast<float>(std::sin(2.0 * M_PI * inputHz / x_highRate * static_cast<double>(i)));
            downsampler.Process(input);
            if (i % x_ratio == x_ratio - 1)
            {
                output.push_back(downsampler.GetOutput());
            }
        }

        return output;
    }

    template<size_t Order>
    std::vector<float> Interpolate(double inputHz)
    {
        UpsamplerGeneric<float, 1, Order, x_ratio> upsampler;
        std::vector<float> output;
        for (size_t i = 0; i < 9600 * x_ratio; ++i)
        {
            if (i % x_ratio == 0)
            {
                size_t lowIndex = i / x_ratio;
                upsampler.Push(static_cast<float>(std::sin(2.0 * M_PI * inputHz / x_lowRate * static_cast<double>(lowIndex))));
            }

            output.push_back(upsampler.Process());
        }

        return output;
    }

    template<typename Number, size_t Order, size_t Ratio>
    void CheckDecimatorAgainstDirectFormI()
    {
        using Decimator = DownsamplerGeneric<Number, 4, Order, Ratio>;
        using Vector = typename Decimator::Vector;
        Decimator decimator;
        DirectFormIButterworth<double, 4, Order> reference;
        reference.SetCyclesPerSample(Decimator::x_cutoff / static_cast<float>(Ratio));

        std::mt19937 random(7219);
        std::uniform_real_distribution<double> distribution(-1.0, 1.0);
        for (size_t i = 0; i < 1024; ++i)
        {
            if (i == 512)
            {
                decimator.Reset();
                reference.Reset();
                DOCTEST_CHECK(decimator.GetOutput() == Vector());
            }

            Vector input;
            MultiChannelNumber<double, 4> referenceInput;
            for (int lane = 0; lane < 4; ++lane)
            {
                input[lane] = static_cast<Number>(distribution(random));
                referenceInput[lane] = input[lane];
            }

            Vector output = decimator.Process(input);
            auto expected = reference.Process(referenceInput);
            DOCTEST_CHECK(output == decimator.GetOutput());
            for (int lane = 0; lane < 4; ++lane)
            {
                DOCTEST_CHECK(std::abs(output[lane] - expected[lane]) < 1.5e-5);
            }
        }
    }
}

DOCTEST_TEST_CASE("DownsamplerGeneric: passband is flat and the stopband is rejected")
{
    // 1 kHz passes; 40 kHz aliases to 8 kHz at the low rate and must be gone.
    //
    std::vector<float> passband = Decimate<8>(1000.0);
    DOCTEST_CHECK(std::abs(Amplitude(passband, 4800, 4800, 1000.0 / x_lowRate) - 1.0) < 0.012);

    std::vector<float> stopband = Decimate<8>(40000.0);
    DOCTEST_CHECK(Amplitude(stopband, 4800, 4800, 8000.0 / x_lowRate) < 0.01);
}

DOCTEST_TEST_CASE("DownsamplerGeneric: gentle order still passes and attenuates")
{
    std::vector<float> passband = Decimate<2>(1000.0);
    DOCTEST_CHECK(std::abs(Amplitude(passband, 4800, 4800, 1000.0 / x_lowRate) - 1.0) < 0.012);

    std::vector<float> stopband = Decimate<2>(40000.0);
    DOCTEST_CHECK(Amplitude(stopband, 4800, 4800, 8000.0 / x_lowRate) < 0.5);
}

DOCTEST_TEST_CASE("DownsamplerGeneric: Lean decimators match direct form I across a reset")
{
    CheckDecimatorAgainstDirectFormI<float, 2, 4>();
    CheckDecimatorAgainstDirectFormI<float, 4, 4>();
    CheckDecimatorAgainstDirectFormI<float, 8, 4>();
    CheckDecimatorAgainstDirectFormI<double, 2, 2>();
    CheckDecimatorAgainstDirectFormI<double, 8, 8>();
}

DOCTEST_TEST_CASE("UpsamplerGeneric: unity passband gain and rejected images")
{
    // A 1 kHz input keeps unit amplitude; its first image at 47 kHz is gone.
    //
    std::vector<float> output = Interpolate<8>(1000.0);
    DOCTEST_CHECK(std::abs(Amplitude(output, 19200, 19200, 1000.0 / x_highRate) - 1.0) < 0.012);
    DOCTEST_CHECK(Amplitude(output, 19200, 19200, 47000.0 / x_highRate) < 0.01);
}

DOCTEST_TEST_CASE("DownsamplerGeneric / UpsamplerGeneric: ratio 1 is a passthrough")
{
    using Vector = MultiChannelNumber<float, 4>;
    DownsamplerGeneric<float, 4, 2, 1> secondOrder;
    DownsamplerGeneric<float, 4, 8, 1> downsampler;
    UpsamplerGeneric<float, 4, 8, 1> upsampler;

    std::mt19937 random(4417);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    for (size_t i = 0; i < 256; ++i)
    {
        Vector input(distribution(random), distribution(random), distribution(random), distribution(random));
        DOCTEST_CHECK(secondOrder.Process(input) == input);
        DOCTEST_CHECK(secondOrder.GetOutput() == input);
        DOCTEST_CHECK(downsampler.Process(input) == input);
        DOCTEST_CHECK(downsampler.GetOutput() == input);

        upsampler.Push(input);
        DOCTEST_CHECK(upsampler.Process() == input);
    }

    secondOrder.Reset();
    downsampler.Reset();
    DOCTEST_CHECK(secondOrder.GetOutput() == Vector());
    DOCTEST_CHECK(downsampler.GetOutput() == Vector());
}

DOCTEST_TEST_CASE("DownsamplerGeneric / UpsamplerGeneric: lanes match mono instances")
{
    using Vector = MultiChannelNumber<float, 4>;
    DownsamplerGeneric<float, 4, 8, x_ratio> downsampler;
    DownsamplerGeneric<float, 1, 8, x_ratio> monoDownsampler[4];
    UpsamplerGeneric<float, 4, 4, x_ratio> upsampler;
    UpsamplerGeneric<float, 1, 4, x_ratio> monoUpsampler[4];

    std::mt19937 random(9029);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    for (size_t i = 0; i < 1024; ++i)
    {
        Vector input(distribution(random), distribution(random), distribution(random), distribution(random));
        Vector output = downsampler.Process(input);

        if (i % x_ratio == 0)
        {
            upsampler.Push(input);
            for (size_t lane = 0; lane < 4; ++lane)
            {
                monoUpsampler[lane].Push(input[lane]);
            }
        }

        Vector upsampled = upsampler.Process();
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CHECK(output[lane] == static_cast<float>(monoDownsampler[lane].Process(input[lane])));
            DOCTEST_CHECK(upsampled[lane] == static_cast<float>(monoUpsampler[lane].Process()));
        }
    }
}
