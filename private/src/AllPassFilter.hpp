#pragma once

#include "MultiChannelNumber.hpp"
#include <cstddef>

// Schroeder all-pass with one interleaved delay line shared by all channels and
// a delay length per channel.
//
template <typename Number, size_t Size>
struct AllPassFilterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_maxDelaySamples = 4096;

    Vector m_delayLine[x_maxDelaySamples];
    size_t m_index;
    Vector m_gain;
    int m_delaySamples[Size];
    Vector m_output;

    AllPassFilterGeneric()
        : m_index(0)
        , m_gain(Vector::Splat(static_cast<Number>(0.5)))
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
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine[i] = Vector::Splat(0);
        }

        m_index = 0;
        m_output = Vector::Splat(0);
    }

    Vector Process(const Vector& x)
    {
        Vector delayed;
        for (size_t i = 0; i < Size; ++i)
        {
            size_t readIndex = (m_index + x_maxDelaySamples - static_cast<size_t>(m_delaySamples[i])) % x_maxDelaySamples;
            delayed[i] = m_delayLine[readIndex][i];
        }

        m_output = -m_gain * x + delayed;
        m_delayLine[m_index] = x + m_gain * m_output;
        m_index = (m_index + 1) % x_maxDelaySamples;
        return m_output;
    }

    Vector GetOutput() const
    {
        return m_output;
    }

    void SetDelaySamples(int channel, int delaySamples)
    {
        m_delaySamples[channel] = delaySamples;
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
using QuadParallelAllPassFilter = ParallelAllPassFilterGeneric<float, 4, Stages>;
