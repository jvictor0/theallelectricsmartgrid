#include "doctest.h"
#include "RecordingDoubleCodec.hpp"
#include "RecordingFormat.hpp"

#include <fstream>
#include <limits>
#include <random>

DOCTEST_TEST_CASE("recording doubles: constants and linear bit patterns have independent golden bytes")
{
    const double epsilon = std::numeric_limits<double>::epsilon();
    const std::vector<double> ramp = {1.0, 1.0 + epsilon, 1.0 + 2 * epsilon, 1.0 + 3 * epsilon};
    std::vector<uint8_t> bytes;
    uint8_t encoding;
    DOCTEST_REQUIRE(RecordingDoubleCodec::Encode(ramp.data(), ramp.size(), bytes, encoding));
    DOCTEST_CHECK(encoding == 1);
    DOCTEST_CHECK(bytes == std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 240, 63, 2, 0, 2});

    const std::vector<double> stopped(48000, 0.0);
    bytes.clear();
    DOCTEST_REQUIRE(RecordingDoubleCodec::Encode(stopped.data(), stopped.size(), bytes, encoding));
    DOCTEST_CHECK(encoding == 1);
    DOCTEST_CHECK(bytes == std::vector<uint8_t>{0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 246, 2});
}

DOCTEST_TEST_CASE("recording doubles: exact finite range and realistic ramps roundtrip through Python")
{
    constexpr size_t x_frames = 48000;
    std::vector<std::vector<double>> examples;
    for (double origin : {0.0, 1048576.25, -1048576.25})
    {
        auto& values = examples.emplace_back();
        double phase = origin;
        for (size_t i = 0; i < x_frames; ++i)
        {
            values.push_back(phase);
            phase += 1.0 / 192000.0;
        }
    }

    auto& changes = examples.emplace_back();
    double phase = 2.0;
    for (size_t i = 0; i < x_frames; ++i)
    {
        if (i == 16000)
        {
            phase = 0.0;
        }

        changes.push_back(phase);
        phase += i < 16000 ? 1.0 / 192000.0 : i < 32000 ? 0.0 : -1.0 / 96000.0;
    }

    auto& extremes = examples.emplace_back();
    std::mt19937_64 random(1234567);
    while (extremes.size() < x_frames)
    {
        const uint64_t bits = random();
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        if (std::isfinite(value))
        {
            extremes.push_back(value);
        }
    }

    extremes[0] = std::numeric_limits<double>::max();
    extremes[1] = std::numeric_limits<double>::lowest();
    extremes[2] = std::numeric_limits<double>::denorm_min();
    extremes[3] = -0.0;
    extremes[4] = 0.0;
    std::vector<double> repeated;
    for (double value : {std::numeric_limits<double>::max(), -0.0, 0.0,
        std::numeric_limits<double>::lowest(), std::numeric_limits<double>::denorm_min(), 1.0})
    {
        repeated.insert(repeated.end(), x_frames / 6, value);
    }

    examples.push_back(std::move(repeated));
    std::vector<uint8_t> fixture;
    for (size_t i = 0; i < examples.size(); ++i)
    {
        std::vector<uint8_t> encoded;
        uint8_t encoding;
        DOCTEST_REQUIRE(RecordingDoubleCodec::Encode(examples[i].data(), x_frames, encoded, encoding));
        DOCTEST_CHECK(encoded.size() <= x_frames * sizeof(double));
        if (i < 3)
        {
            DOCTEST_CHECK(encoding == 1);
            DOCTEST_CHECK(encoded.size() < 1024);
        }
        else if (i == 4)
        {
            DOCTEST_CHECK(encoding == 0);
        }

        RecordingFormat::AppendLE(fixture, x_frames, 4);
        RecordingFormat::AppendLE(fixture, encoding, 1);
        RecordingFormat::AppendLE(fixture, encoded.size(), 4);
        fixture.insert(fixture.end(), encoded.begin(), encoded.end());
        for (double value : examples[i])
        {
            RecordingDoubleCodec::AppendBits(fixture, RecordingDoubleCodec::Bits(value));
        }
    }

    if (const char* path = std::getenv("SMARTGRID_DOUBLE_CODEC_FIXTURE"))
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(fixture.data()), fixture.size());
        DOCTEST_REQUIRE(output.good());
    }
}

DOCTEST_TEST_CASE("recording doubles: nonfinite inputs fail without publishing bytes")
{
    for (double value : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        std::vector<uint8_t> output{42};
        uint8_t encoding;
        DOCTEST_CHECK_FALSE(RecordingDoubleCodec::Encode(&value, 1, output, encoding));
        DOCTEST_CHECK(output == std::vector<uint8_t>{42});
    }
}
