#pragma once

#include "Json.hpp"
#include "ParamEvent.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
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

    static bool EncodeHeader(const Session& session, std::vector<uint8_t>& output,
        JSON initialPatch = {})
    {
        if (!Validate(session) || session.m_recordedAtUtc.empty())
        {
            return false;
        }

        JsonArena arena(x_maxHeaderBytes);
        JSON header = arena.Object();
        header.SetNew("format_version", arena.Integer(3));
        header.SetNew("initial_patch", initialPatch.IsNull() ? arena.Object() : initialPatch);
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

    static const char* EventName(const ParamEvent& event)
    {
        return event.m_type == ParamEvent::Type::GestureSet || event.m_type == ParamEvent::Type::BlendSet
            || event.IsPatch()
            ? "" : event.m_name;
    }

    static bool ValidateEvent(const ParamEvent& event)
    {
        using Type = ParamEvent::Type;
        const char* name = EventName(event);
        if (name == nullptr || std::strlen(name) > UINT16_MAX)
        {
            return false;
        }

        if (event.IsPatch())
        {
            return event.m_valueLen == 0 && event.m_patchText != nullptr
                && event.m_patchBytes <= x_maxBlockBytes;
        }

        if (event.m_type == Type::StateChange)
        {
            return name[0] != '\0' && event.m_scene >= 0 && event.m_scene < 8
                && (event.m_valueLen == 1 || event.m_valueLen == 2
                    || event.m_valueLen == 4 || event.m_valueLen == 8);
        }

        if (event.m_type == Type::EncoderSet || event.m_type == Type::EncoderActivate)
        {
            if (name[0] == '\0' || event.m_scene < 0 || event.m_scene >= 8
                || event.m_track < 0 || event.m_track >= 16)
            {
                return false;
            }

            const size_t length = event.EncoderPathLength();
            for (size_t i = 0; i < length; ++i)
            {
                const int hop = event.m_encoderPath[i];
                if (!((hop >= 0 && hop < 15) || (hop >= 128 && hop < 144)))
                {
                    return false;
                }
            }

            if (event.m_type == Type::EncoderActivate)
            {
                return length != 0 && event.m_encoderPath[length - 1] >= 128
                    && event.m_valueLen == 1 && (event.m_value[0] == 0 || event.m_value[0] == 1);
            }
        }
        else if (event.m_type == Type::GestureSet)
        {
            if (event.m_gesture < 0 || event.m_gesture >= 16)
            {
                return false;
            }
        }
        else if (event.m_type != Type::BlendSet)
        {
            return false;
        }

        float value = 0.0f;
        std::memcpy(&value, event.m_value, sizeof(value));
        return event.m_valueLen == sizeof(value) && std::isfinite(value);
    }

    static bool AppendParamEvents(std::vector<uint8_t>& output, std::vector<ParamEvent> events,
        uint64_t startFrame, uint32_t frames)
    {
        for (size_t i = 0; i < events.size(); ++i)
        {
            auto& event = events[i];
            event.m_order = static_cast<uint32_t>(i);
            if (!ValidateEvent(event) || event.m_sample < startFrame || event.m_sample - startFrame >= frames)
            {
                return false;
            }
        }

        std::stable_sort(events.begin(), events.end(), [](const ParamEvent& a, const ParamEvent& b)
        {
            if (a.m_type != b.m_type)
            {
                return a.m_type < b.m_type;
            }

            const int nameOrder = std::strcmp(EventName(a), EventName(b));
            if (nameOrder != 0)
            {
                return nameOrder < 0;
            }

            return a.m_sample < b.m_sample;
        });
        const size_t countOffset = output.size();
        AppendLE(output, 0, 4);
        uint32_t groups = 0;
        for (size_t begin = 0; begin < events.size();)
        {
            const auto& first = events[begin];
            size_t end = begin + 1;
            while (end < events.size() && events[end].m_type == first.m_type
                && std::strcmp(EventName(events[end]), EventName(first)) == 0)
            {
                if (events[end].m_valueLen != first.m_valueLen)
                {
                    return false;
                }

                ++end;
            }

            const char* name = EventName(first);
            const size_t nameLen = std::strlen(name);
            size_t groupBytes = 8 + nameLen;
            for (size_t i = begin; i < end; ++i)
            {
                const auto& event = events[i];
                groupBytes += 8 + event.m_valueLen;
                if (event.IsPatch())
                {
                    groupBytes += 4 + event.m_patchBytes
                        + (event.m_type == ParamEvent::Type::PatchLoad ? 1 : 0);
                }

                if (event.m_type == ParamEvent::Type::StateChange || event.m_type == ParamEvent::Type::GestureSet)
                {
                    ++groupBytes;
                }
                else if (event.m_type == ParamEvent::Type::EncoderSet || event.m_type == ParamEvent::Type::EncoderActivate)
                {
                    groupBytes += 3 + event.EncoderPathLength();
                }
            }

            if (output.size() + groupBytes + 4 > x_maxBlockBytes)
            {
                return false;
            }

            AppendLE(output, static_cast<uint8_t>(first.m_type), 1);
            AppendLE(output, first.m_valueLen, 1);
            AppendLE(output, nameLen, 2);
            AppendLE(output, end - begin, 4);
            output.insert(output.end(), name, name + nameLen);
            for (size_t i = begin; i < end; ++i)
            {
                const auto& event = events[i];
                AppendLE(output, event.m_sample - startFrame, 4);
                AppendLE(output, event.m_order, 4);
                switch (event.m_type)
                {
                    case ParamEvent::Type::StateChange:
                        AppendLE(output, event.m_scene, 1);
                        break;
                    case ParamEvent::Type::GestureSet:
                        AppendLE(output, event.m_gesture, 1);
                        break;
                    case ParamEvent::Type::EncoderSet:
                    case ParamEvent::Type::EncoderActivate:
                        AppendLE(output, event.m_scene, 1);
                        AppendLE(output, event.m_track, 1);
                        AppendLE(output, event.EncoderPathLength(), 1);
                        for (size_t hop = 0; hop < event.EncoderPathLength(); ++hop)
                        {
                            AppendLE(output, event.m_encoderPath[hop], 1);
                        }

                        break;
                    case ParamEvent::Type::PatchLoad:
                    case ParamEvent::Type::PatchSnapshot:
                        if (event.m_type == ParamEvent::Type::PatchLoad)
                        {
                            AppendLE(output, event.m_restoreFaders, 1);
                        }

                        AppendLE(output, event.m_patchBytes, 4);
                        output.insert(output.end(), event.m_patchText, event.m_patchText + event.m_patchBytes);
                        break;
                    default:
                        break;
                }

                output.insert(output.end(), event.m_value, event.m_value + event.m_valueLen);
            }

            ++groups;
            begin = end;
        }

        for (size_t i = 0; i < 4; ++i)
        {
            output[countOffset + i] = static_cast<uint8_t>(groups >> (8 * i));
        }

        return true;
    }

    static bool EncodeBlock(const Session& session, const int32_t* samples,
        uint32_t frames, uint64_t startFrame, std::vector<uint8_t>& output,
        const std::vector<ParamEvent>& events = {})
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
        output.insert(output.end(), {'B', 'L', 'K', '3'});
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

        if (!AppendParamEvents(output, events, startFrame, frames)
            || output.size() + 4 > x_maxBlockBytes)
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
