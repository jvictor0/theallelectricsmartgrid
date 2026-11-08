#pragma once
#include "Slew.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "Filter.hpp"
#include "NormGen.hpp"
#include "Math.hpp"
#include "OLA.hpp"
#include "Resynthesis.hpp"
#include "SampleTimer.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

// Ring buffer of Size interleaved channels, each read at its own delay.
//
template <typename Number, size_t Size, size_t MaxDelaySamples>
struct DelayLineGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_maxDelaySamples = MaxDelaySamples;

    Vector m_delayLine[x_maxDelaySamples];
    size_t m_index;

    DelayLineGeneric()
        : m_index(0)
    {
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine[i] = Vector::Splat(0);
        }
    }

    void Write(const Vector& x)
    {
        m_delayLine[m_index] = x;
        m_index = (m_index + 1) % x_maxDelaySamples;
    }

    static Number MirrorDelayTime(Number delaySamples)
    {
        while (delaySamples < 0 || delaySamples > x_maxDelaySamples)
        {
            if (delaySamples < 0)
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

    Vector Read(const Vector& delaySamples)
    {
        return ReadCubic(delaySamples);
    }

    Vector ReadCubic(const Vector& delaySamples)
    {
        Vector ySub1, y0, y1, y2, alpha;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Number delay = MirrorDelayTime(delaySamples[lane]);
            Number roundedDelay = std::ceil(delay);
            size_t i0 = (m_index + x_maxDelaySamples - static_cast<size_t>(roundedDelay)) % x_maxDelaySamples;
            size_t iSub1 = (i0 + x_maxDelaySamples - 1) % x_maxDelaySamples;
            ySub1[lane] = m_delayLine[iSub1][lane];
            y0[lane] = m_delayLine[(iSub1 + 1) % x_maxDelaySamples][lane];
            y1[lane] = m_delayLine[(iSub1 + 2) % x_maxDelaySamples][lane];
            y2[lane] = m_delayLine[(iSub1 + 3) % x_maxDelaySamples][lane];
            alpha[lane] = roundedDelay - delay;
        }

        Vector c = y1 - ySub1;
        Vector d = Vector::FusedMultiplyAdd(ySub1, Vector::Splat(2),
            Vector::FusedMultiplyAdd(y0, Vector::Splat(-5), y1 * 4)) - y2;
        Vector e = Vector::FusedMultiplyAdd(y0 - y1, Vector::Splat(3), y2 - ySub1);
        return Vector::FusedMultiplyAdd(alpha * static_cast<Number>(0.5),
            Vector::FusedMultiplyAdd(alpha, Vector::FusedMultiplyAdd(alpha, e, d), c), y0);
    }

    Vector ReadLinear(const Vector& delaySamples)
    {
        Vector y0, y1, alpha;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Number delay = MirrorDelayTime(delaySamples[lane]);
            Number roundedDelay = std::ceil(delay);
            size_t i0 = (m_index + x_maxDelaySamples - static_cast<size_t>(roundedDelay)) % x_maxDelaySamples;
            y0[lane] = m_delayLine[i0][lane];
            y1[lane] = m_delayLine[(i0 + 1) % x_maxDelaySamples][lane];
            alpha[lane] = roundedDelay - delay;
        }

        return Vector::FusedMultiplyAdd(alpha, y1 - y0, y0);
    }

    Number ReadAtIndex(size_t channel, Number index)
    {
        size_t iSub1 = static_cast<size_t>(index - 1) % x_maxDelaySamples;
        size_t i0 = (iSub1 + 1) % x_maxDelaySamples;
        size_t i1 = (iSub1 + 2) % x_maxDelaySamples;
        size_t i2 = (iSub1 + 3) % x_maxDelaySamples;

        Number ySub1 = m_delayLine[iSub1][channel];
        Number y0 = m_delayLine[i0][channel];
        Number y1 = m_delayLine[i1][channel];
        Number y2 = m_delayLine[i2][channel];

        Number c = y1 - ySub1;
        Number d = 2 * ySub1 - 5 * y0 + 4 * y1 - y2;
        Number e = -ySub1 + 3 * (y0 - y1) + y2;
        Number alpha = index - std::floor(index);
        
        return y0 + alpha * (c + alpha * (d + alpha * e)) / 2;
    }
};

template <size_t MaxDelaySamples>
using DelayLine = DelayLineGeneric<float, 1, MaxDelaySamples>;

template <size_t MaxDelaySamples>
using QuadDelayLine = DelayLineGeneric<float, 4, MaxDelaySamples>;

// Delay line whose write head moves through warped time. All channels share one
// write head, so the warped-to-real time map is scalar while audio and envelopes
// are interleaved per channel.
//
template <typename Number, size_t Size, size_t MaxDelaySamples>
struct DelayLineMovableWriterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_maxDelaySamples = MaxDelaySamples;
    static constexpr size_t x_envelopeBucketSamples = 1024;
    static constexpr size_t x_envelopeBucketCount = x_maxDelaySamples / x_envelopeBucketSamples;

    Vector m_delayLine[x_maxDelaySamples];
    double m_writeHeadInverse[x_maxDelaySamples];
    Vector m_maxEnvelope[x_envelopeBucketCount];
    Vector m_minEnvelope[x_envelopeBucketCount];

    static constexpr size_t x_scatterBufferSize = 8;
    size_t m_scatterHead;
    double m_scatterBufferWarpedTime[x_scatterBufferSize] = {0.0};
    double m_scatterBufferTime[x_scatterBufferSize] = {0.0};

    static constexpr double x_turnaroundFadeSamples = SampleTimer::SecondsToHostSamples(0.005);
    double m_turnaroundWarpedTime;
    bool m_descending;
    double m_lastWarpedTime;
    size_t m_ascendingCount;

    double m_lastTime;

    DelayLineMovableWriterGeneric()
        : m_scatterHead(x_scatterBufferSize)
        , m_turnaroundWarpedTime(0.0)
        , m_descending(false)
        , m_lastWarpedTime(0.0)
        , m_ascendingCount(0)
        , m_lastTime(17700716)
    {
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine[i] = Vector::Splat(0);
            m_writeHeadInverse[i] = i;
        }

        for (size_t i = 0; i < x_envelopeBucketCount; ++i)
        {
            m_maxEnvelope[i] = Vector::Splat(0);
            m_minEnvelope[i] = Vector::Splat(0);
        }
    }

    float GetFade(double time)
    {
        return std::min(1.0, (time - m_turnaroundWarpedTime) / x_turnaroundFadeSamples);
    }

    void ScatterBufferToDelayLine()
    {
        double timeLowSamples = m_scatterBufferWarpedTime[(m_scatterHead - 1) % x_scatterBufferSize];
        double timeHighSamples = m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize];
        double realTimeLow = m_scatterBufferTime[(m_scatterHead - 1) % x_scatterBufferSize];
        double realTimeHigh = m_scatterBufferTime[m_scatterHead % x_scatterBufferSize];

        int64_t timeLowIndex = static_cast<int64_t>(std::ceil(timeLowSamples));
        int64_t timeHighIndex = static_cast<int64_t>(std::ceil(timeHighSamples));

        for (int64_t i = timeLowIndex; i < timeHighIndex; ++i)
        {
            double alpha = (static_cast<double>(i) - timeLowSamples) / (timeHighSamples - timeLowSamples);
            m_writeHeadInverse[WrapIndex(i)] = realTimeLow + alpha * (realTimeHigh - realTimeLow);
        }
    }

    static size_t WrapIndex(int64_t index)
    {
        return static_cast<size_t>(PhaseUtils::FloorMod(index, static_cast<int64_t>(x_maxDelaySamples)));
    }

    void Write(const Vector& x, double time)
    {
        if (time < m_lastWarpedTime)
        {
            m_descending = true;
            m_ascendingCount = 0;
        }
        else if (m_descending)
        {
            // Include the turnaround sample in the first forward interval.
            // Descending intervals still leave inverse history untouched.
            //
            m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize] = m_lastWarpedTime;
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = m_lastTime - 1.0;
            ++m_scatterHead;
            m_ascendingCount = 1;
            m_turnaroundWarpedTime = time;
            m_descending = false;
        }

        m_lastWarpedTime = time;
        if (!m_descending)
        {
            ++m_ascendingCount;
            m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize] = time;
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = m_lastTime;
            if (2 <= m_ascendingCount)
            {
                ScatterBufferToDelayLine();
            }

            ++m_scatterHead;
        }

        size_t index = WrapIndex(static_cast<int64_t>(m_lastTime));
        m_delayLine[index] = x;

        size_t envelopeBucketIndex = index / x_envelopeBucketSamples;
        if (index % x_envelopeBucketSamples == 0)
        {
            m_maxEnvelope[envelopeBucketIndex] = x;
            m_minEnvelope[envelopeBucketIndex] = x;
        }
        else
        {
            m_maxEnvelope[envelopeBucketIndex] = Vector::Max(m_maxEnvelope[envelopeBucketIndex], x);
            m_minEnvelope[envelopeBucketIndex] = Vector::Min(m_minEnvelope[envelopeBucketIndex], x);
        }

        if (index == 0)
        {
            INFO("Delay buffer wrap around");            
        }

        m_lastTime += 1.0;
    }

    Vector ReadAtIndex(double index)
    {
        size_t i0 = WrapIndex(static_cast<int64_t>(std::floor(index)));
        size_t iSub1 = (i0 + x_maxDelaySamples - 1) % x_maxDelaySamples;
        size_t i1 = (i0 + 1) % x_maxDelaySamples;
        size_t i2 = (i0 + 2) % x_maxDelaySamples;

        const Vector& ySub1 = m_delayLine[iSub1];
        const Vector& y0 = m_delayLine[i0];
        const Vector& y1 = m_delayLine[i1];
        const Vector& y2 = m_delayLine[i2];

        Vector c = y1 - ySub1;
        Vector d = ySub1 * 2 - y0 * 5 + y1 * 4 - y2;
        Vector e = -ySub1 + (y0 - y1) * 3 + y2;
        Number alpha = static_cast<Number>(index - std::floor(index));
        
        return y0 + (c + (d + e * alpha) * alpha) * alpha / 2;
    }

    Vector Read(double warpedTime)
    {
        return ReadAtIndex(GetRealTime(warpedTime));
    }

    Vector ReadWithOffset(double warpedTime, double offset)
    {
        return ReadAtIndex(GetRealTime(warpedTime) + offset);
    }

    double GetRealTime(double warpedTime)
    {
        double floorTime = std::floor(warpedTime);
        size_t i0 = WrapIndex(static_cast<int64_t>(floorTime));
        size_t i1 = (i0 + 1) % x_maxDelaySamples;
        double alpha = warpedTime - floorTime;
        return m_writeHeadInverse[i0] + alpha * (m_writeHeadInverse[i1] - m_writeHeadInverse[i0]);
    }

    // Each channel mapped from its own warped time.
    //
    MultiChannelNumber<double, Size> GetRealTime(const MultiChannelNumber<double, Size>& warpedTime)
    {
        MultiChannelNumber<double, Size> result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = GetRealTime(warpedTime[i]);
        }

        return result;
    }

    std::pair<Vector, Vector> GetEnvelopeAtRealTime(double realTime)
    {
        double envelopeIndexFloat = realTime / static_cast<double>(x_envelopeBucketSamples) - 0.5;

        int64_t bucketIndex = static_cast<int64_t>(std::floor(envelopeIndexFloat));
        size_t index0 = static_cast<size_t>(PhaseUtils::FloorMod(bucketIndex, static_cast<int64_t>(x_envelopeBucketCount)));
        size_t index1 = (index0 + 1) % x_envelopeBucketCount;
        Number frac = static_cast<Number>(envelopeIndexFloat - std::floor(envelopeIndexFloat));

        Vector maxVal = m_maxEnvelope[index0] * (1 - frac) + m_maxEnvelope[index1] * frac;
        Vector minVal = m_minEnvelope[index0] * (1 - frac) + m_minEnvelope[index1] * frac;

        return {maxVal, minVal};
    }

    std::pair<Vector, Vector> GetEnvelopeAtWarpedTime(double warpedTime)
    {
        return GetEnvelopeAtRealTime(GetRealTime(warpedTime));
    }

    Vector ReadRealTime(double realTime)
    {
        return ReadAtIndex(realTime);
    }

    // Each channel read at its own real time.
    //
    Vector ReadRealTime(const MultiChannelNumber<double, Size>& realTime)
    {
        Vector result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = ReadAtIndex(realTime[i])[i];
        }

        return result;
    }
};

template <size_t MaxDelaySamples>
using DelayLineMovableWriter = DelayLineMovableWriterGeneric<float, 1, MaxDelaySamples>;

template <size_t MaxDelaySamples>
using QuadDelayLineMovableWriter = DelayLineMovableWriterGeneric<float, 4, MaxDelaySamples>;

// Launches resynthesized grains from an audio buffer on a shared schedule. Each
// grain covers all Size channels, which start and end together but read from
// their own times and resynthesize independently.
//
// The audio buffer holds host-rate samples and provides GetRealTime and
// ReadRealTime, both taking MultiChannelNumber<double, Size> host-sample times.
// Analysis reads host-rate windows every analysis hop; synthesized frames
// overlap-add at the engine rate, launching every synthesis hop.
//
template <typename AudioBufferType, size_t Size, size_t OLALatency = 0>
struct GrainManagerGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using TimeVector = MultiChannelNumber<double, Size>;
    using Resynth = ResynthesizerGeneric<float, Size>;
    using SynthesisOLA = OLAGeneric<float, Size, Resynth::x_synthesisBits, OLALatency>;

    // Hann before analysis and after synthesis: four quarter-window shifts of
    // Hann squared sum to 1.5.
    //
    static constexpr float x_overlapGain = 1.5f;

    AudioBufferType* m_audioBuffer;
    size_t m_samplesToNextGrain;
    Resynth m_resynthesizer;
    typename Resynth::Buffer m_prevTable;
    typename Resynth::Buffer m_analysisTable;
    typename Resynth::SynthesisFrame m_synthesisFrame;
    SynthesisOLA m_ola;

    struct Input
    {
        typename Resynth::Input m_resynthInput[Size];
        Vector m_phaseModulation;
    };

    void ClearChannel(size_t channel)
    {
        m_ola.ClearChannel(channel);
        m_resynthesizer.ClearChannel(channel);
    }

    // One input per channel.
    //
    void StartGrain(typename Resynth::Input* input, const TimeVector& startTime)
    {
        for (size_t k = 0; k < Resynth::x_tableSize; ++k)
        {
            float window = Math4096::Hann(k);
            m_analysisTable.m_table[k] = m_audioBuffer->ReadRealTime(startTime + k) * window;
            m_prevTable.m_table[k] = m_audioBuffer->ReadRealTime(startTime + k - Resynth::x_H) * window;
        }

        m_resynthesizer.Process(m_prevTable, m_analysisTable, m_synthesisFrame, input);
        m_ola.Write(m_synthesisFrame);
    }

    // Recorded sample banks have no write frontier.
    //
    Vector Process(const TimeVector& warpedTime, const Vector& sampleOffset, Input& input)
    {
        return Process(warpedTime, sampleOffset, input, std::numeric_limits<double>::infinity());
    }

    Vector Process(const TimeVector& warpedTime, const Vector& sampleOffset, Input& input, double latestSampleTime)
    {
        if (!m_audioBuffer)
        {
            return Vector::Splat(0);
        }

        Vector result;
        if constexpr (OLALatency > 0)
        {
            result = m_ola.Process(input.m_phaseModulation) / x_overlapGain;
        }
        else
        {
            result = m_ola.Process() / x_overlapGain;
        }

        if (m_samplesToNextGrain <= 1)
        {
            // Protect the full analysis window after mapping to real time.
            // Cubic audio interpolation reads two samples beyond its floor index.
            //
            double latestStart = latestSampleTime - (Resynth::x_N - 1) - 2;
            TimeVector startTime = TimeVector::Min(
                m_audioBuffer->GetRealTime(warpedTime) + sampleOffset.template Convert<double>(),
                TimeVector::Splat(latestStart));

            StartGrain(input.m_resynthInput, startTime);
            m_samplesToNextGrain = Resynth::GetSynthesisHopSamples();
        }
        else
        {
            --m_samplesToNextGrain;
        }

        return result;
    }

    GrainManagerGeneric()
        : m_audioBuffer(nullptr)
        , m_samplesToNextGrain(0)
    {
    }

    explicit GrainManagerGeneric(AudioBufferType* audioBuffer)
        : GrainManagerGeneric()
    {
        m_audioBuffer = audioBuffer;
    }
};

template <typename AudioBufferType>
using GrainManager = GrainManagerGeneric<AudioBufferType, 1>;

template <typename AudioBufferType>
using QuadGrainManager = GrainManagerGeneric<AudioBufferType, 4, Resynthesizer::GetSynthesisHopSamples()>;
