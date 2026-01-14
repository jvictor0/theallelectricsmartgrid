#pragma once

#include <algorithm>

template<typename T, size_t N>
struct Array
{
    T m_data[N];
    size_t m_size;

    Array()
        : m_size(0)
    {
    }

    void Add(const T& value)
    {
        m_data[m_size] = value;
        m_size++;
    }

    T& operator[](size_t index)
    {
        return m_data[index];
    }

    void Clear()
    {
        m_size = 0;
    }

    size_t Size() const
    {
        return m_size;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin()
    {
        return m_data;
    }

    iterator end()
    {
        return m_data + m_size;
    }

    const_iterator begin() const
    {
        return m_data;
    }

    const_iterator end() const
    {
        return m_data + m_size;
    }

    void Sort()
    {
        std::sort(m_data, m_data + m_size);
    }

    void Sort(bool (*cmp)(const T& a, const T& b))
    {
        std::sort(m_data, m_data + m_size, cmp);
    }

    void ShrinkIfNecessary(size_t maxSize)
    {
        if (maxSize < m_size)
        {
            m_size = maxSize;
        }
    }

    T& Back()
    {
        return m_data[m_size - 1];
    }

    const T& Back() const
    {
        return m_data[m_size - 1];
    }

    void Pop()
    {
        m_size--;
    }

    bool Empty() const
    {
        return m_size == 0;
    }
};