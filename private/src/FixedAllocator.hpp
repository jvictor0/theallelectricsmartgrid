#pragma once

template <typename T, size_t N>
struct FixedAllocator
{
    T m_data[N];
    bool m_allocated[N];
    size_t m_index;

    int m_freeQueue[N];
    int m_freeQueueHead;
    int m_freeQueueTail;

    FixedAllocator()
    {
        m_index = 0;
        m_freeQueueHead = N;
        m_freeQueueTail = 0;

        for (size_t i = 0; i < N; ++i)
        {
            m_freeQueue[i] = i;
            m_allocated[i] = false;
        }
    }

    size_t Available() const
    {
        return m_freeQueueHead - m_freeQueueTail;
    }

    T* Allocate()
    {
        if (m_freeQueueHead == m_freeQueueTail)
        {
            return nullptr;
        }

        int index = m_freeQueue[m_freeQueueTail % N];
        m_freeQueueTail++;
        m_allocated[index] = true;
        return &m_data[index];
    }

    void Free(T* ptr)
    {
        assert(Available() < N);        

        int index = IndexOf(ptr);
        m_freeQueue[m_freeQueueHead % N] = index;
        m_freeQueueHead++;
        m_allocated[index] = false;
    }

    int IndexOf(T* ptr) const
    {
        return ptr - m_data;
    }

    bool IsAllocated(T* ptr) const
    {
        int index = IndexOf(ptr);
        return m_allocated[index];
    }
};