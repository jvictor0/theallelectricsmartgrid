#include "doctest.h"
#include "RecordingFlacCodec.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <cstdio>
#include <stdbool.h>

#if defined(SMARTGRID_RECORDING_SYSTEM_FLAC)
#include <FLAC/stream_decoder.h>
#else
namespace juce
{
namespace FlacNamespace
{
#include <juce_audio_formats/codecs/flac/stream_decoder.h>
}
}
using namespace juce::FlacNamespace;
#endif

namespace
{
struct FlacDecoded
{
    const std::vector<uint8_t>& m_bytes;
    size_t m_cursor = 7;
    std::vector<int32_t> m_samples;
    unsigned m_rate = 0;
    unsigned m_depth = 0;
    unsigned m_channels = 0;
    uint64_t m_frames = 0;
    unsigned m_maxBlock = 0;
    bool m_error = false;

    static FLAC__StreamDecoderReadStatus Read(const FLAC__StreamDecoder*, FLAC__byte* buffer, size_t* bytes, void* client)
    {
        auto& self = *static_cast<FlacDecoded*>(client);
        *bytes = std::min(*bytes, self.m_bytes.size() - self.m_cursor);
        std::memcpy(buffer, self.m_bytes.data() + self.m_cursor, *bytes);
        self.m_cursor += *bytes;
        return *bytes ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE : FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }

    static FLAC__StreamDecoderWriteStatus Write(const FLAC__StreamDecoder*, const FLAC__Frame* frame, const FLAC__int32* const buffers[], void* client)
    {
        auto& self = *static_cast<FlacDecoded*>(client);
        self.m_maxBlock = std::max(self.m_maxBlock, frame->header.blocksize);
        self.m_samples.insert(self.m_samples.end(), buffers[0], buffers[0] + frame->header.blocksize);
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    static void Metadata(const FLAC__StreamDecoder*, const FLAC__StreamMetadata* metadata, void* client)
    {
        auto& self = *static_cast<FlacDecoded*>(client);
        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
        {
            const auto& info = metadata->data.stream_info;
            self.m_rate = info.sample_rate;
            self.m_depth = info.bits_per_sample;
            self.m_channels = info.channels;
            self.m_frames = info.total_samples;
        }
    }

    static void Error(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus, void* client)
    {
        static_cast<FlacDecoded*>(client)->m_error = true;
    }
};

void CheckRoundTrip(const std::vector<int32_t>& samples, uint32_t rate, bool coordinate)
{
    std::vector<uint8_t> bytes{12, 34, 56};
    RecordingFlacCodec::Encoding encoding;
    bool audible = false;
    DOCTEST_REQUIRE(RecordingFlacCodec::Encode(samples.data(), static_cast<uint32_t>(samples.size()), rate, coordinate, bytes, encoding, audible));
    DOCTEST_CHECK(audible == !coordinate);
    DOCTEST_CHECK(encoding.m_encoding == 2);
    DOCTEST_CHECK(encoding.m_width == 0);
    DOCTEST_REQUIRE(bytes.size() > 11);
    DOCTEST_CHECK(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 3) == std::vector<uint8_t>{12, 34, 56});
    const auto length = static_cast<uint32_t>(bytes[3]) | static_cast<uint32_t>(bytes[4]) << 8 | static_cast<uint32_t>(bytes[5]) << 16 | static_cast<uint32_t>(bytes[6]) << 24;
    DOCTEST_CHECK(length == bytes.size() - 7);
    DOCTEST_CHECK(std::memcmp(bytes.data() + 7, "fLaC", 4) == 0);
    FlacDecoded decoded{bytes};
    std::unique_ptr<FLAC__StreamDecoder, decltype(&FLAC__stream_decoder_delete)> decoder(FLAC__stream_decoder_new(), &FLAC__stream_decoder_delete);
    DOCTEST_REQUIRE(decoder);
    DOCTEST_REQUIRE(FLAC__stream_decoder_set_md5_checking(decoder.get(), true));
    DOCTEST_REQUIRE(FLAC__stream_decoder_init_stream(decoder.get(), FlacDecoded::Read, nullptr, nullptr, nullptr, nullptr, FlacDecoded::Write, FlacDecoded::Metadata, FlacDecoded::Error, &decoded) == FLAC__STREAM_DECODER_INIT_STATUS_OK);
    DOCTEST_CHECK(FLAC__stream_decoder_process_until_end_of_stream(decoder.get()));
    DOCTEST_CHECK(FLAC__stream_decoder_finish(decoder.get()));
    DOCTEST_CHECK_FALSE(decoded.m_error);
    DOCTEST_CHECK(decoded.m_samples == samples);
    DOCTEST_CHECK(decoded.m_channels == 1);
    DOCTEST_CHECK(decoded.m_depth == 24);
    DOCTEST_CHECK(decoded.m_rate == rate);
    DOCTEST_CHECK(decoded.m_frames == samples.size());
    DOCTEST_CHECK(decoded.m_maxBlock <= 1024);
    DOCTEST_CHECK(decoded.m_cursor == bytes.size());
}
}

DOCTEST_TEST_CASE("FLAC codec round trips endpoints and changing multiple scratch blocks")
{
    std::vector<int32_t> samples(3137);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        samples[i] = static_cast<int32_t>((i * 7417) % 16777216) - 8388608;
    }

    samples[0] = -8388608;
    samples[1] = 8388607;
    CheckRoundTrip(samples, 48000, false);
    for (auto& value : samples)
    {
        value = std::max(0, value);
    }

    CheckRoundTrip(samples, 44100, true);
    samples.resize(1024);
    CheckRoundTrip(samples, 96000, true);
}

DOCTEST_TEST_CASE("FLAC codec constants and short raw streams preserve prefix")
{
    for (const int32_t value : {0, 8388607, -8388608, -12345})
    {
        std::vector<int32_t> samples(2051, value);
        std::vector<uint8_t> bytes{19};
        RecordingFlacCodec::Encoding encoding;
        bool audible = true;
        DOCTEST_REQUIRE(RecordingFlacCodec::Encode(samples.data(), 2051, 48000, false, bytes, encoding, audible));
        DOCTEST_CHECK(encoding.m_encoding == 1);
        DOCTEST_CHECK(encoding.m_width == 0);
        DOCTEST_CHECK(bytes.size() == 4);
        DOCTEST_CHECK(bytes[0] == 19);
        DOCTEST_CHECK(audible == (value != 0));
        const uint32_t packed = bytes[1] | static_cast<uint32_t>(bytes[2]) << 8 | static_cast<uint32_t>(bytes[3]) << 16;
        DOCTEST_CHECK(packed == (static_cast<uint32_t>(value) & 0xffffff));
    }

    const int32_t samples[]{0, 8388607};
    std::vector<uint8_t> bytes{19};
    RecordingFlacCodec::Encoding encoding;
    bool audible = true;
    DOCTEST_REQUIRE(RecordingFlacCodec::Encode(samples, 2, 48000, true, bytes, encoding, audible));
    DOCTEST_CHECK(encoding.m_encoding == 0);
    DOCTEST_CHECK(encoding.m_width == 24);
    DOCTEST_CHECK(bytes == std::vector<uint8_t>{19, 0, 0, 0, 255, 255, 127});
    DOCTEST_CHECK_FALSE(audible);
    bytes.resize(1);
    DOCTEST_REQUIRE(RecordingFlacCodec::Encode(samples + 1, 1, 48000, true, bytes, encoding, audible));
    DOCTEST_CHECK(encoding.m_encoding == 1);
    DOCTEST_CHECK_FALSE(audible);
}

DOCTEST_TEST_CASE("FLAC codec validates every sample and restores output after late failures")
{
    const std::vector<uint8_t> prefix{1, 2, 3};
    std::vector<uint8_t> bytes = prefix;
    RecordingFlacCodec::Encoding encoding;
    bool audible = true;
    std::vector<int32_t> samples(4000, 0);
    auto invalid = [&](const int32_t* data, uint32_t frames, uint32_t rate, bool coordinate)
    {
        DOCTEST_CHECK_FALSE(RecordingFlacCodec::Encode(data, frames, rate, coordinate, bytes, encoding, audible));
        DOCTEST_CHECK(bytes == prefix);
        DOCTEST_CHECK_FALSE(audible);
    };
    invalid(nullptr, 1, 48000, false);
    invalid(samples.data(), 0, 48000, false);
    invalid(samples.data(), 4000, 0, false);
    invalid(samples.data(), 4000, 0xffffffff, false);
    samples.back() = -1;
    invalid(samples.data(), 4000, 48000, true);
    samples.back() = 8388608;
    invalid(samples.data(), 4000, 48000, false);
    samples.back() = -8388609;
    invalid(samples.data(), 4000, 48000, false);
}

DOCTEST_TEST_CASE("FLAC codec enforces total output bound")
{
    std::vector<uint8_t> bytes(64 * 1024 * 1024, 23);
    const int32_t sample = 1;
    RecordingFlacCodec::Encoding encoding;
    bool audible = true;
    DOCTEST_CHECK_FALSE(RecordingFlacCodec::Encode(&sample, 1, 48000, false, bytes, encoding, audible));
    DOCTEST_CHECK(bytes.size() == 64 * 1024 * 1024);
    DOCTEST_CHECK(bytes.front() == 23);
    DOCTEST_CHECK(bytes.back() == 23);
    DOCTEST_CHECK_FALSE(audible);
}

DOCTEST_TEST_CASE("FLAC codec handles incompressible streams and callback output exhaustion")
{
    std::vector<int32_t> samples(4097);
    uint32_t random = 123456789;
    for (auto& value : samples)
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        value = static_cast<int32_t>(random & 0xffffff) - 8388608;
    }

    CheckRoundTrip(samples, 48000, false);
    std::vector<uint8_t> bytes(64 * 1024 * 1024 - 100, 47);
    const size_t size = bytes.size();
    RecordingFlacCodec::Encoding encoding{17, 19};
    bool audible = true;
    DOCTEST_CHECK_FALSE(RecordingFlacCodec::Encode(samples.data(), static_cast<uint32_t>(samples.size()), 48000, false, bytes, encoding, audible));
    DOCTEST_CHECK(bytes.size() == size);
    DOCTEST_CHECK(std::all_of(bytes.begin(), bytes.end(), [](uint8_t value)
    {
        return value == 47;
    }));
    DOCTEST_CHECK(encoding.m_encoding == 17);
    DOCTEST_CHECK(encoding.m_width == 19);
    DOCTEST_CHECK_FALSE(audible);
    bytes.clear();
    DOCTEST_REQUIRE(RecordingFlacCodec::Encode(samples.data(), 1024, 48000, false, bytes, encoding, audible));
    DOCTEST_CHECK(encoding.m_encoding == 0);
    DOCTEST_CHECK(encoding.m_width == 24);
    DOCTEST_CHECK(bytes.size() == 3072);
}
