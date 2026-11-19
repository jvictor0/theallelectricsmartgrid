#pragma once

#include "MultiChannelNumber.hpp"
#include "CircularBufferGeneric.hpp"
#include "SchroederAllPassCoreGeneric.hpp"
#include <cassert>
#include <cstddef>

// Schroeder all-pass with one interleaved delay line shared by all channels and
// a delay length per channel.
//
template <typename Number, size_t Size>
struct AllPassFilterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_maxDelaySamples = 4096;

    CircularBufferGeneric<Number, Size, x_maxDelaySamples> m_buffer;
    Vector m_gain;
    int m_delaySamples[Size];
    bool m_uniformDelay = true;
    Vector m_output;

    AllPassFilterGeneric()
        : m_gain(Vector::Splat(static_cast<Number>(0.5)))
        , m_output(Vector::Splat(0))
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_delaySamples[i] = 0;
        }

        Clear();
    }

    void Clear()
    {
        m_buffer.Clear();
        m_output = Vector::Splat(0);
    }

    Vector Process(const Vector& x)
    {
        Vector delayed;
        if (m_uniformDelay)
        {
            delayed = m_buffer.ReadDelay(m_delaySamples[0] == 0 ? x_maxDelaySamples : static_cast<size_t>(m_delaySamples[0]));
        }
        else
        {
            for (size_t i = 0; i < Size; ++i)
            {
                size_t delay = m_delaySamples[i] == 0 ? x_maxDelaySamples : static_cast<size_t>(m_delaySamples[i]);
                delayed[i] = m_buffer.ReadDelay(delay)[i];
            }
        }

        SchroederAllPassCoreGeneric<Number, Size> core{{m_gain}, {}};
        typename SchroederAllPassCoreGeneric<Number, Size>::Input input{x, delayed};
        core.Process(input);
        m_output = core.m_output.m_y;
        m_buffer.Push(core.m_output.m_written);
        return m_output;
    }

    Vector GetOutput() const
    {
        return m_output;
    }

    void SetDelaySamples(int channel, int delaySamples)
    {
        assert(channel >= 0 && static_cast<size_t>(channel) < Size);
        assert(delaySamples >= 0 && static_cast<size_t>(delaySamples) <= x_maxDelaySamples);
        m_delaySamples[channel] = delaySamples;
        m_uniformDelay = true;
        for (size_t i = 1; i < Size; ++i)
        {
            m_uniformDelay = m_uniformDelay && m_delaySamples[i] == m_delaySamples[0];
        }
    }

    int GetDelaySamples(int channel) const
    {
        return m_delaySamples[channel];
    }

    void SetGain(Number gain)
    {
        m_gain = Vector::Splat(gain);
    }
};

using AllPassFilter = AllPassFilterGeneric<float, 1>;
using StereoAllPassFilter = AllPassFilterGeneric<float, 2>;
using QuadAllPassFilter = AllPassFilterGeneric<float, 4>;

// Stages all-pass filters fed the same input, outputs averaged.
//
template <typename Number, size_t Size, size_t Stages>
struct ParallelAllPassFilterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    AllPassFilterGeneric<Number, Size> m_filters[Stages];
    Vector m_output;

    ParallelAllPassFilterGeneric()
        : m_filters{}
        , m_output(Vector::Splat(0))
    {
    }

    void SetDelaySamples(int stage, int delaySamples)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_filters[stage].SetDelaySamples(static_cast<int>(i), delaySamples);
        }
    }

    void SetDelaySamples(int stage, int channel, int delaySamples)
    {
        m_filters[stage].SetDelaySamples(channel, delaySamples);
    }

    void SetGain(Number gain)
    {
        for (size_t i = 0; i < Stages; ++i)
        {
            m_filters[i].SetGain(gain);
        }
    }

    Vector Process(const Vector& input)
    {
        m_output = Vector::Splat(0);
        for (size_t i = 0; i < Stages; ++i)
        {
            m_output += m_filters[i].Process(input);
        }

        m_output /= static_cast<Number>(Stages);
        return m_output;
    }

    Vector GetOutput() const
    {
        return m_output;
    }
};

template <size_t Stages>
using ParallelAllPassFilter = ParallelAllPassFilterGeneric<float, 1, Stages>;

template <size_t Stages>
using StereoParallelAllPassFilter = ParallelAllPassFilterGeneric<float, 2, Stages>;

template <size_t Stages>
using QuadParallelAllPassFilter = ParallelAllPassFilterGeneric<float, 4, Stages>;
