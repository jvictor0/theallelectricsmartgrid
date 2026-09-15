#pragma once

#include "Json.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct RecordingFormat
{
    static constexpr size_t x_maxHeaderBytes = 1024 * 1024;
    static constexpr size_t x_maxBlockBytes = 64 * 1024 * 1024;
    static constexpr size_t x_maxTracks = 128;

    enum class TrackType : uint8_t
    {
        Mono,
        PannedMono,
        Stereo,
        Quad
    };

    struct Track
    {
        uint32_t m_id;
        std::string m_name;
        TrackType m_type;
        std::string m_role;
        std::string m_tap;
    };

    static size_t StreamCount(TrackType type)
    {
        switch (type)
        {
            case TrackType::Mono: return 1;
            case TrackType::PannedMono: return 3;
            case TrackType::Stereo: return 2;
            case TrackType::Quad: return 4;
        }

        return 0;
    }

    static const char* TypeName(TrackType type)
    {
        switch (type)
        {
            case TrackType::Mono: return "mono";
            case TrackType::PannedMono: return "panned_mono";
            case TrackType::Stereo: return "stereo";
            case TrackType::Quad: return "quad";
        }

        return "";
    }

    struct Session
    {
        uint32_t m_sampleRate = 48000;
        uint32_t m_blockFrames = 48000;
        std::string m_gitCommitSha;
        std::string m_recordedAtUtc;
        std::vector<Track> m_tracks;

        size_t StreamCount() const
        {
            size_t count = 0;
            for (const auto& track : m_tracks)
            {
                count += RecordingFormat::StreamCount(track.m_type);
            }

            return count;
        }
    };

    struct Encoding
    {
        uint8_t m_encoding;
        uint8_t m_width;
    };

    static bool Validate(const Session& session)
    {
        if (session.m_sampleRate == 0 || session.m_blockFrames == 0
            || session.m_tracks.empty() || session.m_tracks.size() > x_maxTracks)
        {
            return false;
        }

        const auto& sha = session.m_gitCommitSha;
        if ((sha.size() != 40 && sha.size() != 64)
            || !std::all_of(sha.begin(), sha.end(), [](char ch)
            {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
            }))
        {
            return false;
        }

        uint32_t previous = 0;
        bool first = true;
        bool hasStereoMaster = false;
        bool hasQuadMaster = false;
        size_t textBytes = session.m_recordedAtUtc.size() + sha.size();
        for (const auto& track : session.m_tracks)
        {
            if ((!first && track.m_id <= previous) || StreamCount(track.m_type) == 0
                || track.m_name.empty() || track.m_role.empty() || track.m_tap.empty())
            {
                return false;
            }

            first = false;
            previous = track.m_id;
            if (track.m_role == "master_stereo" || track.m_role == "master_quad")
            {
                const bool stereo = track.m_role == "master_stereo";
                bool& seen = stereo ? hasStereoMaster : hasQuadMaster;
                if (seen || track.m_type != (stereo ? TrackType::Stereo : TrackType::Quad)
                    || track.m_tap != "post_mastering_pre_master_volume")
                {
                    return false;
                }

                seen = true;
            }

            textBytes += track.m_name.size() + track.m_role.size() + track.m_tap.size();
            if (textBytes > x_maxHeaderBytes)
            {
                return false;
            }
        }

        const size_t streams = session.StreamCount();
        return streams <= x_maxBlockBytes / sizeof(int32_t) / session.m_blockFrames;
    }

    static int32_t Quantize(float sample, bool position = false)
    {
        assert(std::isfinite(sample));
        const double clamped = std::clamp(static_cast<double>(sample), position ? 0.0 : -1.0, 1.0);
        return static_cast<int32_t>(std::lround(clamped * 8388607.0));
    }

    static void AppendLE(std::vector<uint8_t>& output, uint64_t value, size_t bytes)
    {
        for (size_t i = 0; i < bytes; ++i)
        {
            output.push_back(static_cast<uint8_t>(value >> (8 * i)));
        }
    }

    static uint32_t Crc32(const uint8_t* bytes, size_t size)
    {
        static const std::array<uint32_t, 256> x_table = []()
        {
            std::array<uint32_t, 256> table{};
            for (uint32_t i = 0; i < table.size(); ++i)
            {
                uint32_t value = i;
                for (size_t bit = 0; bit < 8; ++bit)
                {
                    value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
                }

                table[i] = value;
            }

            return table;
        }();

        uint32_t crc = 0xffffffffu;
        for (size_t i = 0; i < size; ++i)
        {
            crc = x_table[(crc ^ bytes[i]) & 255] ^ (crc >> 8);
        }

        return crc ^ 0xffffffffu;
    }

    static Encoding ChooseEncoding(const int32_t* values, size_t count)
    {
        assert(count > 0);
        int64_t smallest = 0;
        int64_t largest = 0;
        for (size_t i = 1; i < count; ++i)
        {
            const int64_t delta = static_cast<int64_t>(values[i]) - values[i - 1];
            smallest = std::min(smallest, delta);
            largest = std::max(largest, delta);
        }

        uint8_t width = 0;
        if (smallest != 0 || largest != 0)
        {
            width = 1;
            while (smallest < -(int64_t{1} << (width - 1))
                || largest >= (int64_t{1} << (width - 1)))
            {
                ++width;
            }
        }

        if (3 + ((count - 1) * width + 7) / 8 < count * 3)
        {
            return {1, width};
        }

        return {0, 24};
    }

    static void AppendStream(std::vector<uint8_t>& output, const int32_t* values,
        size_t count, Encoding encoding)
    {
        if (encoding.m_encoding == 0)
        {
            for (size_t i = 0; i < count; ++i)
            {
                AppendLE(output, static_cast<uint32_t>(values[i]), 3);
            }

            return;
        }

        AppendLE(output, static_cast<uint32_t>(values[0]), 3);
        if (encoding.m_width == 0)
        {
            return;
        }

        uint64_t packed = 0;
        size_t bits = 0;
        const uint32_t mask = (uint32_t{1} << encoding.m_width) - 1;
        for (size_t i = 1; i < count; ++i)
        {
            const int64_t delta = static_cast<int64_t>(values[i]) - values[i - 1];
            packed |= static_cast<uint64_t>(static_cast<uint32_t>(delta) & mask) << bits;
            bits += encoding.m_width;
            while (bits >= 8)
            {
                output.push_back(static_cast<uint8_t>(packed));
                packed >>= 8;
                bits -= 8;
            }
        }

        if (bits != 0)
        {
            output.push_back(static_cast<uint8_t>(packed));
        }
    }

    static bool EncodeHeader(const Session& session, std::vector<uint8_t>& output)
    {
        if (!Validate(session) || session.m_recordedAtUtc.empty())
        {
            return false;
        }

        JsonArena arena(x_maxHeaderBytes);
        JSON header = arena.Object();
        header.SetNew("format_version", arena.Integer(1));
        header.SetNew("recorded_at_utc", arena.String(session.m_recordedAtUtc.c_str()));
        header.SetNew("git_commit_sha", arena.String(session.m_gitCommitSha.c_str()));
        header.SetNew("sample_rate", arena.Integer(session.m_sampleRate));
        header.SetNew("block_frames", arena.Integer(session.m_blockFrames));
        JSON tracks = arena.Array();
        for (const auto& track : session.m_tracks)
        {
            JSON entry = arena.Object();
            entry.SetNew("id", arena.Integer(track.m_id));
            entry.SetNew("name", arena.String(track.m_name.c_str()));
            entry.SetNew("type", arena.String(TypeName(track.m_type)));
            entry.SetNew("role", arena.String(track.m_role.c_str()));
            entry.SetNew("tap", arena.String(track.m_tap.c_str()));
            tracks.AppendNew(entry);
        }

        header.SetNew("tracks", tracks);
        if (arena.Failed())
        {
            return false;
        }

        char* text = header.Dumps(0);
        if (text == nullptr)
        {
            return false;
        }

        const size_t size = std::strlen(text);
        output.clear();
        if (size <= x_maxHeaderBytes)
        {
            const char magic[] = "SMRTGRID";
            output.insert(output.end(), magic, magic + 8);
            AppendLE(output, size, 4);
            output.insert(output.end(), text, text + size);
        }

        std::free(text);
        return !output.empty();
    }

    static bool EncodeBlock(const Session& session, const int32_t* samples,
        uint32_t frames, uint64_t startFrame, std::vector<uint8_t>& output)
    {
        if (!Validate(session) || frames == 0 || frames > session.m_blockFrames || samples == nullptr)
        {
            return false;
        }

        std::vector<Encoding> encodings;
        std::vector<size_t> included;
        size_t streamOffset = 0;
        for (size_t trackIndex = 0; trackIndex < session.m_tracks.size(); ++trackIndex)
        {
            const auto& track = session.m_tracks[trackIndex];
            const size_t streams = StreamCount(track.m_type);
            bool audible = false;
            for (size_t stream = 0; stream < streams; ++stream)
            {
                const bool position = track.m_type == TrackType::PannedMono && stream != 0;
                const int32_t* values = samples + (streamOffset + stream) * session.m_blockFrames;
                for (size_t frame = 0; frame < frames; ++frame)
                {
                    if (values[frame] < (position ? 0 : -8388608) || values[frame] > 8388607)
                    {
                        return false;
                    }

                    audible |= !position && values[frame] != 0;
                }

                encodings.push_back(ChooseEncoding(values, frames));
            }

            if (audible)
            {
                included.push_back(trackIndex);
            }

            streamOffset += streams;
        }

        output.clear();
        output.insert(output.end(), {'B', 'L', 'K', '1'});
        AppendLE(output, 0, 4);
        AppendLE(output, startFrame, 8);
        AppendLE(output, frames, 4);
        AppendLE(output, included.size(), 2);
        streamOffset = 0;
        size_t includedIndex = 0;
        for (size_t trackIndex = 0; trackIndex < session.m_tracks.size(); ++trackIndex)
        {
            const auto& track = session.m_tracks[trackIndex];
            const size_t streams = StreamCount(track.m_type);
            if (includedIndex < included.size() && included[includedIndex] == trackIndex)
            {
                AppendLE(output, track.m_id, 4);
                for (size_t stream = 0; stream < streams; ++stream)
                {
                    const auto encoding = encodings[streamOffset + stream];
                    output.push_back(encoding.m_encoding);
                    output.push_back(encoding.m_width);
                }

                ++includedIndex;
            }

            streamOffset += streams;
        }

        streamOffset = 0;
        includedIndex = 0;
        for (size_t trackIndex = 0; trackIndex < session.m_tracks.size(); ++trackIndex)
        {
            const size_t streams = StreamCount(session.m_tracks[trackIndex].m_type);
            if (includedIndex < included.size() && included[includedIndex] == trackIndex)
            {
                for (size_t stream = 0; stream < streams; ++stream)
                {
                    AppendStream(output, samples + (streamOffset + stream) * session.m_blockFrames,
                        frames, encodings[streamOffset + stream]);
                }

                ++includedIndex;
            }

            streamOffset += streams;
        }

        if (output.size() + 4 > x_maxBlockBytes)
        {
            return false;
        }

        const uint32_t recordBytes = static_cast<uint32_t>(output.size() + 4);
        for (size_t i = 0; i < 4; ++i)
        {
            output[4 + i] = static_cast<uint8_t>(recordBytes >> (8 * i));
        }

        AppendLE(output, Crc32(output.data(), output.size()), 4);
        return true;
    }

    static void EncodeEnd(uint64_t totalFrames, std::vector<uint8_t>& output)
    {
        output.clear();
        output.insert(output.end(), {'E', 'N', 'D', '1'});
        AppendLE(output, totalFrames, 8);
        AppendLE(output, Crc32(output.data(), output.size()), 4);
    }
};
