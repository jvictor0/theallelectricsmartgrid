#pragma once

// Generated from DSP.DelayBuffer. Do not edit.
// Storage lowering is tested against native Lean, not formally verified.
//

#include "MultiChannelArray.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

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
            return (m_head + Capacity - (age + 1)) % Capacity;
        }
        else
        {
            const bool v0 = (age < m_head);
            size_t v6;
            if (v0)
            {
                const size_t v1 = (age + 1);
                const size_t v2 = (m_head < v1 ? 0 : m_head - v1);
                v6 = v2;
            }
            else
            {
                const size_t v3 = (age + 1);
                const size_t v4 = (Capacity < v3 ? 0 : Capacity - v3);
                const size_t v5 = (m_head + v4);
                v6 = v5;
            }

            return v6;
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
        if constexpr (x_modularWrap)
        {
            m_data[m_head] = value;
            m_head = (m_head + 1) % Capacity;
        }
        else
        {
            const size_t v0 = (m_head + 1);
            const bool v1 = (v0 < Capacity);
            size_t v3;
            if (v1)
            {
                const size_t v2 = (m_head + 1);
                v3 = v2;
            }
            else
            {
                v3 = 0;
            }

            m_data[m_head] = value;
            m_head = v3;
        }
    }

    void Clear()
    {
        m_data.fill(Vector::Splat(0));
        m_head = 0;
    }

};
