#pragma once

#include <algorithm>

template <typename T, size_t N>
struct PriorityQueue
{
    T m_data[N];
    size_t m_size;

    PriorityQueue()
        : m_size(0)
    {
    }

    void Push(const T& value)
    {
        m_data[m_size] = value;
        ++m_size;
        std::push_heap(m_data, m_data + m_size);
    }

    T Pop()
    {
        std::pop_heap(m_data, m_data + m_size);
        --m_size;
        return m_data[m_size];
    }

    T Peek() const
    {
        return m_data[0];
    }

    bool IsEmpty() const
    {
        return m_size == 0;
    }

    size_t Size() const
    {
        return m_size;
    }
};