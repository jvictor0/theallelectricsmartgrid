#pragma once

#include <cstdint>
#include <vector>

struct RecordingFlacCodec
{
    struct Encoding
    {
        uint8_t m_encoding = 0;
        uint8_t m_width = 0;
    };

    static bool Encode(const int32_t* samples, uint32_t frames, uint32_t sampleRate,
                       bool coordinate, std::vector<uint8_t>& output,
                       Encoding& encoding, bool& audible);
};
