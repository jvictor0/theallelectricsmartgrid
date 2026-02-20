#pragma once
#include "Slew.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "Filter.hpp"
#include "CircleTracker.hpp"
#include "NormGen.hpp"
#include "InterleavedArray.hpp"
#include "Math.hpp"
#include "Resynthesis.hpp"

struct XFader
{
    static constexpr float x_fadeIncr = 1.0 / (48000.0 * 0.005);

    double m_left;
    double m_right;
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
                m_fade = 0;
            }
        }
    }

    void Start(double right)
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

    double GetValue(bool left)
    {
        return left ? m_left : m_right;
    }

    float GetFade(bool left)
    {
        return left ? 1.0f - m_fade : m_fade;
    }

    template<class T, double (T::*ReadFunc)(double)>
    double Read(T* delayLine)
    {
        if (m_fadeDone)
        {
            return (delayLine->*ReadFunc)(m_left);
        }
        else
        {
            double readLeft = (delayLine->*ReadFunc)(m_left);
            double readRight = (delayLine->*ReadFunc)(m_right);
            return readLeft * (1.0f - m_fade) + readRight * m_fade;
        }
    }
};

struct QuadXFader
{
    XFader m_fader[4];

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

    QuadXFader ModOne()
    {
        QuadXFader result;
        for (size_t i = 0; i < 4; ++i)
        {
            result.m_fader[i].m_left = m_fader[i].m_left - std::floor(m_fader[i].m_left);
            result.m_fader[i].m_right = m_fader[i].m_right - std::floor(m_fader[i].m_right);
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

    template<class T, double (T::*ReadFunc)(double)>
    QuadDouble Read(T* delayLine)
    {
        QuadDouble result;
        for (size_t i = 0; i < 4; ++i)
        {
            result[i] = m_fader[i].template Read<T, ReadFunc>(delayLine);
        }

        return result;
    }
};

template<size_t Size = 1 << 16>
struct DelayLine
{
    static constexpr size_t x_maxDelaySamples = Size;
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
        return fader.Read<DelayLine, &DelayLine::Read>(this);
    }
};

template<size_t Size>
struct DelayLineMovableWriter
{
    static constexpr size_t x_maxDelaySamples = Size;
    InterleavedArrayHolder<float, x_maxDelaySamples, 4> m_delayLine;
    InterleavedArrayHolder<double, x_maxDelaySamples, 4> m_writeHeadInverse;

    static constexpr size_t x_scatterBufferSize = 8;
    size_t m_scatterHead;
    double m_scatterBufferWarpedTime[x_scatterBufferSize] = {0.0};
    double m_scatterBufferTime[x_scatterBufferSize] = {0.0};

    static constexpr double x_turnaroundFadeSamples = 0.005 * 48000.0;
    double m_turnaroundWarpedTime;
    bool m_descending;
    double m_lastWarpedTime;
    size_t m_ascendingCount;

    double m_lastTime;

    DelayLineMovableWriter()
        : m_scatterHead(x_scatterBufferSize)
        , m_turnaroundWarpedTime(0.0)
        , m_descending(false)
        , m_lastWarpedTime(0.0)
        , m_ascendingCount(0)
        , m_lastTime(17700716)
    {
    }

    void SetArray(InterleavedArray<float, x_maxDelaySamples, 4>* delayLine, InterleavedArray<double, x_maxDelaySamples, 4>* writeHeadInverse, size_t foldIx)
    {
        m_delayLine.m_array = delayLine;
        m_delayLine.m_foldIx = foldIx;
        m_writeHeadInverse.m_array = writeHeadInverse;
        m_writeHeadInverse.m_foldIx = foldIx;

        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine[i] = 0.0f;
            m_writeHeadInverse[i] = i;
        }
    }

    float GetFade(double time)
    {
        return std::min(1.0, (time - m_turnaroundWarpedTime) / x_turnaroundFadeSamples);
    }

    void ScatterBufferToDelayLine()
    {
        double timeLowSamples = m_scatterBufferWarpedTime[(m_scatterHead - 2) % x_scatterBufferSize];
        double timeHighSamples = m_scatterBufferWarpedTime[(m_scatterHead - 1) % x_scatterBufferSize];

        size_t timeLowIndex = std::ceil(timeLowSamples);
        size_t timeHighIndex = std::ceil(timeHighSamples);

        for (size_t i = timeLowIndex; i < timeHighIndex; ++i)
        {
            double time = static_cast<double>(i);
            double value = PhaseUtils::CubicLagrangeNonUniform(
                m_scatterBufferWarpedTime[(m_scatterHead - 3) % x_scatterBufferSize], m_scatterBufferTime[(m_scatterHead - 3) % x_scatterBufferSize],
                m_scatterBufferWarpedTime[(m_scatterHead - 2) % x_scatterBufferSize], m_scatterBufferTime[(m_scatterHead - 2) % x_scatterBufferSize],
                m_scatterBufferWarpedTime[(m_scatterHead - 1) % x_scatterBufferSize], m_scatterBufferTime[(m_scatterHead - 1) % x_scatterBufferSize],
                m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize], m_scatterBufferTime[m_scatterHead % x_scatterBufferSize],
                time);
            m_writeHeadInverse[i % x_maxDelaySamples] = value;
        }
    }

    void Write(float x, double time)
    {
        if (time < m_lastWarpedTime)
        {
            m_descending = true;
            m_ascendingCount = 0;
        }
        else if (m_descending)
        {
            m_turnaroundWarpedTime = time;
            m_descending = false;
        }

        m_lastWarpedTime = time;
        if (!m_descending)
        {
            ++m_ascendingCount;
            m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize] = time;
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = m_lastTime;
            if (4 <= m_ascendingCount)
            {
                ScatterBufferToDelayLine();
            }

            ++m_scatterHead;
        }

        size_t index = static_cast<size_t>(m_lastTime) % x_maxDelaySamples;
        m_delayLine[index] = x;
        if (index == 0)
        {
            INFO("Delay buffer wrap around");            
        }

        m_lastTime += 1.0;
    }

    template<class T>
    T ReadAtIndex(double index, InterleavedArrayHolder<T, x_maxDelaySamples, 4>& line)
    {
        size_t iSub1 = static_cast<size_t>(index - 1 + x_maxDelaySamples) % x_maxDelaySamples;
        size_t i0 = (iSub1 + 1) % x_maxDelaySamples;
        size_t i1 = (iSub1 + 2) % x_maxDelaySamples;
        size_t i2 = (iSub1 + 3) % x_maxDelaySamples;

        T c = line[i1] - line[iSub1];
        T d = 2 * line[iSub1] - 5 * line[i0] + 4 * line[i1] - line[i2];
        T e = -line[iSub1] + 3 * (line[i0] - line[i1]) + line[i2];
        T alpha = index - std::floor(index);
        
        return line[i0] + alpha * (c + alpha * (d + alpha * e)) / 2;
    }

    float Read(double warpedTime)
    {
        double realTime = GetRealTime(warpedTime);
        return ReadAtIndex(realTime, m_delayLine);
    }

    float ReadWithOffset(double warpedTime, double offset)
    {
        return ReadAtIndex(GetRealTime(warpedTime) + offset, m_delayLine);
    }

    float ReadWithOffset(XFader fader, double offset)
    {
        if (fader.m_fadeDone)
        {
            return ReadWithOffset(fader.m_left, offset);
        }
        else
        {
            float readLeft = ReadWithOffset(fader.m_left, offset);
            float readRight = ReadWithOffset(fader.m_right, offset);
            return readLeft * (1.0f - fader.m_fade) + readRight * fader.m_fade;
        }
    }

    double GetRealTime(double warpedTime)
    {
        return ReadAtIndex(warpedTime, m_writeHeadInverse);
    }

    float ReadRealTime(double realTime)
    {
        return ReadAtIndex(realTime, m_delayLine);
    }

    float Read(XFader fader)
    {
        return fader.Read<DelayLineMovableWriter, &DelayLineMovableWriter::Read>(this);
    }
};

template<size_t Size>
struct GrainManager
{
    struct Grain
    {
        Grain()
            : m_owner(nullptr)
        {
        }

        void Reset()
        {
        }

        float Process()
        {
            return m_grain.Process() / 1.5;
        }

        void Start(double wallTime, Resynthesizer::Input& input, double startTime, double warpedTime)
        {
            Resynthesizer::Buffer prevTable;

            double rms = 0;

            for (size_t i = 0; i < Resynthesizer::x_tableSize; ++i)
            {
                float window = Math4096::Hann(i);
                m_grain.m_buffer.m_table[i] = m_delayLine->ReadRealTime(startTime + i) * window;
                rms += m_grain.m_buffer.m_table[i] * m_grain.m_buffer.m_table[i];
            }

            for (size_t i = 0; i < Resynthesizer::x_tableSize; ++i)
            {
                float window = Math4096::Hann(i);
                prevTable.m_table[i] = m_delayLine->ReadRealTime(startTime + i - Resynthesizer::x_H) * window;
            }

            m_owner->m_resynthesizer.Process(prevTable, &m_grain, input);
        }

        bool IsRunning() const
        {
            return m_grain.m_running;
        }

        Resynthesizer::Grain m_grain;
        DelayLineMovableWriter<Size>* m_delayLine;
        GrainManager* m_owner;
    };
    
    static constexpr size_t x_maxGrains = 1024;
    FixedAllocator<Grain, x_maxGrains> m_grainsAlloc;
    Grain* m_grainsArray[x_maxGrains];
    size_t m_numGrains;
    DelayLineMovableWriter<Size>* m_delayLine;
    RGen m_rgen;
    int m_samplesToNextGrain;
    double m_lastSampleOffset;
    Resynthesizer m_resynthesizer;

    void CompactGrains()
    {
        for (size_t i = 0; i < m_numGrains;)
        {
            if (!m_grainsArray[i])        
            {
                m_grainsArray[i] = m_grainsArray[m_numGrains - 1];
                --m_numGrains;
            }
            else
            {
                ++i;
            }
        }
    }

    Grain* AllocateGrain()
    {
        Grain* grain = m_grainsAlloc.Allocate();
        if (grain)
        {
            grain->m_delayLine = m_delayLine;
            grain->m_owner = this;
            ++m_numGrains;
            m_grainsArray[m_numGrains - 1] = grain;
        }
        else
        {
            INFO("Failed to allocate grain");
        }

        return grain;
    }

    float ProcessGrains()
    {
        bool anyFreed = false;
        float result = 0.0f;
        for (size_t i = 0; i < m_numGrains; ++i)
        {
            result += m_grainsArray[i]->Process();
            if (!m_grainsArray[i]->IsRunning())
            {
                anyFreed = true;
                m_grainsArray[i]->Reset();
                m_grainsAlloc.Free(m_grainsArray[i]);
                m_grainsArray[i] = nullptr;                
            }
        }

        if (anyFreed)
        {
            CompactGrains();
        }

        return result;
    }

    struct Input
    {
        Resynthesizer::Input m_resynthInput;
    };

    float Process(double warpedTime, double sampleOffset, Input& input)
    {
        float result = ProcessGrains();
        --m_samplesToNextGrain;
        if (m_samplesToNextGrain <= 0)
        {
            Grain* grain = AllocateGrain();
            if (grain)
            {
                double startTime = m_delayLine->GetRealTime(warpedTime) + sampleOffset;
                grain->Start(m_delayLine->m_lastTime, input.m_resynthInput, startTime, warpedTime);
            }

            m_samplesToNextGrain = Resynthesizer::GetGrainLaunchSamples();
        }

        m_lastSampleOffset = sampleOffset;

        return result;
    }

    GrainManager()
        : m_delayLine(nullptr)
        , m_samplesToNextGrain(0)
        , m_lastSampleOffset(0)
    {
        m_numGrains = 0;
        for (size_t i = 0; i < x_maxGrains; ++i)
        {
            m_grainsArray[i] = nullptr;
        }
    }

    struct UIState
    {
        static constexpr size_t x_maxFrames = 16;
        std::atomic<double> m_phaseOffsets[x_maxGrains][x_maxFrames];
        std::atomic<size_t> m_numGrains[x_maxFrames];
        std::atomic<size_t> m_which;

        double GetPhaseOffset(size_t i, size_t frame)
        {
            return m_phaseOffsets[i][frame].load();
        }

        size_t GetNumGrains(size_t frame)
        {
            return m_numGrains[frame].load();
        }

        size_t GetFrame()
        {
            return (m_which.load() - 1) % x_maxFrames;
        }

        float FrequencyResponse(size_t frame, float freq)
        {
            size_t numGrains = GetNumGrains(frame);
            float omega = 2 * M_PI * freq;
            std::complex<float> response = 0.0f;
            for (size_t i = 0; i < numGrains; ++i)
            {
                float phaseOffset = GetPhaseOffset(i, frame);
                response += std::exp(std::complex<float>(0.0f, omega * phaseOffset));
            }

            return std::abs(response) / 2.0f;
        }
        
    };

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_numGrains[uiState->m_which.load()].store(0);        
        uiState->m_numGrains[uiState->m_which.load()].store(m_numGrains);
        uiState->m_which.store((uiState->m_which.load() + 1) % UIState::x_maxFrames);
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

    ParallelAllPassFilter()
        : m_filters{}
        , m_output(0.0f)
    {
    }

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
        QuadFloat result;
        for (size_t i = 0; i < 4; ++i)
        {
            result[i] = fader.m_fader[i].Read<DelayLine<Size>, &DelayLine<Size>::Read>(&m_delayLine[i]);
        }

        return result;
    }

    struct UIState
    {
        typename DelayLine<Size>::UIState m_delayLineUIState[4];

        UIState()
        {
        }
    };   
    
    void PopulateUIState(UIState* uiState)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_delayLine[i].PopulateUIState(&uiState->m_delayLineUIState[i]);
        }
    }
};

template<size_t Size>
struct QuadDelayLineMovableWriter
{
    InterleavedArray<float, Size, 4> m_delayLineArray;
    InterleavedArray<double, Size, 4> m_writeHeadInverseArray;
    DelayLineMovableWriter<Size> m_delayLine[4];

    QuadDelayLineMovableWriter()
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_delayLine[i].SetArray(&m_delayLineArray, &m_writeHeadInverseArray, i);
        }
    }

    void Write(QuadFloat x, QuadDouble time)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_delayLine[i].Write(x[i], time[i]);
        }
    }

    QuadFloat Read(QuadXFader fader)
    {
        QuadFloat result;
        for (size_t i = 0; i < 4; ++i)
        {
            result[i] = m_delayLine[i].Read(fader.m_fader[i]);
        }

        return result;
    }
};

template<size_t Size>
struct QuadGrainManager
{
    GrainManager<Size> m_grainManager[4];

    struct Input
    {
        typename GrainManager<Size>::Input m_input[4];
    };

    QuadGrainManager(QuadDelayLineMovableWriter<Size>* delayLine)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_grainManager[i].m_delayLine = &delayLine->m_delayLine[i];
        }
    }

    QuadFloat Process(QuadDouble readHead, QuadFloat sampleOffset, Input& input)
    {
        QuadFloat result;
        for (size_t i = 0; i < 4; ++i)
        {
            result[i] = m_grainManager[i].Process(readHead[i], sampleOffset[i], input.m_input[i]);
        }

        return result;
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
