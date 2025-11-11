#pragma once
#include "ModuleUtils.hpp"
#include "Slew.hpp"
#include "WaveTable.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "Filter.hpp"

template<size_t Size = 1 << 16>
struct DelayLine
{
    static constexpr size_t x_maxDelaySamples = Size;
    float m_delayLine[x_maxDelaySamples] = {0.0f};
    size_t m_index;

    struct XFader
    {
        static constexpr float x_fadeIncr = 1.0 / (48000.0 * 0.005);

        float m_left;
        float m_right;
        float m_fade;
        bool m_fadeDone;
        bool m_leftSet;

        XFader()
            : m_left(0.0f)
            , m_right(0.0f)
            , m_fade(0.0f)
            , m_fadeDone(true)
            , m_leftSet(false)
        {
        }

        void Process()
        {
            if (!m_fadeDone)
            {
                m_fade += x_fadeIncr;
                if (m_fade >= 1.0f)
                {
                    m_fadeDone = true;
                    m_left = m_right;
                }
            }
        }

        void Start(float right)
        {
            if (!m_leftSet)
            {
                m_left = right;
                m_leftSet = true;
                m_fadeDone = true;
            }
            else
            {
                m_right = right;
                m_fade = 0.0f;
                m_fadeDone = false;
            }
        }
    };

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
        return ReadAtIndex(index);
    }

    float ReadAtIndex(float index)
    {
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

    float Read(XFader fader)
    {
        if (fader.m_fadeDone)
        {
            return Read(fader.m_left);
        }
        else
        {
            float readLeft = Read(fader.m_left);
            float readRight = Read(fader.m_right);
            return readLeft * (1.0f - fader.m_fade) + readRight * fader.m_fade;
        }
    }

    float ReadAsBuffer(double bufferSamples, double bufferPhase, double bufferPos, size_t curIndex)
    {
        double index;
        if (bufferPos <= bufferPhase)
        {
            index = curIndex - bufferSamples * (bufferPhase - bufferPos);
        }
        else
        {
            index = curIndex - bufferSamples * (1.0 - (bufferPos - bufferPhase));
        }

        if (x_maxDelaySamples <= curIndex - index)
        {
            return 0;
        }

        return ReadAtIndex(index);
    }

    struct UIState
    {
        std::atomic<size_t> m_index;
        std::atomic<DelayLine<Size>*> m_delayLine;

        UIState()
            : m_index(0)
            , m_delayLine(nullptr)
        {
        }

        void SetIndex(size_t index)
        {
            m_index.store(index);
        }

        size_t GetIndex()
        {
            return m_index.load();
        }

        void SetDelayLine(DelayLine<Size>* delayLine)
        {
            m_delayLine.store(delayLine);
        }

        DelayLine<Size>* GetDelayLine()
        {
            return m_delayLine.load();
        }

        float ReadAsBuffer(double bufferSamples, double bufferPhase, double bufferPos)
        {
            return GetDelayLine()->ReadAsBuffer(bufferSamples, bufferPhase, bufferPos, GetIndex());
        }
    };

    void PopulateUIState(UIState* uiState)
    {
        uiState->SetIndex(m_index);
        uiState->SetDelayLine(this);
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

template<size_t Size>
struct QuadDelayLine
{
    DelayLine<Size> m_delayLine[4];

    struct QuadXFader
    {
        typename DelayLine<Size>::XFader m_fader[4];

        QuadXFader ScaleOffset(QuadFloat scale, QuadFloat offset)
        {
            QuadXFader result;
            for (size_t i = 0; i < 4; ++i)
            {
                result.m_fader[i].m_left = scale[i] * m_fader[i].m_left + offset[i];
                result.m_fader[i].m_right = scale[i] * m_fader[i].m_right + offset[i];
                result.m_fader[i].m_fade = m_fader[i].m_fade;
                result.m_fader[i].m_fadeDone = m_fader[i].m_fadeDone;
                result.m_fader[i].m_leftSet = m_fader[i].m_leftSet;
            }

            return result;
        }

        void Process()
        {
            for (size_t i = 0; i < 4; ++i)
            {
                m_fader[i].Process();
            }
        }
    };

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

    QuadFloat Read(QuadXFader fader)
    {
        return QuadFloat(
            m_delayLine[0].Read(fader.m_fader[0]),
            m_delayLine[1].Read(fader.m_fader[1]),
            m_delayLine[2].Read(fader.m_fader[2]),
            m_delayLine[3].Read(fader.m_fader[3]));
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

struct DelayTimeSynchronizer
{
    double m_approxDelaySamples;
    double m_tempoFreq;
    double m_outDelaySamplesFactor;
    double m_outDelaySamples;
    float m_factor;

    OPLowPassFilter m_tempoFilter;

    DelayTimeSynchronizer()
        : m_approxDelaySamples(0.0)
        , m_tempoFreq(0.0)
        , m_outDelaySamplesFactor(0.0)
        , m_outDelaySamples(0.0)
        , m_factor(0.0)
    {
        m_tempoFilter.SetAlphaFromNatFreq(0.3 / 48000.0);
    }

    template<class XFaderType>
    void Update(XFaderType& fader, double tempoFreq, double delaySamples, double factor)
    {
        tempoFreq = m_tempoFilter.Process(tempoFreq);
        
        size_t factorIx = std::round(factor * 4);
        float possibleFactors[5] = {1.0 / 5, 1.0 / 3, 1.0, 3.0, 5.0};
        factor = possibleFactors[factorIx];

        if (tempoFreq != m_tempoFreq || delaySamples != m_approxDelaySamples || factor != m_factor)
        {
            m_tempoFreq = tempoFreq;
            if (!fader.m_fadeDone)
            {
                fader.m_right = m_outDelaySamplesFactor / tempoFreq;
            }
            else
            {
                m_approxDelaySamples = delaySamples;
                m_factor = factor;

                double delaySamplesFactor = std::pow(2.0, std::floor(std::log2(delaySamples * tempoFreq / factor))) * factor;
                m_outDelaySamples = delaySamplesFactor / tempoFreq;

                if (delaySamplesFactor != m_outDelaySamplesFactor)
                {
                    m_outDelaySamplesFactor = delaySamplesFactor;
                    fader.Start(m_outDelaySamples);
                }
                else
                {
                    fader.m_left = m_outDelaySamples;
                }
            }
        }
    }
};