#pragma once

#include <atomic>

template <typename T, size_t N>
struct CircularQueue
{
    T m_data[N];
    size_t m_head = 0;
    size_t m_tail = 0;

    bool Push(const T& value)
    {
        if (m_head - m_tail == N)
        {
            return false;
        }

        m_data[m_head] = value;
        m_head++;
        return true;
    }

    bool Pop(T& value)
    {
        if (m_head == m_tail)
        {
            return false;
        }

        value = m_data[m_tail];
        m_tail++;
        return true;
    }
};