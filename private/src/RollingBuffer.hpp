#pragma once

template<size_t Size>
struct RollingBuffer
{
    float m_buffer[Size];
    size_t m_index;

    RollingBuffer()
        : m_buffer{}
        , m_index(0)
    {
    }

    void Write(float value)
    {
        m_buffer[m_index] = value;
        m_index = (m_index + 1) % Size;
    }

    float Max()
    {
        return *std::max_element(m_buffer, m_buffer + Size);
    }

    float Min()
    {
        return *std::min_element(m_buffer, m_buffer + Size);
    }
};