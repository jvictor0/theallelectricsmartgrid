#pragma once

#include "AudioBuffer.hpp"
#include "DelayLine.hpp"
#include "Oversample.hpp"
#include "PhasorPlayHead.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "TheoryOfTime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

struct SampleSource
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

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

    GrainManager<AudioBufferBank> m_grainManager;

    bool m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockBaseOutput[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];
    Upsampler m_upsampler;
    PhasorPlayHead m_phasorPlayHead;
    double m_previousTotalPhaseTime;
    float m_sampleStart;
    float m_sampleLength;

    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        GrainManager<AudioBufferBank>::Input m_grainManagerInput;
        PhasorPlayHead::Input m_phasorPlayHeadInput;

        Input()
            : m_theoryOfTime(nullptr)
        {
        }
    };

    SampleSource()
        : m_upsampler(x_oversample)
        , m_previousTotalPhaseTime(0.0)
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
        m_grainManager.m_audioBuffer = audioBufferBank;
    }

    void ProcessUBlock(Input& input)
    {
        double previousTotalPhaseTime = m_previousTotalPhaseTime;
        input.m_phasorPlayHeadInput.m_theoryOfTime = input.m_theoryOfTime;
        for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
        {
            input.m_phasorPlayHeadInput.m_sampleIndex = static_cast<int>(i);
            double totalPhaseTime =
                static_cast<double>(m_phasorPlayHead.Process(input.m_phasorPlayHeadInput));
            m_uBlockBaseOutput[i] =
                m_grainManager.Process(totalPhaseTime, 0.0, input.m_grainManagerInput);
            m_uBlockTop[i] = std::floor(totalPhaseTime) != std::floor(previousTotalPhaseTime);
            previousTotalPhaseTime = totalPhaseTime;
        }

        m_previousTotalPhaseTime = previousTotalPhaseTime;
        m_upsampler.Process(m_uBlockBaseOutput, m_uBlockOutput);
    }

    void PopulateUIState(UIState* uiState) const
    {
        uiState->m_samplePosition.store(m_previousTotalPhaseTime, std::memory_order_relaxed);
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
