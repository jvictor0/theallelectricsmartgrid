#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace WavReaderDetail
{
    static inline uint16_t ReadU16LE(const uint8_t* p)
    {
        return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    }

    static inline uint32_t ReadU32LE(const uint8_t* p)
    {
        return static_cast<uint32_t>(p[0])
            | (static_cast<uint32_t>(p[1]) << 8)
            | (static_cast<uint32_t>(p[2]) << 16)
            | (static_cast<uint32_t>(p[3]) << 24);
    }

    static inline uint64_t ReadU64LE(const uint8_t* p)
    {
        return static_cast<uint64_t>(ReadU32LE(p))
            | (static_cast<uint64_t>(ReadU32LE(p + 4)) << 32);
    }

    static inline int32_t ReadS24LE(const uint8_t* p)
    {
        int32_t v = static_cast<int32_t>(p[0]) | (static_cast<int32_t>(p[1]) << 8) | (static_cast<int32_t>(p[2]) << 16);
        if (0 != (v & 0x800000))
        {
            v |= static_cast<int32_t>(~0xFFFFFF);
        }

        return v;
    }

    static inline float ReadF32LE(const uint8_t* p)
    {
        uint32_t u = ReadU32LE(p);
        float f;
        std::memcpy(&f, &u, sizeof(float));
        return f;
    }

    static inline bool ReadEntireFile(const char* fileName, std::vector<uint8_t>& out)
    {
        std::ifstream file(fileName, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return false;
        }

        std::streamoff end = file.tellg();
        if (end <= 0)
        {
            return false;
        }

        out.resize(static_cast<size_t>(end));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(out.data()), end))
        {
            return false;
        }

        return true;
    }
}

struct WavReader
{
    enum class Format
    {
        Unknown,
        Pcm,
        Float
    };

    std::vector<uint8_t> m_fileBytes;
    Format m_format{Format::Unknown};
    uint16_t m_numChannels{0};
    uint32_t m_sampleRate{0};
    uint16_t m_blockAlign{0};
    uint16_t m_bitsPerSample{0};
    size_t m_dataOffset{0};
    size_t m_dataSize{0};
    size_t m_numFrames{0};
    bool m_isRf64{false};
    bool m_ready{false};

    void Reset()
    {
        m_fileBytes.clear();
        m_format = Format::Unknown;
        m_numChannels = 0;
        m_sampleRate = 0;
        m_blockAlign = 0;
        m_bitsPerSample = 0;
        m_dataOffset = 0;
        m_dataSize = 0;
        m_numFrames = 0;
        m_isRf64 = false;
        m_ready = false;
    }

    bool LoadFromFile(const char* fileName)
    {
        Reset();

        using namespace WavReaderDetail;
        if (!ReadEntireFile(fileName, m_fileBytes))
        {
            return false;
        }

        size_t fmtOffset = 0;
        size_t fmtSize = 0;
        if (!FindFmtAndData(fmtOffset, fmtSize))
        {
            Reset();
            return false;
        }

        if (!ParseFormat(fmtOffset, fmtSize))
        {
            Reset();
            return false;
        }

        if (!IsSupported())
        {
            Reset();
            return false;
        }

        m_numFrames = m_dataSize / m_blockAlign;
        m_ready = true;
        return true;
    }

    bool FindFmtAndData(size_t& fmtOffset, size_t& fmtSize)
    {
        if (m_fileBytes.size() < 12)
        {
            return false;
        }

        bool isRiff = 0 == std::memcmp(m_fileBytes.data(), "RIFF", 4);
        m_isRf64 = 0 == std::memcmp(m_fileBytes.data(), "RF64", 4);
        if ((!isRiff && !m_isRf64) || 0 != std::memcmp(m_fileBytes.data() + 8, "WAVE", 4))
        {
            return false;
        }

        fmtOffset = 0;
        fmtSize = 0;
        m_dataOffset = 0;
        m_dataSize = 0;
        uint64_t rf64DataSize = 0;

        size_t pos = 12;
        while (pos + 8 <= m_fileBytes.size())
        {
            const uint8_t* id = m_fileBytes.data() + pos;
            uint32_t chunkSize = WavReaderDetail::ReadU32LE(m_fileBytes.data() + pos + 4);
            uint64_t effectiveChunkSize = chunkSize;
            size_t payload = pos + 8;

            if (0 == std::memcmp(id, "ds64", 4) && 16 <= chunkSize)
            {
                rf64DataSize = WavReaderDetail::ReadU64LE(m_fileBytes.data() + payload + 8);
            }

            if (m_isRf64 && 0 == std::memcmp(id, "data", 4) && chunkSize == 0xFFFFFFFFu)
            {
                effectiveChunkSize = rf64DataSize;
            }

            if (static_cast<uint64_t>(m_fileBytes.size() - payload) < effectiveChunkSize)
            {
                return false;
            }

            if (0 == std::memcmp(id, "fmt ", 4))
            {
                fmtOffset = payload;
                fmtSize = static_cast<size_t>(effectiveChunkSize);
            }
            else if (0 == std::memcmp(id, "data", 4))
            {
                m_dataOffset = payload;
                m_dataSize = static_cast<size_t>(effectiveChunkSize);
            }

            pos = payload + static_cast<size_t>(effectiveChunkSize) + (static_cast<size_t>(effectiveChunkSize) & 1u);
        }

        return 0 < fmtSize && 0 < m_dataSize && fmtOffset != 0 && m_dataOffset != 0;
    }

    bool ParseFormat(size_t fmtOffset, size_t fmtSize)
    {
        if (fmtSize < 16)
        {
            return false;
        }

        const uint8_t* fmt = m_fileBytes.data() + fmtOffset;
        uint16_t audioFormat = WavReaderDetail::ReadU16LE(fmt + 0);
        m_numChannels = WavReaderDetail::ReadU16LE(fmt + 2);
        m_sampleRate = WavReaderDetail::ReadU32LE(fmt + 4);
        m_blockAlign = WavReaderDetail::ReadU16LE(fmt + 12);
        m_bitsPerSample = WavReaderDetail::ReadU16LE(fmt + 14);

        if (audioFormat == 0xFFFE && 40 <= fmtSize)
        {
            audioFormat = static_cast<uint16_t>(WavReaderDetail::ReadU32LE(fmt + 24));
        }

        if (audioFormat == 1)
        {
            m_format = Format::Pcm;
        }
        else if (audioFormat == 3)
        {
            m_format = Format::Float;
        }
        else
        {
            m_format = Format::Unknown;
        }

        return 0 < m_numChannels && 0 < m_blockAlign && m_dataSize % m_blockAlign == 0;
    }

    bool IsSupported() const
    {
        if (m_format == Format::Pcm && m_bitsPerSample == 16)
        {
            return m_blockAlign == m_numChannels * 2;
        }

        if (m_format == Format::Pcm && m_bitsPerSample == 24)
        {
            return m_blockAlign == m_numChannels * 3;
        }

        if (m_format == Format::Float && m_bitsPerSample == 32)
        {
            return m_blockAlign == m_numChannels * 4;
        }

        return false;
    }

    float GetSample(size_t frameIndex, size_t channelIndex) const
    {
        if (!m_ready || m_numFrames <= frameIndex || m_numChannels <= channelIndex)
        {
            return 0.0f;
        }

        const uint8_t* frame = m_fileBytes.data() + m_dataOffset + frameIndex * m_blockAlign;
        const uint8_t* sample = frame + channelIndex * BytesPerSample();
        if (m_format == Format::Pcm && m_bitsPerSample == 16)
        {
            int16_t v = static_cast<int16_t>(WavReaderDetail::ReadU16LE(sample));
            return static_cast<float>(v) * (1.0f / 32768.0f);
        }

        if (m_format == Format::Pcm && m_bitsPerSample == 24)
        {
            return static_cast<float>(WavReaderDetail::ReadS24LE(sample)) * (1.0f / 8388608.0f);
        }

        if (m_format == Format::Float && m_bitsPerSample == 32)
        {
            return WavReaderDetail::ReadF32LE(sample);
        }

        return 0.0f;
    }

    float GetLeftRightSum(size_t frameIndex) const
    {
        float sum = GetSample(frameIndex, 0);
        if (1 < m_numChannels)
        {
            sum += GetSample(frameIndex, 1);
        }

        return sum;
    }

    void WriteLeftRightSum(std::vector<float>& out) const
    {
        out.resize(m_numFrames);
        for (size_t i = 0; i < m_numFrames; ++i)
        {
            out[i] = GetLeftRightSum(i);
        }
    }

    size_t BytesPerSample() const
    {
        return static_cast<size_t>(m_bitsPerSample / 8);
    }
};
