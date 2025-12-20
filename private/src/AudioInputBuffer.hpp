#pragma once

struct AudioInputBuffer
{
    static constexpr size_t x_numInputs = 16;
    float m_input[x_numInputs];
    size_t m_numInputs;

    AudioInputBuffer()
    {
        memset(m_input, 0, sizeof(m_input));
        m_numInputs = 0;
    }
};