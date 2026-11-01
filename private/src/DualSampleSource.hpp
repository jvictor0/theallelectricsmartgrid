#pragma once

#include "AudioBuffer.hpp"
#include "DelayLine.hpp"
#include "MultiChannelNumber.hpp"
#include "Oversample.hpp"
#include "PhasorPlayHead.hpp"
#include "SampleTimer.hpp"
#include "SampleTop.hpp"
#include "SmartGridOneEncoders.hpp"
#include "TheoryOfTime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

// One sample bank per channel behind the grain manager's buffer interface.
// A missing bank reads as silence.
//
template <size_t Size>
struct AudioBufferBankChannels
{
    using Vector = MultiChannelNumber<float, Size>;
    using TimeVector = MultiChannelNumber<double, Size>;

    AudioBufferBank* m_banks[Size];

    AudioBufferBankChannels()
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_banks[i] = nullptr;
        }
    }

    bool IsEmpty() const
    {
        for (size_t i = 0; i < Size; ++i)
        {
            if (m_banks[i])
            {
                return false;
            }
        }

        return true;
    }

    TimeVector GetRealTime(const TimeVector& warpedTime) const
    {
        TimeVector result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = m_banks[i] ? m_banks[i]->GetRealTime(warpedTime[i]) : 0.0;
        }

        return result;
    }

    Vector ReadRealTime(const TimeVector& realTime) const
    {
        Vector result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = m_banks[i] ? m_banks[i]->ReadRealTime(realTime[i]) : 0.0f;
        }

        return result;
    }
};

// Resynthesized sample playback for every channel at the base rate. Grains
// launch on one schedule for all channels, and the resynthesizer's FFTs run
// across channels.
//
template <size_t Size>
struct SampleSourceGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using TimeVector = MultiChannelNumber<double, Size>;
    using Banks = AudioBufferBankChannels<Size>;
    using GrainManagerType = GrainManagerGeneric<Banks, Size>;
    using PlayHead = PhasorPlayHeadGeneric<Size>;

    Banks m_audioBufferBanks;
    GrainManagerType m_grainManager;
    PlayHead m_phasorPlayHead;
    TimeVector m_previousTotalPhaseTime;
    SampleTop m_top[Size];

    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        typename GrainManagerType::Input m_grainManagerInput;
        typename PlayHead::Input m_phasorPlayHeadInput;

        Input()
            : m_theoryOfTime(nullptr)
        {
        }
    };

    SampleSourceGeneric()
        : m_previousTotalPhaseTime(TimeVector::Splat(0))
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
        }
    }

    void SetAudioBufferBank(size_t channel, AudioBufferBank* audioBufferBank)
    {
        m_audioBufferBanks.m_banks[channel] = audioBufferBank;
        m_grainManager.m_audioBuffer = m_audioBufferBanks.IsEmpty() ? nullptr : &m_audioBufferBanks;
    }

    // One base-rate sample at sampleIndex within the microblock.
    //
    Vector Process(Input& input, int sampleIndex)
    {
        input.m_phasorPlayHeadInput.m_theoryOfTime = input.m_theoryOfTime;
        input.m_phasorPlayHeadInput.m_sampleIndex = sampleIndex;
        TimeVector totalPhaseTime = m_phasorPlayHead.Process(input.m_phasorPlayHeadInput).template Convert<double>();
        Vector output = m_grainManager.Process(totalPhaseTime, Vector::Splat(0), input.m_grainManagerInput);
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = SampleTop::FromPhases(m_previousTotalPhaseTime[i], totalPhaseTime[i]);
        }

        m_previousTotalPhaseTime = totalPhaseTime;
        return output;
    }
};

using TrioSampleSource = SampleSourceGeneric<3>;

// Mono micro-block shell: runs the core at the base rate and upsamples.
//
struct SampleSource
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    using Core = SampleSourceGeneric<1>;
    using Input = Core::Input;

    struct UIState
    {
        std::atomic<double> m_samplePosition;
        std::atomic<float> m_start;
        std::atomic<float> m_length;

        UIState()
            : m_samplePosition(0.0)
            , m_start(0.0f)
            , m_length(1.0f)
        {
        }
    };

    Core m_core;

    SampleTop m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockBaseOutput[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];
    Upsampler m_upsampler;
    float m_sampleStart;
    float m_sampleLength;

    SampleSource()
        : m_upsampler(x_oversample)
        , m_sampleStart(0.0f)
        , m_sampleLength(1.0f)
    {
        for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
        {
            m_uBlockTop[i] = false;
            m_uBlockBaseOutput[i] = 0.0f;
        }

        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            m_uBlockOutput[i] = 0.0f;
        }
    }

    void SetAudioBufferBank(AudioBufferBank* audioBufferBank)
    {
        m_core.SetAudioBufferBank(0, audioBufferBank);
    }

    void ProcessUBlock(Input& input)
    {
        for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
        {
            m_uBlockBaseOutput[i] = m_core.Process(input, static_cast<int>(i));
            m_uBlockTop[i] = m_core.m_top[0];
        }

        m_upsampler.Process(m_uBlockBaseOutput, m_uBlockOutput);
    }

    void PopulateUIState(UIState* uiState) const
    {
        uiState->m_samplePosition.store(m_core.m_previousTotalPhaseTime[0], std::memory_order_relaxed);
        uiState->m_start.store(m_sampleStart, std::memory_order_relaxed);
        uiState->m_length.store(m_sampleLength, std::memory_order_relaxed);
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t voiceIx,
        float)
    {
        using Param = SmartGridOneEncoders::Param;

        int voice = static_cast<int>(voiceIx);

        int switchVal = encoders.GetSwitchVal(Param::SampleLoopIndex, voice);
        input.m_phasorPlayHeadInput.m_loopIndex = TheoryOfTimeBase::x_numLoops - switchVal - 1;

        static constexpr float x_readHeadSpeeds[17] =
        {
            -4.0f,
            -3.0f,
            -2.0f,
            -3.0f / 2.0f,
            -4.0f / 3.0f,
            -1.0f,
            -1.0f / 2.0f,
            -1.0f / 4.0f,
            0,
            1.0f / 4.0f,
            1.0f / 2.0f,
            1.0f,
            4.0f / 3.0f,
            3.0f / 2.0f,
            2.0f,
            3.0f,
            4.0f
        };

        int speedIx = encoders.GetSwitchVal(Param::SampleReadSpeed, voice);
        float speed = x_readHeadSpeeds[speedIx];

        float startVal = encoders.GetValue(Param::SampleStart, voice);
        float lengthVal = encoders.GetValue(Param::SampleLength, voice);
        lengthVal = std::max(lengthVal, 1.0e-6f);
        input.m_phasorPlayHeadInput.m_start = startVal;
        input.m_phasorPlayHeadInput.m_length = lengthVal;
        input.m_phasorPlayHeadInput.m_speed = speed;
        m_sampleStart = startVal;
        m_sampleLength = lengthVal;
    }
};
