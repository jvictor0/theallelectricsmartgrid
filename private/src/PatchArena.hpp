#pragma once

#include "Json.hpp"
#include <atomic>
#include <cassert>

// Shared immutable patch storage. The owner may rewrite it only after all
// readers finish. A failed acquisition defers work; it never waits on audio.
//
struct PatchArena : JsonArena
{
    static_assert(std::atomic<int>::is_always_lock_free);
    std::atomic<int> m_readers{0};
    JSON m_patch;

    using JsonArena::JsonArena;

    bool TryWrite()
    {
        int expected = 0;
        return m_readers.compare_exchange_strong(expected, -1);
    }

    void FinishWrite(JSON patch)
    {
        assert(m_readers == -1);
        m_patch = patch;
        m_readers.store(0);
    }

    bool TryRead()
    {
        int readers = m_readers.load();
        return readers >= 0 && m_readers.compare_exchange_strong(readers, readers + 1);
    }

    // Called while the producer already owns a read or excludes arena reuse.
    //
    void Retain()
    {
        const int previous = m_readers.fetch_add(1);
        assert(previous >= 0);
    }

    void Release()
    {
        const int previous = m_readers.fetch_sub(1);
        std::ignore = previous;
        assert(previous > 0);
    }
};
