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
#include "TimeWarpInverter.hpp"
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
// write head, so the inverter's warped-to-real time map is scalar while audio and
// envelopes are interleaved per channel.
//
// Audio is stored once per host sample. The inverter is owned elsewhere and
// written every engine sample, Oversample per host sample, in engine-sample
// warped and real time. It must record each engine sample before the delay
// line is written or read during that sample.
//
template <typename Number, size_t Size, size_t MaxDelaySamples, size_t Oversample>
struct DelayLineMovableWriterGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    using Inverter = TimeWarpInverter<MaxDelaySamples * Oversample>;
    static constexpr size_t x_maxDelaySamples = MaxDelaySamples;
    static constexpr size_t x_oversample = Oversample;

    Vector m_delayLine[x_maxDelaySamples];
    Inverter* m_inverter;
    double m_firstSampleTime;
    double m_latestSampleTime;

    DelayLineMovableWriterGeneric()
        : m_inverter(nullptr)
        , m_firstSampleTime(std::numeric_limits<double>::infinity())
        , m_latestSampleTime(-std::numeric_limits<double>::infinity())
    {
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_delayLine[i] = Vector::Splat(0);
        }


    }

    static size_t WrapIndex(int64_t index)
    {
        return static_cast<size_t>(PhaseUtils::FloorMod(index, static_cast<int64_t>(x_maxDelaySamples)));
    }

    // The engine clock numbers samples from one. Host sample k is completed at
    // engine sample k * Oversample.
    //
    template <typename Time>
    static Time ToHostTime(const Time& engineTime)
    {
        return engineTime / static_cast<double>(Oversample);
    }

    // Bounds of audio actually written, independent of inverse-map updates.
    //
    double GetLatestSampleTime() const
    {
        return m_latestSampleTime;
    }

    double GetOldestSampleTime() const
    {
        return std::max(m_firstSampleTime, m_latestSampleTime - static_cast<double>(x_maxDelaySamples - 1));
    }

    void Write(const Vector& x, size_t hostSample)
    {
        assert(!std::isfinite(m_latestSampleTime) || static_cast<double>(hostSample) == m_latestSampleTime + 1.0);
        m_firstSampleTime = std::min(m_firstSampleTime, static_cast<double>(hostSample));
        m_latestSampleTime = static_cast<double>(hostSample);
        size_t index = WrapIndex(static_cast<int64_t>(hostSample));
        m_delayLine[index] = x;

        if (index == 0)
        {
            INFO("Delay buffer wrap around");            
        }
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
        double realTime = GetRealTime(warpedTime);
        return std::isfinite(realTime) ? ReadAtIndex(realTime) : Vector::Splat(0);
    }

    Vector ReadWithOffset(double warpedTime, double offset)
    {
        double realTime = GetRealTime(warpedTime) + offset;
        return std::isfinite(realTime) ? ReadAtIndex(realTime) : Vector::Splat(0);
    }

    // Warped and real times here are in host samples.
    //
    double GetRealTime(double warpedTime)
    {
        assert(m_inverter);
        return ToHostTime(m_inverter->GetRealTime(warpedTime * static_cast<double>(Oversample)));
    }

    MultiChannelNumber<double, Size> GetRealTime(const MultiChannelNumber<double, Size>& warpedTime)
    {
        assert(m_inverter);
        return ToHostTime(m_inverter->GetRealTime(warpedTime * static_cast<double>(Oversample)));
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
using DelayLineMovableWriter = DelayLineMovableWriterGeneric<float, 1, MaxDelaySamples, SampleTimer::x_oversample>;

template <size_t MaxDelaySamples>
using QuadDelayLineMovableWriter = DelayLineMovableWriterGeneric<float, 4, MaxDelaySamples, SampleTimer::x_oversample>;

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
        TimeVector readableTime = startTime;
        bool anyAvailable = false;
        for (size_t channel = 0; channel < Size; ++channel)
        {
            if (std::isfinite(startTime[channel]))
            {
                anyAvailable = true;
            }
            else
            {
                readableTime[channel] = 0.0;
                m_resynthesizer.ClearChannel(channel);
            }
        }

        if (!anyAvailable)
        {
            return;
        }

        for (size_t k = 0; k < Resynth::x_tableSize; ++k)
        {
            float window = Math4096::Hann(k);
            m_analysisTable.m_table[k] = m_audioBuffer->ReadRealTime(readableTime + k) * window;
            m_prevTable.m_table[k] = m_audioBuffer->ReadRealTime(readableTime + k - Resynth::x_H) * window;
        }

        // Missing lanes contribute no new grain; previously scheduled OLA tails
        // continue normally. Never pass unavailable timestamps to audio indexing.
        //
        for (size_t channel = 0; channel < Size; ++channel)
        {
            if (!std::isfinite(startTime[channel]))
            {
                for (size_t k = 0; k < Resynth::x_tableSize; ++k)
                {
                    m_analysisTable.m_table[k][channel] = 0;
                    m_prevTable.m_table[k][channel] = 0;
                }
            }
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
        return Process(warpedTime, sampleOffset, input, -std::numeric_limits<double>::infinity(), latestSampleTime);
    }

    Vector Process(const TimeVector& warpedTime, const Vector& sampleOffset, Input& input,
        double oldestSampleTime, double latestSampleTime)
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
            double earliestStart = oldestSampleTime + Resynth::x_H + 1;
            TimeVector startTime = m_audioBuffer->GetRealTime(warpedTime) + sampleOffset.template Convert<double>();
            for (size_t channel = 0; channel < Size; ++channel)
            {
                if (std::isfinite(startTime[channel]))
                {
                    startTime[channel] = std::min(startTime[channel], latestStart);
                    if (startTime[channel] < earliestStart)
                    {
                        startTime[channel] = std::numeric_limits<double>::quiet_NaN();
                    }
                }
            }

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
