#pragma once
#include "ModuleUtils.hpp"
#include "Slew.hpp"
#include "WaveTable.hpp"
#include "QuadUtils.hpp"

struct DelayLine
{
    static constexpr size_t x_maxDelaySamples = 1 << 16;
    float m_delayLine[x_maxDelaySamples] = {0.0f};
    size_t m_index;

    DelayLine()
        : m_index(0)
    {
    }

    void Write(float x)
    {
        m_delayLine[m_index] = x;
        m_index = (m_index + 1) % x_maxDelaySamples;
    }

    float MirrorDelayTime(float delaySamples)
    {
        while (delaySamples < 0.0f || delaySamples > x_maxDelaySamples)
        {
            if (delaySamples < 0.0f)
            {
                delaySamples = -delaySamples;
            }
            else
            {
                delaySamples = 2 * x_maxDelaySamples - delaySamples;
            }
        }

        return delaySamples;
    }        

    float Read(float delaySamples)
    {
        delaySamples = MirrorDelayTime(delaySamples);
        
        // use cubic interpolation
        //
        float index = m_index - delaySamples + 2 * x_maxDelaySamples;

        size_t iSub1 = static_cast<size_t>(index - 1) % x_maxDelaySamples;
        size_t i0 = (iSub1 + 1) % x_maxDelaySamples;
        size_t i1 = (iSub1 + 2) % x_maxDelaySamples;
        size_t i2 = (iSub1 + 3) % x_maxDelaySamples;

        float c = m_delayLine[i1] - m_delayLine[iSub1];
        float d = 2 * m_delayLine[iSub1] - 5 * m_delayLine[i0] + 4 * m_delayLine[i1] - m_delayLine[i2];
        float e = -m_delayLine[iSub1] + 3 * (m_delayLine[i0] - m_delayLine[i1]) + m_delayLine[i2];
        float alpha = index - std::floor(index);
        
        return m_delayLine[i0] + alpha * (c + alpha * (d + alpha * e)) / 2;
    }
};

struct AllPassFilter
{
    static constexpr size_t x_maxDelaySamples = 4096;
    float m_delayLine[x_maxDelaySamples] = {0.0f};
    int m_index;
    float m_gain;
    int m_delaySamples;
    float m_output;

    AllPassFilter()
        : m_index(0)
        , m_gain(0.5f)
        , m_delaySamples(0)
        , m_output(0.0f)
    {
        Clear();
    }

    void Clear()
    {
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine[i] = 0.0f;
        }
        
        m_index = 0;
        m_output = 0.0f;
    }
    
    float Process(float x)
    {
        m_output = -m_gain * x + m_delayLine[(m_index + x_maxDelaySamples - m_delaySamples) % x_maxDelaySamples];
        m_delayLine[m_index] = x + m_gain * m_output;
        m_index = (m_index + 1) % x_maxDelaySamples;
        return m_output;
    }
};

template<int Size>
struct ParallelAllPassFilter
{
    AllPassFilter m_filters[Size];
    float m_output;

    void SetDelaySamples(int i, size_t delaySamples)
    {
        m_filters[i].m_delaySamples = delaySamples;
    }

    void SetGain(float gain)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_filters[i].m_gain = gain;
        }
    }

    float Process(float input)
    {
        m_output = 0;
        for (size_t i = 0; i < Size; ++i)
        {
            m_output += m_filters[i].Process(input);
        }

        m_output /= Size;
        return m_output;
    }
};

struct QuadDelayLine
{
    DelayLine m_delayLine[4];

    void Write(QuadFloat x)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_delayLine[i].Write(x[i]);
        }
    }

    QuadFloat Read(QuadFloat delaySamples)
    {
        return QuadFloat(
            m_delayLine[0].Read(delaySamples[0]),
            m_delayLine[1].Read(delaySamples[1]),
            m_delayLine[2].Read(delaySamples[2]),
            m_delayLine[3].Read(delaySamples[3]));
    }
};

struct QuadAllPassFilter
{
    AllPassFilter m_allPassFilter[4];

    QuadFloat Process(QuadFloat x)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_allPassFilter[i].Process(x[i]);
        }

        return GetOutput();
    }

    QuadFloat GetOutput()
    {
        return QuadFloat(
            m_allPassFilter[0].m_output,
            m_allPassFilter[1].m_output,
            m_allPassFilter[2].m_output,
            m_allPassFilter[3].m_output);
    }

    void SetDelaySamples(int i, int delaySamples)
    {
        m_allPassFilter[i].m_delaySamples = delaySamples;
    }

    int GetDelaySamples(int i)
    {
        return m_allPassFilter[i].m_delaySamples;
    }

    void SetGain(float gain)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_allPassFilter[i].m_gain = gain;
        }
    }
};

template<size_t Size>
struct QuadParallelAllPassFilter
{
    ParallelAllPassFilter<Size> m_allPassFilter[4];

    QuadFloat Process(QuadFloat x)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_allPassFilter[i].Process(x[i]);
        }

        return GetOutput();
    }

    QuadFloat GetOutput()
    {
        return QuadFloat(
            m_allPassFilter[0].m_output,
            m_allPassFilter[1].m_output,
            m_allPassFilter[2].m_output,
            m_allPassFilter[3].m_output);
    }

    void SetDelaySamples(int i, int j, int delaySamples)
    {
        m_allPassFilter[j].SetDelaySamples(i, delaySamples);
    }

    void SetGain(float gain)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_allPassFilter[i].SetGain(gain);
        }
    }
};
