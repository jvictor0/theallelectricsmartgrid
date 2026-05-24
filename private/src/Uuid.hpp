#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>

struct Uuid
{
    static constexpr size_t x_stringCapacity = 37;

    static void FormatRandomV4(char* dest, size_t capacity)
    {
        if (dest == nullptr || capacity < x_stringCapacity)
        {
            if (dest != nullptr && capacity > 0)
            {
                dest[0] = '\0';
            }

            return;
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> byteDist(0, 255);

        uint8_t bytes[16];
        for (size_t i = 0; i < 16; ++i)
        {
            bytes[i] = static_cast<uint8_t>(byteDist(gen));
        }

        bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0Fu) | 0x40u);
        bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3Fu) | 0x80u);

        std::snprintf(
            dest,
            capacity,
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0],
            bytes[1],
            bytes[2],
            bytes[3],
            bytes[4],
            bytes[5],
            bytes[6],
            bytes[7],
            bytes[8],
            bytes[9],
            bytes[10],
            bytes[11],
            bytes[12],
            bytes[13],
            bytes[14],
            bytes[15]);
    }
};
