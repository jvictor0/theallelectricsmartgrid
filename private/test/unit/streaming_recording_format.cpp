#include "doctest.h"
#include "RecordingFormat.hpp"
#include "ParamEvent.hpp"

#include <limits>
#include <fstream>

namespace
{

ParamEvent MakeParamEvent(const char* name, size_t sample, int scene, char value)
{
    ParamEvent event;
    event.m_type = ParamEvent::Type::StateChange;
    event.m_sample = sample;
    event.m_name = name;
    event.m_scene = scene;
    event.m_valueLen = 1;
    event.m_value[0] = value;
    return event;
}

}

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
        std::vector<uint8_t> expectedBytes;
        for (size_t i = 0; i < std::strlen(hex); i += 2)
        {
            expectedBytes.push_back(static_cast<uint8_t>(std::stoul(std::string(hex + i, 2), nullptr, 16)));
        }

        // The independent v1 fixture still specifies the exact audio bytes.
        // Upgrade only its envelope to the v3 empty event trailer.
        //
        if (expectedBytes[0] == 'B')
        {
            expectedBytes.resize(expectedBytes.size() - 4);
            expectedBytes[3] = '3';
            expectedBytes.insert(expectedBytes.end(), 4, 0);
            const uint32_t size = static_cast<uint32_t>(expectedBytes.size() + 4);
            for (size_t i = 0; i < 4; ++i)
            {
                expectedBytes[4 + i] = static_cast<uint8_t>(size >> (8 * i));
            }

            RecordingFormat::AppendLE(expectedBytes,
                RecordingFormat::Crc32(expectedBytes.data(), expectedBytes.size()), 4);
        }

        DOCTEST_CHECK(bytes == expectedBytes);
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
    DOCTEST_CHECK(bytes.size() == 30);
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

DOCTEST_TEST_CASE("recording format: param event snapshots the stored scene at its actual width")
{
    uint16_t value = 0x1234;
    State state("Test", sizeof(value), reinterpret_cast<char*>(&value), 8, nullptr);
    state.m_curScene = 3;
    std::memcpy(state.m_buf + 3 * sizeof(value), &value, sizeof(value));
    const auto event = ParamEvent::MkStateChange(&state, 3, 5);
    value = 0x5678;
    DOCTEST_CHECK(static_cast<uint8_t>(event.m_value[0]) == 0x34);
    DOCTEST_CHECK(static_cast<uint8_t>(event.m_value[1]) == 0x12);
    DOCTEST_CHECK(event.m_scene == 3);
    DOCTEST_CHECK(event.m_sample == 5);
}

DOCTEST_TEST_CASE("recording format: new headers declare version three")
{
    RecordingFormat::Session session;
    session.m_gitCommitSha = std::string(40, 'a');
    session.m_recordedAtUtc = "2026-09-19T00:00:00Z";
    session.m_tracks = {{0, "voice", RecordingFormat::TrackType::Mono, "input", "post_fader"}};
    std::vector<uint8_t> bytes;
    DOCTEST_REQUIRE(RecordingFormat::EncodeHeader(session, bytes));
    const std::string text(bytes.begin() + 12, bytes.end());
    JsonArena arena(1024 * 1024);
    const JSON header = arena.Loads(text.c_str());
    DOCTEST_CHECK(header.Get("format_version").IntegerValue() == 3);
}

DOCTEST_TEST_CASE("recording format: event groups sort by name and time with stable ties")
{
    RecordingFormat::Session session;
    session.m_blockFrames = 8;
    session.m_gitCommitSha = std::string(40, 'a');
    session.m_tracks = {{0, "voice", RecordingFormat::TrackType::Mono, "input", "post_fader"}};
    const std::vector<ParamEvent> events =
    {
        MakeParamEvent("B", 12, 0, 9),
        MakeParamEvent("A", 14, 2, 6),
        MakeParamEvent("A", 10, 1, 2),
        MakeParamEvent("A", 10, 1, 3),
    };
    const int32_t samples[8]{};
    std::vector<uint8_t> bytes;
    DOCTEST_REQUIRE(RecordingFormat::EncodeBlock(session, samples, 8, 8, bytes, events));
    DOCTEST_CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "BLK3");
    const std::vector<uint8_t> expected =
    {
        2, 0, 0, 0,
        1, 1, 1, 0, 3, 0, 0, 0, 'A',
        2, 0, 0, 0, 2, 0, 0, 0, 1, 2,
        2, 0, 0, 0, 3, 0, 0, 0, 1, 3,
        6, 0, 0, 0, 1, 0, 0, 0, 2, 6,
        1, 1, 1, 0, 1, 0, 0, 0, 'B',
        4, 0, 0, 0, 0, 0, 0, 0, 0, 9,
    };
    DOCTEST_CHECK(std::vector<uint8_t>(bytes.begin() + 22, bytes.end() - 4) == expected);

    JsonArena arena(4096);
    JSON patch = arena.Object();
    patch.SetNew("unsaved", arena.Integer(42));
    session.m_recordedAtUtc = "2026-09-19T00:00:00Z";
    DOCTEST_REQUIRE(RecordingFormat::EncodeHeader(session, bytes, patch));
    const std::string text(bytes.begin() + 12, bytes.end());
    JsonArena parsed(8192);
    DOCTEST_CHECK(parsed.Loads(text.c_str()).Get("initial_patch").Get("unsaved").IntegerValue() == 42);
}

DOCTEST_TEST_CASE("recording format: parameter types serialize only their own fields")
{
    ParamEvent encoder = ParamEvent::MkBlendSet(0.5f, 12);
    encoder.m_type = ParamEvent::Type::EncoderSet;
    encoder.m_name = "A";
    encoder.m_scene = 2;
    encoder.m_track = 3;
    std::fill(std::begin(encoder.m_encoderPath), std::end(encoder.m_encoderPath), -1);
    encoder.m_encoderPath[0] = 2;
    encoder.m_encoderPath[1] = 0x81;
    ParamEvent active = encoder;
    active.m_type = ParamEvent::Type::EncoderActivate;
    active.m_encoderPath[0] = 0x81;
    active.m_encoderPath[1] = -1;
    active.m_valueLen = 1;
    active.m_value[0] = 1;
    ParamEvent gesture = ParamEvent::MkGestureSet(7, 0.25f, 10);
    gesture.m_scene = -99;
    gesture.m_track = -99;
    ParamEvent blend = ParamEvent::MkBlendSet(0.75f, 11);
    blend.m_gesture = -99;
    std::vector<uint8_t> bytes;
    DOCTEST_REQUIRE(RecordingFormat::AppendParamEvents(bytes, {active, blend, encoder, gesture}, 8, 8));
    const std::vector<uint8_t> expected =
    {
        4, 0, 0, 0,
        2, 4, 0, 0, 1, 0, 0, 0,
        2, 0, 0, 0, 3, 0, 0, 0, 7, 0, 0, 128, 62,
        3, 4, 0, 0, 1, 0, 0, 0,
        3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 64, 63,
        4, 4, 1, 0, 1, 0, 0, 0, 'A',
        4, 0, 0, 0, 2, 0, 0, 0, 2, 3, 2, 2, 129, 0, 0, 0, 63,
        5, 1, 1, 0, 1, 0, 0, 0, 'A',
        4, 0, 0, 0, 0, 0, 0, 0, 2, 3, 1, 129, 1,
    };
    DOCTEST_CHECK(bytes == expected);
}

DOCTEST_TEST_CASE("recording format: rejects invalid typed parameter payloads")
{
    auto gesture = ParamEvent::MkGestureSet(16, 0.5f, 0);
    auto blend = ParamEvent::MkBlendSet(std::numeric_limits<float>::infinity(), 0);
    auto encoder = ParamEvent::MkBlendSet(0.25f, 0);
    encoder.m_type = ParamEvent::Type::EncoderSet;
    encoder.m_name = "Carrier";
    encoder.m_encoderPath[0] = 127;
    auto active = encoder;
    active.m_type = ParamEvent::Type::EncoderActivate;
    active.m_encoderPath[0] = 128;
    active.m_valueLen = 1;
    active.m_value[0] = 2;
    for (auto event : {gesture, blend, encoder, active})
    {
        std::vector<uint8_t> bytes;
        DOCTEST_CHECK_FALSE(RecordingFormat::AppendParamEvents(bytes, {event}, 0, 1));
    }

    encoder.m_encoderPath[0] = -1;
    std::vector<uint8_t> bytes;
    DOCTEST_CHECK(RecordingFormat::AppendParamEvents(bytes, {encoder}, 0, 1));
    DOCTEST_CHECK(bytes.size() == 4 + 8 + 7 + 11 + 4);
    encoder.m_track = 16;
    DOCTEST_CHECK_FALSE(RecordingFormat::AppendParamEvents(bytes, {encoder}, 0, 1));
    encoder.m_track = 0;
    encoder.m_scene = 8;
    DOCTEST_CHECK_FALSE(RecordingFormat::AppendParamEvents(bytes, {encoder}, 0, 1));
}

DOCTEST_TEST_CASE("recording format: patch payloads retain order across differently typed groups")
{
    ParamEvent load;
    load.m_type = ParamEvent::Type::PatchLoad;
    load.m_sample = 10;
    load.m_restoreFaders = true;
    load.m_patchText = "{}";
    load.m_patchBytes = 2;
    ParamEvent snapshot = load;
    snapshot.m_type = ParamEvent::Type::PatchSnapshot;
    auto blend = ParamEvent::MkBlendSet(0.5f, 10);
    std::vector<uint8_t> bytes;
    DOCTEST_REQUIRE(RecordingFormat::AppendParamEvents(bytes, {snapshot, load, blend}, 8, 4));
    const std::vector<uint8_t> expected =
    {
        3, 0, 0, 0,
        3, 4, 0, 0, 1, 0, 0, 0,
        2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 63,
        6, 0, 0, 0, 1, 0, 0, 0,
        2, 0, 0, 0, 1, 0, 0, 0, 1, 2, 0, 0, 0, '{', '}',
        7, 0, 0, 0, 1, 0, 0, 0,
        2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, '{', '}',
    };
    DOCTEST_CHECK(bytes == expected);
}
