#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace CodegenInteger
{
template<typename I>
I Add(I a, I b)
{
    constexpr I x_min = std::numeric_limits<I>::min();
    constexpr I x_max = std::numeric_limits<I>::max();
    if constexpr (std::is_unsigned_v<I>)
    {
        if (a > x_max - b)
        {
            std::abort();
        }
    }
    else if ((b > 0 && a > x_max - b) || (b < 0 && a < x_min - b))
    {
        std::abort();
    }

    return a + b;
}

inline int64_t Subtract(int64_t a, int64_t b)
{
    constexpr auto x_min = std::numeric_limits<int64_t>::min();
    constexpr auto x_max = std::numeric_limits<int64_t>::max();
    if ((b > 0 && a < x_min + b) || (b < 0 && a > x_max + b))
    {
        std::abort();
    }

    return a - b;
}

inline int64_t FromIndex(size_t value)
{
    if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    {
        std::abort();
    }

    return static_cast<int64_t>(value);
}

inline size_t ToIndex(int64_t value)
{
    if (value <= 0)
    {
        return 0;
    }

    if (static_cast<uint64_t>(value) > std::numeric_limits<size_t>::max())
    {
        std::abort();
    }

    return static_cast<size_t>(value);
}

template<typename Number>
std::uint64_t ToWord(Number value)
{
    if (!(value > 0))
    {
        return 0;
    }

    if (value >= static_cast<Number>(0x1p64))
    {
        return std::numeric_limits<std::uint64_t>::max();
    }

    return static_cast<std::uint64_t>(value);
}
}
