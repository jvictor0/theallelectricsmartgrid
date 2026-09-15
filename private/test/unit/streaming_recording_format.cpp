#include "doctest.h"
#include "RecordingFormat.hpp"

#include <limits>
#include <fstream>

DOCTEST_TEST_CASE("recording format: preserves legacy PCM rounding and position endpoints")
{
    DOCTEST_CHECK(RecordingFormat::Quantize(-2.0f) == -8388607);
    DOCTEST_CHECK(RecordingFormat::Quantize(-1.0f) == -8388607);
    DOCTEST_CHECK(RecordingFormat::Quantize(-0.5f) == -4194304);
    DOCTEST_CHECK(RecordingFormat::Quantize(0.0f) == 0);
    DOCTEST_CHECK(RecordingFormat::Quantize(0.5f) == 4194304);
    DOCTEST_CHECK(RecordingFormat::Quantize(2.0f) == 8388607);
    DOCTEST_CHECK(RecordingFormat::Quantize(-0.5f, true) == 0);
    DOCTEST_CHECK(RecordingFormat::Quantize(0.5f, true) == 4194304);
    DOCTEST_CHECK(RecordingFormat::Quantize(1.5f, true) == 8388607);
}

DOCTEST_TEST_CASE("recording format: encoded streams match independent packed bytes")
{
    struct Example
    {
        std::vector<int32_t> m_values;
        uint8_t m_encoding;
        uint8_t m_width;
        std::vector<uint8_t> m_bytes;
    };

    const Example examples[] =
    {
        {{10, 10, 10, 10, 10}, 1, 0, {10, 0, 0}},
        {{4, 3, 3, 2, 1}, 1, 1, {4, 0, 0, 13}},
        {{0, 1, 2, 3, 4}, 1, 2, {0, 0, 0, 85}},
        {{5, 3, 6, 5}, 1, 3, {5, 0, 0, 222, 1}},
        {{-8388608, 8388607}, 0, 24, {0, 0, 128, 255, 255, 127}},
        {{-1}, 0, 24, {255, 255, 255}},
    };

    for (const auto& example : examples)
    {
        auto encoding = RecordingFormat::ChooseEncoding(example.m_values.data(), example.m_values.size());
        DOCTEST_CHECK(encoding.m_encoding == example.m_encoding);
        DOCTEST_CHECK(encoding.m_width == example.m_width);
        std::vector<uint8_t> bytes;
        RecordingFormat::AppendStream(bytes, example.m_values.data(), example.m_values.size(), encoding);
        DOCTEST_CHECK(bytes == example.m_bytes);
    }
}

DOCTEST_TEST_CASE("recording format: signed width boundaries and raw ties")
{
    for (uint8_t width = 1; width <= 24; ++width)
    {
        std::vector<int32_t> values(64, -(int32_t{1} << (width - 1)));
        values[0] = 0;
        const auto encoding = RecordingFormat::ChooseEncoding(values.data(), values.size());
        DOCTEST_CHECK(encoding.m_width == width);
        DOCTEST_CHECK(encoding.m_encoding == (width == 24 ? 0 : 1));
    }
}

DOCTEST_TEST_CASE("recording format: master metadata matches reader contracts")
{
    RecordingFormat::Session session;
    session.m_gitCommitSha = std::string(40, 'a');
    session.m_tracks = {{0, "master", RecordingFormat::TrackType::Stereo,
        "master_stereo", "post_mastering_pre_master_volume"}};
    DOCTEST_REQUIRE(RecordingFormat::Validate(session));
    session.m_tracks[0].m_type = RecordingFormat::TrackType::Mono;
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(session));
    session.m_tracks[0].m_type = RecordingFormat::TrackType::Stereo;
    session.m_tracks[0].m_tap = "post_volume";
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(session));
    session.m_tracks[0].m_tap = "post_mastering_pre_master_volume";
    session.m_tracks.push_back(session.m_tracks[0]);
    session.m_tracks.back().m_id = 1;
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(session));
}

DOCTEST_TEST_CASE("recording format: all track types match the independent Python golden records")
{
    std::ifstream input(std::string(SMARTGRID_REPO_ROOT) + "/private/test/fixtures/streaming-recording/golden.json");
    DOCTEST_REQUIRE(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    JsonArena arena(1024 * 1024);
    JSON expected = arena.Loads(text.c_str());
    DOCTEST_REQUIRE(!expected.IsNull());
    JSON header = expected.Get("header");
    RecordingFormat::Session session;
    session.m_sampleRate = header.Get("sample_rate").IntegerValue();
    session.m_blockFrames = header.Get("block_frames").IntegerValue();
    session.m_recordedAtUtc = header.Get("recorded_at_utc").StringValue();
    session.m_gitCommitSha = header.Get("git_commit_sha").StringValue();
    JSON tracks = header.Get("tracks");
    for (size_t i = 0; i < tracks.Size(); ++i)
    {
        JSON track = tracks.GetAt(i);
        const std::string type = track.Get("type").StringValue();
        const auto trackType = type == "mono" ? RecordingFormat::TrackType::Mono
            : type == "panned_mono" ? RecordingFormat::TrackType::PannedMono
            : type == "stereo" ? RecordingFormat::TrackType::Stereo : RecordingFormat::TrackType::Quad;
        session.m_tracks.push_back({static_cast<uint32_t>(track.Get("id").IntegerValue()),
            track.Get("name").StringValue(), trackType, track.Get("role").StringValue(), track.Get("tap").StringValue()});
    }

    std::vector<uint8_t> bytes;
    DOCTEST_REQUIRE(RecordingFormat::EncodeHeader(session, bytes));
    std::vector<uint8_t> file = bytes;
    auto checkHex = [&](const char* hex)
    {
        const size_t size = std::strlen(hex) / 2;
        DOCTEST_REQUIRE(bytes.size() == size);
        for (size_t i = 0; i < size; ++i)
        {
            const std::string pair(hex + 2 * i, 2);
            DOCTEST_CHECK(bytes[i] == std::stoul(pair, nullptr, 16));
        }
    };

    JSON blocks = expected.Get("blocks");
    for (size_t i = 0; i < blocks.Size(); ++i)
    {
        JSON block = blocks.GetAt(i);
        const uint32_t frames = block.Get("frame_count").IntegerValue();
        std::vector<int32_t> samples(session.StreamCount() * session.m_blockFrames);
        size_t streamOffset = 0;
        for (const auto& track : session.m_tracks)
        {
            JSON streams = block.Get("tracks").Get(std::to_string(track.m_id).c_str());
            for (size_t stream = 0; stream < streams.Size(); ++stream)
            {
                for (size_t frame = 0; frame < frames; ++frame)
                {
                    samples[streamOffset * session.m_blockFrames + frame] = streams.GetAt(stream).GetAt(frame).IntegerValue();
                }

                ++streamOffset;
            }
        }

        DOCTEST_REQUIRE(RecordingFormat::EncodeBlock(session, samples.data(), frames,
            block.Get("start_frame").IntegerValue(), bytes));
        checkHex(block.Get("record_hex").StringValue());
        file.insert(file.end(), bytes.begin(), bytes.end());
    }

    RecordingFormat::EncodeEnd(expected.Get("total_frames").IntegerValue(), bytes);
    checkHex(expected.Get("completion_hex").StringValue());
    file.insert(file.end(), bytes.begin(), bytes.end());
    if (const char* path = std::getenv("SMARTGRID_CODEC_FIXTURE"))
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(file.data()), file.size());
        DOCTEST_REQUIRE(output.good());
    }
}

DOCTEST_TEST_CASE("recording format: sparse blocks derive stream layout and carry CRC")
{
    RecordingFormat::Session session;
    session.m_blockFrames = 4;
    session.m_gitCommitSha = std::string(40, 'a');
    session.m_recordedAtUtc = "2026-09-15T00:00:00Z";
    session.m_tracks =
    {
        {0, "voice", RecordingFormat::TrackType::PannedMono, "input", "post_fader"},
        {1, "sub", RecordingFormat::TrackType::Mono, "mono_input", "post_reduction"},
    };

    DOCTEST_REQUIRE(RecordingFormat::Validate(session));
    std::vector<int32_t> samples(16, 0);
    std::fill(samples.begin() + 4, samples.begin() + 12, 4194304);
    std::vector<uint8_t> bytes;
    DOCTEST_REQUIRE(RecordingFormat::EncodeBlock(session, samples.data(), 4, 0, bytes));
    DOCTEST_CHECK(bytes.size() == 26);
    DOCTEST_CHECK(bytes[20] == 0);
    DOCTEST_CHECK(bytes[21] == 0);

    samples[0] = 1;
    DOCTEST_REQUIRE(RecordingFormat::EncodeBlock(session, samples.data(), 4, 0, bytes));
    DOCTEST_CHECK(bytes[20] == 1);
    DOCTEST_CHECK(bytes[22] == 0);
    DOCTEST_CHECK(bytes[26] == 1);
    DOCTEST_CHECK(bytes[27] == 1);
    DOCTEST_CHECK(bytes[28] == 1);
    DOCTEST_CHECK(bytes[29] == 0);
    DOCTEST_CHECK(bytes[30] == 1);
    DOCTEST_CHECK(bytes[31] == 0);

    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    DOCTEST_CHECK(RecordingFormat::Crc32(check, sizeof(check)) == 0xcbf43926u);
    RecordingFormat::EncodeEnd(7, bytes);
    DOCTEST_CHECK(bytes.size() == 16);
    DOCTEST_CHECK(bytes[4] == 7);

    DOCTEST_REQUIRE(RecordingFormat::EncodeHeader(session, bytes));
    DOCTEST_CHECK(std::string(bytes.begin(), bytes.begin() + 8) == "SMRTGRID");
    session.m_tracks.push_back(session.m_tracks[0]);
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(session));
    session.m_tracks.pop_back();
    session.m_blockFrames = std::numeric_limits<uint32_t>::max();
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(session));
}
