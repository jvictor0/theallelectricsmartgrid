#pragma once

#include "MultiChannelArray.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

// Native storage for DSP.DelayBuffer. The head is the next slot to write.
//
template<typename Number, size_t Size, size_t Capacity>
struct CircularBufferGeneric
{
    static_assert(Capacity > 0);
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_capacity = Capacity;
    static constexpr bool x_modularWrap = (Capacity & (Capacity - 1)) == 0 &&
        Capacity <= std::numeric_limits<size_t>::max() / 2;
    MultiChannelArray<Number, Size, Capacity> m_data{};
    size_t m_head = 0;

    size_t Address(size_t age) const
    {
        assert(age < Capacity);
        assert(m_head < Capacity);
        if constexpr (x_modularWrap)
        {
            // DSP.DelayBuffer.address_mod and modulo_intermediates_bounded.
            //
            return (m_head + Capacity - (age + 1)) % Capacity;
        }
        else
        {
            return age < m_head ? m_head - (age + 1) : m_head + (Capacity - (age + 1));
        }
    }

    Vector ReadAge(size_t age) const
    {
        return m_data[Address(age)];
    }

    Vector ReadWrappedAge(std::uint64_t age) const
    {
        return ReadAge(age % Capacity);
    }

    template<typename Index, size_t Count>
    std::array<Vector, Count> ReadAgeOffsets(const MultiChannelNumber<Index, Size>& ages,
        const std::array<std::uint64_t, Count>& offsets) const
    {
        return m_data.GatherWrappedOffsets([&](size_t lane)
        {
            return Address(ages[lane] % Capacity);
        }, offsets);
    }

    Vector ReadDelay(size_t delay) const
    {
        assert(delay > 0 && delay <= Capacity);
        return ReadAge(delay - 1);
    }

    Vector ReadDelays(const MultiChannelNumber<size_t, Size>& delays) const
    {
        Vector result;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            result[lane] = ReadDelay(delays[lane])[lane];
        }

        return result;
    }

    void Push(const Vector& value)
    {
        assert(m_head < Capacity);
        m_data[m_head] = value;
        const auto next = m_head + 1;
        if constexpr (x_modularWrap)
        {
            // DSP.DelayBuffer.advance_mod.
            //
            m_head = next % Capacity;
        }
        else
        {
            m_head = next < Capacity ? next : 0;
        }
    }

    void Clear()
    {
        m_data.fill(Vector::Splat(0));
        m_head = 0;
    }
};
