#include "doctest.h"
#include "OLA.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace
{

using OLA = OLAGeneric<float, 4, 5, 8>;
using Sample = std::array<double, 4>;

double InterpolateReference(const std::vector<Sample>& samples, double position, size_t channel)
{
    int left = static_cast<int>(std::floor(position));
    double fraction = position - left;
    auto at = [&](int index)
    {
        return index < 0 || index >= static_cast<int>(samples.size()) ? 0.0 : samples[index][channel];
    };
    double start = at(left);
    double end = at(left + 1);
    double startSlope = (end - at(left - 1)) / 2.0;
    double endSlope = (at(left + 2) - start) / 2.0;
    double square = fraction * fraction;
    double cube = square * fraction;
    return (2 * cube - 3 * square + 1) * start + (cube - 2 * square + fraction) * startSlope +
        (-2 * cube + 3 * square) * end + (cube - square) * endSlope;
}

}

DOCTEST_TEST_CASE("OLA PM: cubic reads match an unbounded overlap sum through motion and wraps")
{
    constexpr size_t x_hop = 8;
    constexpr size_t x_frames = 80;
    constexpr size_t x_samples = x_frames * x_hop + OLA::x_bufferSize;
    static_assert(OLA::x_bufferSize == 6 * x_hop);
    bool writeAfterRead = false;
    DOCTEST_SUBCASE("Write before Process")
    {
    }

    DOCTEST_SUBCASE("Process before Write as in GrainManager")
    {
        writeAfterRead = true;
    }

    std::vector<OLA::Frame> frames(x_frames);
    std::vector<Sample> reference(x_samples);
    for (size_t frame = 0; frame < x_frames; ++frame)
    {
        for (size_t i = 0; i < OLA::x_frameSize; ++i)
        {
            for (size_t channel = 0; channel < 4; ++channel)
            {
                float value = static_cast<float>(0.3 * std::sin(0.37 * i + frame + channel) + 0.01 * frame);
                frames[frame].m_table[i][channel] = value;
                reference[frame * x_hop + i + (writeAfterRead ? 1 : 0)][channel] += value;
            }
        }
    }

    OLA modulated;
    OLA neutral;
    const float x_offsets[] = {-6.0f, 6.0f, -5.875f, 5.875f, 0.0f, 0.375f, -0.5f};
    for (size_t sample = 0; sample < x_samples; ++sample)
    {
        bool launch = sample % x_hop == 0 && sample / x_hop < x_frames;
        if (launch && !writeAfterRead)
        {
            modulated.Write(frames[sample / x_hop]);
            neutral.Write(frames[sample / x_hop]);
        }

        OLA::Vector offsets;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            offsets[channel] = x_offsets[(sample + 2 * channel) % std::size(x_offsets)];
        }

        auto output = modulated.Process(offsets);
        auto unmodulated = neutral.Process();
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CAPTURE(sample);
            DOCTEST_CAPTURE(channel);
            double position = static_cast<double>(sample) - x_hop;
            DOCTEST_REQUIRE(output[channel] == doctest::Approx(
                InterpolateReference(reference, position + offsets[channel], channel)).epsilon(2e-6).scale(1.0));
            DOCTEST_REQUIRE(unmodulated[channel] == doctest::Approx(
                InterpolateReference(reference, position, channel)).epsilon(2e-6).scale(1.0));
        }

        if (launch && writeAfterRead)
        {
            modulated.Write(frames[sample / x_hop]);
            neutral.Write(frames[sample / x_hop]);
        }
    }
}

DOCTEST_TEST_CASE("OLA PM: clearing a channel silences retained history and queued output")
{
    OLA cleared;
    OLA reference;
    OLA::Frame frame;
    for (size_t i = 0; i < OLA::x_frameSize; ++i)
    {
        frame.m_table[i] = OLA::Vector::Splat(1.0f + static_cast<float>(i) / OLA::x_frameSize);
    }

    for (size_t sample = 0; sample < 100; ++sample)
    {
        if (sample % 8 == 0)
        {
            cleared.Write(frame);
            reference.Write(frame);
        }

        cleared.Process();
        reference.Process();
    }

    cleared.ClearChannel(0);
    double retainedEnergy = 0.0;
    for (size_t sample = 0; sample < 2 * OLA::x_bufferSize; ++sample)
    {
        auto offset = OLA::Vector::Splat(sample % 2 == 0 ? -6.0f : 5.75f);
        auto actual = cleared.Process(offset);
        auto expected = reference.Process(offset);
        DOCTEST_REQUIRE(actual[0] == 0.0f);
        retainedEnergy += expected[0] * expected[0];
        for (size_t channel = 1; channel < 4; ++channel)
        {
            DOCTEST_REQUIRE(actual[channel] == expected[channel]);
        }

        if (sample >= OLA::x_bufferSize)
        {
            DOCTEST_REQUIRE(expected[0] == 0.0f);
        }
    }

    DOCTEST_CHECK(retainedEnergy > 1.0);
}
