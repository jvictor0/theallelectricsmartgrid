#pragma once

#include "BasicWaveTable.hpp"
#include <algorithm>

// Overlap-add of prepared time-domain frames over Size interleaved channels.
// Write adds a frame at the clock index. Optional latency retains twice that
// many past samples for interpolated PM reads. Process retires the oldest
// sample and advances the clock independently of the read position.
// Windowing and normalization stay with the caller.
//
template<typename Number, size_t Size, size_t Bits, size_t Latency = 0>
struct OLAGeneric
{
    using Frame = BasicWaveTableGeneric<Number, Size, Bits>;
    using Vector = MultiChannelNumber<Number, Size>;

    static constexpr size_t x_frameSize = Frame::x_tableSize;
    static constexpr size_t x_latency = Latency;
    static constexpr size_t x_bufferSize = x_frameSize + 2 * x_latency;
    static constexpr size_t x_interpolationMargin = 2;
    static constexpr Number x_maxPhaseModulation = x_latency > x_interpolationMargin
        ? static_cast<Number>(x_latency - x_interpolationMargin) : Number(0);
    static_assert(x_latency == 0 || x_latency >= x_interpolationMargin);

    size_t m_index;
    Vector m_buffer[x_bufferSize]{};

    OLAGeneric()
        : m_index(0)
    {
    }

    void ClearChannel(size_t channel)
    {
        for (size_t i = 0; i < x_bufferSize; ++i)
        {
            m_buffer[i][channel] = 0;
        }
    }

    Vector Process()
    {
        if constexpr (x_latency > 0)
        {
            return Process(Vector::Splat(0));
        }
        else
        {
            Vector result = m_buffer[m_index];
            Advance();
            return result;
        }
    }

    Vector Process(const Vector& phaseModulation)
    {
        static_assert(x_latency >= x_interpolationMargin);
        Vector fraction;
        Vector y0;
        Vector y1;
        Vector y2;
        Vector y3;
        for (size_t channel = 0; channel < Size; ++channel)
        {
            assert(std::isfinite(phaseModulation[channel]));
            assert(std::abs(phaseModulation[channel]) <= x_maxPhaseModulation);
            Number offset = phaseModulation[channel] - static_cast<Number>(x_latency);
            Number floored = std::floor(offset);
            fraction[channel] = offset - floored;
            size_t index = WrapIndex(static_cast<size_t>(
                static_cast<int64_t>(m_index + x_bufferSize) + static_cast<int64_t>(floored)));
            y0[channel] = m_buffer[WrapIndex(index + x_bufferSize - 1)][channel];
            y1[channel] = m_buffer[index][channel];
            y2[channel] = m_buffer[WrapIndex(index + 1)][channel];
            y3[channel] = m_buffer[WrapIndex(index + 2)][channel];
        }

        // Catmull-Rom interpolation uses the same kernel as wavetable reads.
        //
        Vector a0 = y0 * -0.5f + y1 * 1.5f - y2 * 1.5f + y3 * 0.5f;
        Vector a1 = y0 - y1 * 2.5f + y2 * 2.0f - y3 * 0.5f;
        Vector a2 = y0 * -0.5f + y2 * 0.5f;
        Vector result = ((a0 * fraction + a1) * fraction + a2) * fraction + y1;
        Advance();
        return result;
    }

    static size_t WrapIndex(size_t index)
    {
        return index < x_bufferSize ? index : index - x_bufferSize;
    }

    void Advance()
    {
        m_buffer[WrapIndex(m_index + x_frameSize)] = Vector::Splat(0);
        ++m_index;
        if (m_index == x_bufferSize)
        {
            m_index = 0;
        }
    }

    void Write(const Frame& frame)
    {
        size_t split = std::min(x_frameSize, x_bufferSize - m_index);
        for (size_t i = 0; i < split; ++i)
        {
            m_buffer[m_index + i] += frame.m_table[i];
        }

        for (size_t i = split; i < x_frameSize; ++i)
        {
            m_buffer[i - split] += frame.m_table[i];
        }
    }
};
