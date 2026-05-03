#pragma once

#include "AudioBuffer.hpp"
#include "DelayLine.hpp"
#include "Math.hpp"
#include "Oversample.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "TheoryOfTime.hpp"

#include <algorithm>
#include <cmath>

struct DualSampleSource
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    GrainManager<AudioBufferBank> m_grainManager[2];

    bool m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockBaseOutput[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];
    Upsampler m_upsampler;
    double m_previousTotalPhaseTime;

    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        GrainManager<AudioBufferBank>::Input m_grainManagerInput[2];
        float m_mix;

        Input()
            : m_theoryOfTime(nullptr)
            , m_mix(0.5f)
        {
        }
    };

    DualSampleSource()
        : m_upsampler(x_oversample)
        , m_previousTotalPhaseTime(0.0)
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

    void SetAudioBufferBanks(AudioBufferBank* audioBufferBank0, AudioBufferBank* audioBufferBank1)
    {
        m_grainManager[0].m_audioBuffer = audioBufferBank0;
        m_grainManager[1].m_audioBuffer = audioBufferBank1;
    }

    double GetTotalPhaseTime(TheoryOfTime* theoryOfTime, size_t sampleIndex) const
    {
        if (!theoryOfTime)
        {
            return 0.0;
        }

        const TimeLoop& masterLoop = theoryOfTime->m_loops[TheoryOfTimeBase::x_masterLoop];
        return masterLoop.m_phasorIndependent[sampleIndex];
    }

    void ProcessUBlock(Input& input)
    {
        float mix = std::max(0.0f, std::min(1.0f, input.m_mix));
        float mixPhase = mix * 0.25f;
        float gain0 = Math::Cos2pi(mixPhase);
        float gain1 = Math::Sin2pi(mixPhase);

        double previousTotalPhaseTime = m_previousTotalPhaseTime;
        for (size_t i = 0; i < SampleTimer::x_controlFrameRate; ++i)
        {
            double totalPhaseTime = GetTotalPhaseTime(input.m_theoryOfTime, i);
            float sample0 = m_grainManager[0].Process(totalPhaseTime, 0.0, input.m_grainManagerInput[0]);
            float sample1 = m_grainManager[1].Process(totalPhaseTime, 0.0, input.m_grainManagerInput[1]);

            m_uBlockBaseOutput[i] = sample0 * gain0 + sample1 * gain1;
            m_uBlockTop[i] = std::floor(totalPhaseTime) != std::floor(previousTotalPhaseTime);
            previousTotalPhaseTime = totalPhaseTime;
        }

        m_previousTotalPhaseTime = previousTotalPhaseTime;
        m_upsampler.Process(m_uBlockBaseOutput, m_uBlockOutput);
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t voiceIx,
        float baseFreq)
    {
    }
};
