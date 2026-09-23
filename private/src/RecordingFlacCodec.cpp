#include "RecordingFlacCodec.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdbool.h>

#if defined(SMARTGRID_RECORDING_SYSTEM_FLAC)
#include <FLAC/stream_encoder.h>
#else
namespace juce
{
namespace FlacNamespace
{
#include <juce_audio_formats/codecs/flac/stream_encoder.h>
}
}
using namespace juce::FlacNamespace;
#endif

namespace
{
constexpr size_t x_maxOutputBytes = 64 * 1024 * 1024;
constexpr uint32_t x_blockFrames = 1024;

struct FlacOutput
{
    std::vector<uint8_t>& m_output;
    size_t m_start;
    size_t m_cursor = 0;
    bool m_failed = false;

    static FLAC__StreamEncoderWriteStatus Write(const FLAC__StreamEncoder*, const FLAC__byte bytes[], size_t count, unsigned, unsigned, void* client) noexcept
    {
        auto& self = *static_cast<FlacOutput*>(client);
        if (self.m_failed || count > x_maxOutputBytes - self.m_start - self.m_cursor)
        {
            self.m_failed = true;
            return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }

        try
        {
            const size_t position = self.m_start + self.m_cursor;
            self.m_output.resize(std::max(self.m_output.size(), position + count));
            std::memcpy(self.m_output.data() + position, bytes, count);
            self.m_cursor += count;
            return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
        }
        catch (...)
        {
            self.m_failed = true;
            return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }
    }

    static FLAC__StreamEncoderSeekStatus Seek(const FLAC__StreamEncoder*, FLAC__uint64 offset, void* client) noexcept
    {
        auto& self = *static_cast<FlacOutput*>(client);
        if (offset > self.m_output.size() - self.m_start)
        {
            self.m_failed = true;
            return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
        }

        self.m_cursor = static_cast<size_t>(offset);
        return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
    }

    static FLAC__StreamEncoderTellStatus Tell(const FLAC__StreamEncoder*, FLAC__uint64* offset, void* client) noexcept
    {
        *offset = static_cast<FlacOutput*>(client)->m_cursor;
        return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
    }
};

void AppendPcm24(std::vector<uint8_t>& output, int32_t value)
{
    const auto bits = static_cast<uint32_t>(value);
    output.push_back(static_cast<uint8_t>(bits));
    output.push_back(static_cast<uint8_t>(bits >> 8));
    output.push_back(static_cast<uint8_t>(bits >> 16));
}
}

bool RecordingFlacCodec::Encode(const int32_t* samples, uint32_t frames, uint32_t sampleRate,
                               bool coordinate, std::vector<uint8_t>& output,
                               Encoding& encoding, bool& audible)
{
    audible = false;
    const size_t start = output.size();
    if (!samples || !frames || !sampleRate || !FLAC__format_sample_rate_is_valid(sampleRate)
        || start > x_maxOutputBytes - 4)
    {
        return false;
    }

    try
    {
        output.resize(start + 4);
        FlacOutput destination{output, start + 4};
        std::unique_ptr<FLAC__StreamEncoder, decltype(&FLAC__stream_encoder_delete)> encoder(FLAC__stream_encoder_new(), &FLAC__stream_encoder_delete);
        auto encode = [&]()
        {
            if (!encoder
                || !FLAC__stream_encoder_set_channels(encoder.get(), 1)
                || !FLAC__stream_encoder_set_bits_per_sample(encoder.get(), 24)
                || !FLAC__stream_encoder_set_sample_rate(encoder.get(), sampleRate)
                || !FLAC__stream_encoder_set_compression_level(encoder.get(), 5)
                || !FLAC__stream_encoder_set_blocksize(encoder.get(), x_blockFrames)
                || !FLAC__stream_encoder_set_total_samples_estimate(encoder.get(), frames)
                || FLAC__stream_encoder_init_stream(encoder.get(), FlacOutput::Write, FlacOutput::Seek, FlacOutput::Tell, nullptr, &destination) != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
            {
                return false;
            }

            std::array<FLAC__int32, x_blockFrames> scratch{};
            int32_t first = 0;
            bool constant = true;
            bool nonzero = false;
            for (uint32_t offset = 0; offset < frames;)
            {
                const auto count = std::min(x_blockFrames, frames - offset);
                for (uint32_t i = 0; i < count; ++i)
                {
                    const int32_t value = samples[offset + i];
                    if (value < (coordinate ? 0 : -8388608) || value > 8388607)
                    {
                        return false;
                    }

                    if (offset == 0 && i == 0)
                    {
                        first = value;
                    }

                    scratch[i] = value;
                    constant &= value == first;
                    nonzero |= value != 0;
                }

                if (!FLAC__stream_encoder_process_interleaved(encoder.get(), scratch.data(), count))
                {
                    return false;
                }

                offset += count;
            }

            if (!FLAC__stream_encoder_finish(encoder.get()) || destination.m_failed)
            {
                return false;
            }

            Encoding selected{2, 0};
            if (constant)
            {
                output.resize(start);
                AppendPcm24(output, first);
                selected = {1, 0};
            }
            else if (frames <= x_blockFrames && static_cast<size_t>(frames) * 3 < output.size() - start)
            {
                output.resize(start);
                for (uint32_t i = 0; i < frames; ++i)
                {
                    AppendPcm24(output, scratch[i]);
                }

                selected = {0, 24};
            }
            else
            {
                const auto bytes = static_cast<uint32_t>(output.size() - start - 4);
                for (unsigned i = 0; i < 4; ++i)
                {
                    output[start + i] = static_cast<uint8_t>(bytes >> (i * 8));
                }
            }

            encoding = selected;
            audible = !coordinate && nonzero;
            return true;
        };
        if (encode())
        {
            return true;
        }
    }
    catch (...)
    {
    }

    output.resize(start);
    audible = false;
    return false;
}
