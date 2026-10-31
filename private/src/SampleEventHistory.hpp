#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

// One audio-thread writer, with immutable events until their slots are reused.
// Atomic fields and generation checks let UI readers reject an overwritten event
// without locking the audio thread. Sequential consistency is inexpensive here:
// writes happen at note/round boundaries, not at audio rate.
//
template<size_t x_numValues, size_t x_capacity>
struct SampleEventHistory
{
    static_assert(x_capacity > 0);
    static_assert(std::atomic<double>::is_always_lock_free);

    struct Event
    {
        size_t m_sample = 0;
        std::array<double, x_numValues> m_values{};
    };

    struct Slot
    {
        std::atomic<size_t> m_generation{0};
        std::atomic<size_t> m_sample{0};
        std::array<std::atomic<double>, x_numValues> m_values{};
    };

    std::array<Slot, x_capacity> m_slots;
    std::atomic<size_t> m_count{0};

    void Record(const Event& event)
    {
        size_t index = m_count.load();
        Slot& slot = m_slots[index % x_capacity];
        slot.m_generation.store(0);
        slot.m_sample.store(event.m_sample);
        for (size_t i = 0; i < x_numValues; ++i)
        {
            slot.m_values[i].store(event.m_values[i]);
        }

        slot.m_generation.store(index + 1);
        m_count.store(index + 1);
    }

    bool Read(size_t index, Event& event) const
    {
        const Slot& slot = m_slots[index % x_capacity];
        if (slot.m_generation.load() != index + 1)
        {
            return false;
        }

        event.m_sample = slot.m_sample.load();
        for (size_t i = 0; i < x_numValues; ++i)
        {
            event.m_values[i] = slot.m_values[i].load();
        }

        return slot.m_generation.load() == index + 1;
    }

    // Events must be recorded in nondecreasing sample order. The caller bounds
    // the query by its published audio position; this finds the held event.
    //
    bool FindAtOrBefore(double sample, Event& event) const
    {
        if (!std::isfinite(sample) || sample < 0.0)
        {
            return false;
        }

        size_t end = m_count.load();
        size_t begin = end > x_capacity ? end - x_capacity : 0;
        size_t low = begin;
        size_t high = end;
        while (low < high)
        {
            size_t middle = low + (high - low) / 2;
            Event candidate;
            if (!Read(middle, candidate))
            {
                return false;
            }

            if (static_cast<double>(candidate.m_sample) <= sample)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }

        return low > begin && Read(low - 1, event);
    }
};
