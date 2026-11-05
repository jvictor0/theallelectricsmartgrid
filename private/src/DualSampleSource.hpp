#pragma once

#include "AudioBuffer.hpp"
#include "DelayLine.hpp"
#include "MultiChannelNumber.hpp"
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

// Width-independent parts of the sample source: UI state.
//
struct SampleSourceBase
{
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
};

// Resynthesized sample playback for every channel, one sample per call.
// Grains launch on one schedule for all channels, and the resynthesizer's
// FFTs run across channels.
//
template <size_t Size>
struct SampleSourceGeneric : SampleSourceBase
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
    Vector m_sampleStart;
    Vector m_sampleLength;

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
        , m_sampleStart(Vector::Splat(0))
        , m_sampleLength(Vector::Splat(1))
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
        }
    }

    void SetAudioBufferBank(size_t channel, AudioBufferBank* audioBufferBank)
    {
        if (!audioBufferBank && m_audioBufferBanks.m_banks[channel])
        {
            m_grainManager.ClearChannel(channel);
        }

        m_audioBufferBanks.m_banks[channel] = audioBufferBank;
        m_grainManager.m_audioBuffer = m_audioBufferBanks.IsEmpty() ? nullptr : &m_audioBufferBanks;
    }

    Vector Process(Input& input)
    {
        input.m_phasorPlayHeadInput.m_theoryOfTime = input.m_theoryOfTime;
        TimeVector totalPhaseTime = m_phasorPlayHead.Process(input.m_phasorPlayHeadInput).template Convert<double>();
        Vector output = m_grainManager.Process(totalPhaseTime, Vector::Splat(0), input.m_grainManagerInput);
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = SampleTop::FromPhases(m_previousTotalPhaseTime[i], totalPhaseTime[i]);
        }

        m_previousTotalPhaseTime = totalPhaseTime;
        return output;
    }

    void PopulateUIState(UIState* uiState, size_t channel) const
    {
        uiState->m_samplePosition.store(m_previousTotalPhaseTime[channel], std::memory_order_relaxed);
        uiState->m_start.store(m_sampleStart[channel], std::memory_order_relaxed);
        uiState->m_length.store(m_sampleLength[channel], std::memory_order_relaxed);
    }

    void PopulateUIState(UIState* uiState) const
    {
        static_assert(Size == 1, "multi-channel sources must name the channel to publish");
        PopulateUIState(uiState, 0);
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track)
    {
        using Param = SmartGridOneEncoders::Param;

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

        for (size_t i = 0; i < Size; ++i)
        {
            int voice = static_cast<int>(track * Size + i);
            int switchVal = encoders.GetSwitchVal(Param::SampleLoopIndex, voice);
            input.m_phasorPlayHeadInput.m_loopIndex[i] = TheoryOfTimeBase::x_numLoops - switchVal - 1;

            int speedIx = encoders.GetSwitchVal(Param::SampleReadSpeed, voice);
            input.m_phasorPlayHeadInput.m_speed[i] = x_readHeadSpeeds[speedIx];
        }

        m_sampleStart = encoders.GetValue<Size>(Param::SampleStart, track);
        m_sampleLength = Vector::Max(encoders.GetValue<Size>(Param::SampleLength, track), Vector::Splat(1.0e-6f));
        input.m_phasorPlayHeadInput.m_start = m_sampleStart;
        input.m_phasorPlayHeadInput.m_length = m_sampleLength;
    }
};

using SampleSource = SampleSourceGeneric<1>;
using TrioSampleSource = SampleSourceGeneric<3>;
