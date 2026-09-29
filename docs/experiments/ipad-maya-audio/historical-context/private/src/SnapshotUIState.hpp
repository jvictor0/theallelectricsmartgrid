#pragma once

#include <array>
#include <atomic>
#include <cstddef>

template<typename Snapshot>
struct SnapshotUIState
{
    static constexpr size_t x_uiBufferSize = 10;

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

