#pragma once

#include "MultiChannelNumber.hpp"
#include <cstdint>
#include <cstring>

template<size_t Size>
struct MultiChannelBool
{
    static constexpr size_t x_numLanes = MultiChannelNumber<float, Size>::x_numLanes;
    using Vector = bool __attribute__((ext_vector_type(x_numLanes)));

    Vector m_values{};

    template<class ComparisonVector>
    static MultiChannelBool FromComparison(ComparisonVector comparison)
    {
        return {__builtin_convertvector(comparison, Vector)};
    }

    bool operator[](size_t lane) const
    {
        return m_values[lane];
    }

    uint64_t ToBits() const
    {
        static_assert(Size <= 64);
        // Clang packs one bit per lane. Exclude any padding above the real channels.
        //
        uint64_t bits = 0;
        std::memcpy(&bits, &m_values, sizeof(m_values));
        return bits & (UINT64_MAX >> (64 - Size));
    }
};
