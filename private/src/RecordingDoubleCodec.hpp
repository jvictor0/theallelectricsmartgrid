#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

struct RecordingDoubleCodec
{
    static_assert(sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559);

    static uint64_t Bits(double value)
    {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    static void AppendBits(std::vector<uint8_t>& output, uint64_t bits)
    {
        for (size_t i = 0; i < 8; ++i)
        {
            output.push_back(static_cast<uint8_t>(bits >> (8 * i)));
        }
    }

    static void AppendVarint(std::vector<uint8_t>& output, uint64_t value)
    {
        while (value >= 128)
        {
            output.push_back(static_cast<uint8_t>(value) | 128);
            value >>= 7;
        }

        output.push_back(static_cast<uint8_t>(value));
    }

    // Predict the integer bit pattern, with modulo-2^64 arithmetic. No floating
    // point subtraction or reconstruction can lose low bits or overflow.
    // Zero second differences form runs, including steady ramps and constants.
    //
    static bool Encode(const double* values, size_t count, std::vector<uint8_t>& output, uint8_t& encoding)
    {
        if (values == nullptr || count == 0)
        {
            return false;
        }

        for (size_t i = 0; i < count; ++i)
        {
            if (!std::isfinite(values[i]))
            {
                return false;
            }
        }

        const size_t start = output.size();
        uint64_t previous = Bits(values[0]);
        uint64_t previousDelta = 0;
        uint64_t run = 0;
        AppendBits(output, previous);
        for (size_t i = 1; i < count; ++i)
        {
            const uint64_t bits = Bits(values[i]);
            const uint64_t delta = bits - previous;
            const uint64_t residual = delta - previousDelta;
            previous = bits;
            previousDelta = delta;
            if (residual == 0)
            {
                ++run;
                continue;
            }

            if (run != 0)
            {
                output.push_back(0);
                AppendVarint(output, run);
                run = 0;
            }

            AppendVarint(output, (residual << 1) ^ (uint64_t{0} - (residual >> 63)));
        }

        if (run != 0)
        {
            output.push_back(0);
            AppendVarint(output, run);
        }

        encoding = 1;
        if (output.size() - start >= count * sizeof(double))
        {
            encoding = 0;
            output.resize(start);
            for (size_t i = 0; i < count; ++i)
            {
                AppendBits(output, Bits(values[i]));
            }
        }

        return true;
    }
};
