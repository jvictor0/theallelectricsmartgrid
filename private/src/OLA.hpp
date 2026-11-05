#pragma once

#include "BasicWaveTable.hpp"

// Overlap-add of prepared time-domain frames over Size interleaved channels.
// Write adds a frame starting at the next sample Process returns; Process
// reads and clears one sample. Windowing and normalization stay with the
// caller.
//
template<typename Number, size_t Size, size_t Bits>
struct OLAGeneric
{
    using Frame = BasicWaveTableGeneric<Number, Size, Bits>;
    using Vector = MultiChannelNumber<Number, Size>;

    static constexpr size_t x_frameSize = Frame::x_tableSize;

    size_t m_index;
    Frame m_buffer;

    OLAGeneric()
        : m_index(0)
    {
    }

    void ClearChannel(size_t channel)
    {
        for (size_t i = 0; i < x_frameSize; ++i)
        {
            m_buffer.m_table[i][channel] = 0;
        }
    }

    Vector Process()
    {
        Vector result = m_buffer.m_table[m_index];
        m_buffer.m_table[m_index] = Vector::Splat(0);
        ++m_index;
        if (m_index == x_frameSize)
        {
            m_index = 0;
        }

        return result;
    }

    void Write(const Frame& frame)
    {
        size_t split = x_frameSize - m_index;
        for (size_t i = 0; i < split; ++i)
        {
            m_buffer.m_table[m_index + i] += frame.m_table[i];
        }

        for (size_t i = split; i < x_frameSize; ++i)
        {
            m_buffer.m_table[i - split] += frame.m_table[i];
        }
    }
};
