#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cassert>
#include <memory>

template<typename Snapshot, size_t Capacity = 10>
struct SnapshotUIState
{
    static constexpr size_t x_uiBufferSize = Capacity;

    std::atomic<size_t> m_which;
    std::array<Snapshot, x_uiBufferSize> m_snapshots;

    SnapshotUIState()
        : m_which(0)
        , m_snapshots{}
    {
    }

    Snapshot& BeginSnapshot()
    {
        size_t which = (m_which.load(std::memory_order_relaxed) + 1) % m_snapshots.size();
        return m_snapshots[which];
    }

    void CommitSnapshot()
    {
        size_t which = (m_which.load(std::memory_order_relaxed) + 1) % m_snapshots.size();
        m_which.store(which, std::memory_order_release);
    }

    Snapshot& GetCurrentSnapshot()
    {
        return m_snapshots[m_which.load(std::memory_order_acquire)];
    }

    const Snapshot& GetCurrentSnapshot() const
    {
        return m_snapshots[m_which.load(std::memory_order_acquire)];
    }
};

// Single audio producer, single UI consumer. A slot is owned only while copying;
// the producer skips an occupied slot rather than waiting for the UI. Readers
// retain their own copies, never references into the wrapping history.
//
template<typename Snapshot>
struct SampledSnapshotHistory
{
    static_assert(std::atomic<bool>::is_always_lock_free && std::atomic<size_t>::is_always_lock_free,
        "Sample publication must not acquire an implementation lock");

    struct Slot
    {
        std::atomic<bool> m_owned{false};
        size_t m_id = 0;
        Snapshot m_snapshot{};
    };

    size_t m_capacity;
    std::unique_ptr<Slot[]> m_slots;
    std::atomic<size_t> m_published{0};
    std::atomic<size_t> m_first{1};

    explicit SampledSnapshotHistory(size_t capacity)
        : m_capacity(capacity)
        , m_slots(std::make_unique<Slot[]>(capacity))
    {
        assert(capacity > 0);
    }

    bool Push(const Snapshot& snapshot)
    {
        size_t id = m_published.load(std::memory_order_relaxed) + 1;
        Slot& slot = m_slots[id % m_capacity];
        bool expected = false;
        if (!slot.m_owned.compare_exchange_strong(expected, true, std::memory_order_acquire))
        {
            return false;
        }

        slot.m_snapshot = snapshot;
        slot.m_id = id;
        slot.m_owned.store(false, std::memory_order_release);
        m_published.store(id, std::memory_order_release);
        return true;
    }

    void Clear()
    {
        m_first.store(m_published.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }

    bool Read(size_t id, Snapshot& snapshot)
    {
        Slot& slot = m_slots[id % m_capacity];
        bool expected = false;
        if (!slot.m_owned.compare_exchange_strong(expected, true, std::memory_order_acquire))
        {
            return false;
        }

        bool valid = id != 0 && slot.m_id == id;
        if (valid)
        {
            snapshot = slot.m_snapshot;
        }

        slot.m_owned.store(false, std::memory_order_release);
        return valid;
    }
};

