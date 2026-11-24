#pragma once

#include "MultiChannelNumber.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

template<typename Number, size_t Size, size_t Capacity>
struct MultiChannelArray : std::array<MultiChannelNumber<Number, Size>, Capacity>
{
    using Vector = MultiChannelNumber<Number, Size>;
    using Storage = std::array<Vector, Capacity>;
    using Storage::operator[];

    template<typename Index>
    Vector operator[](const MultiChannelNumber<Index, Size>& indices) const
    {
        static_assert(std::is_integral_v<Index>);
        Vector result;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            result[lane] = (*this)[indices[lane]][lane];
        }

        return result;
    }

    // Address supplies a valid starting index for each lane. Offsets are
    // caller-selected positions relative to that index, with circular wrap.
    //
    template<typename Address, size_t Count>
    std::array<Vector, Count> GatherWrappedOffsets(
        Address address, const std::array<std::uint64_t, Count>& offsets) const
    {
        static_assert(Capacity > 0);
        std::array<Vector, Count> result;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            const size_t base = address(lane);
            for (size_t tap = 0; tap < Count; ++tap)
            {
                const size_t offset = offsets[tap] % Capacity;
                size_t index;
                if constexpr (Capacity <= std::numeric_limits<size_t>::max() / 2)
                {
                    index = (base + offset) % Capacity;
                }
                else
                {
                    const size_t untilWrap = Capacity - base;
                    index = offset < untilWrap ? base + offset : offset - untilWrap;
                }

                result[tap][lane] = (*this)[index][lane];
            }
        }

        return result;
    }
};
