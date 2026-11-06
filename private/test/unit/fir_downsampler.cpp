#include "doctest.h"
#include "ButterworthFilter.hpp"
#include <cmath>

DOCTEST_TEST_CASE("FIR downsampler: trio lanes match independent filters and reset cadence")
{
    using Filter = FIRDownsamplerGeneric<float, 3, 17, 4>;
    Filter vector;
    FIRDownsamplerGeneric<float, 1, 17, 4> scalar[3];
    for (size_t sample = 0; sample < 1024; ++sample)
    {
        MultiChannelNumber<float, 3> input(std::sin(sample * 0.03),
            sample == 31 ? 1.0f : 0.0f, 0.25f);
        bool emitted = vector.Process(input);
        DOCTEST_CHECK(emitted == (sample % 4 == 3));
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(scalar[lane].Process(input[lane]) == emitted);
            if (emitted)
            {
                DOCTEST_CHECK(vector.GetOutput()[lane] == doctest::Approx(scalar[lane].GetOutput()[0]));
            }
        }
    }

    vector.Reset();
    Filter fresh;
    for (size_t sample = 0; sample < 128; ++sample)
    {
        auto input = Filter::Vector::Splat(sample % 7);
        DOCTEST_CHECK(vector.Process(input) == fresh.Process(input));
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(vector.GetOutput()[lane] == fresh.GetOutput()[lane]);
        }
    }
}

DOCTEST_TEST_CASE("FIR downsampler: preserves DC and ramp timing and rejects aliased tones")
{
    FIRDownsamplerGeneric<double, 4, 65, 4> filter;
    double stopPower = 0.0;
    double passPower = 0.0;
    size_t outputs = 0;
    for (size_t sample = 0; sample < 8192; ++sample)
    {
        MultiChannelNumber<double, 4> input(1.0, static_cast<double>(sample),
            std::sin(2.0 * M_PI * 0.02 * sample), std::sin(2.0 * M_PI * 0.20 * sample));
        if (filter.Process(input) && sample > 128)
        {
            const auto& output = filter.GetOutput();
            DOCTEST_CHECK(output[0] == doctest::Approx(1.0));
            DOCTEST_CHECK(output[1] == doctest::Approx(sample - filter.x_groupDelay));
            passPower += output[2] * output[2];
            stopPower += output[3] * output[3];
            ++outputs;
        }
    }

    DOCTEST_CHECK(passPower / outputs > 0.49);
    DOCTEST_CHECK(stopPower / outputs < 0.000001);
}

DOCTEST_TEST_CASE("FIR downsampler: 17 taps preserve slow motion and attenuate a folded tone by over 30 dB")
{
    FIRDownsamplerGeneric<double, 2, 17, 4> filter;
    double passPower = 0.0;
    double stopPower = 0.0;
    size_t outputs = 0;
    for (size_t sample = 0; sample < 8192; ++sample)
    {
        MultiChannelNumber<double, 2> input(std::sin(2.0 * M_PI * 0.02 * sample),
            std::sin(2.0 * M_PI * 0.20 * sample));
        if (filter.Process(input) && sample > 128)
        {
            passPower += filter.GetOutput()[0] * filter.GetOutput()[0];
            stopPower += filter.GetOutput()[1] * filter.GetOutput()[1];
            ++outputs;
        }
    }

    DOCTEST_CHECK(filter.x_groupDelay == 8);
    DOCTEST_CHECK(passPower / outputs > 0.47);
    DOCTEST_CHECK(stopPower / outputs < 0.0005);
}

DOCTEST_TEST_CASE("FIR downsampler: ratio one has no delay")
{
    FIRDownsamplerGeneric<float, 3, 17, 1> filter;
    DOCTEST_CHECK(filter.x_groupDelay == 0);
    for (size_t sample = 0; sample < 100; ++sample)
    {
        MultiChannelNumber<float, 3> input(sample, -static_cast<float>(sample), 2.0f);
        DOCTEST_REQUIRE(filter.Process(input));
        for (size_t lane = 0; lane < 3; ++lane)
        {
            DOCTEST_CHECK(filter.GetOutput()[lane] == input[lane]);
        }
    }
}
