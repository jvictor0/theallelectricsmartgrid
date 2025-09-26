#pragma once

template <typename T, size_t N>
struct FixedAllocator
{
    T m_data[N];
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
        INFO("Allocating %d", index);
        m_freeQueueTail++;
        return &m_data[index];
    }

    void Free(T* ptr)
    {
        assert(Available() < N);        

        int index = ptr - m_data;
        INFO("Freeing %d", index);
        m_freeQueue[m_freeQueueHead % N] = index;
        m_freeQueueHead++;
    }
};