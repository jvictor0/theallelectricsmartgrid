#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

#include "MultiChannelNumber.hpp"
#include "SampleTimer.hpp"
#include "TheoryOfTimeBase.hpp"

struct ASD
{
    struct ASDControl
    {
        bool m_gate = false;
    };

    struct Input
    {
        TheoryOfTimeBase* m_theoryOfTime = nullptr;
        double m_phaseRatio = 1.0;
        double m_envelopePeriodSamples = SampleTimer::SecondsToSamples(1.0);
        float m_attackIncrement = 0.0f;
        float m_decayIncrement = 0.0f;
        bool m_gate = false;

        void Set(const ASDControl& control)
        {
            m_gate = control.m_gate;
        }
    };

    double m_rawOutput = 0.0;
    float m_output = 0.0f;

    float Process(const Input& input)
    {
        assert(input.m_theoryOfTime != nullptr);
        const TheoryOfTimeBase& time = *input.m_theoryOfTime;
        double deltaPhase = time.m_sample.m_running
            ? time.GetModulatedGlobalPhaseIncrement()
            : time.GetUnmodulatedGlobalPhaseIncrement();
        double deltaSamples = std::abs(deltaPhase * input.m_phaseRatio * input.m_envelopePeriodSamples);
        if (input.m_gate)
        {
            m_rawOutput = std::min(1.0, m_rawOutput + deltaSamples * input.m_attackIncrement);
        }
        else
        {
            m_rawOutput = std::max(0.0, m_rawOutput - deltaSamples * input.m_decayIncrement);
        }

        m_output = static_cast<float>(m_rawOutput);
        return m_output;
    }
};

template <size_t Size>
struct ASDGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    struct Input
    {
        ASD::Input m_channels[Size];

        void Set(size_t channel, const ASD::ASDControl& control)
        {
            m_channels[channel].Set(control);
        }
    };

    ASD m_channels[Size];
    Vector m_output;

    Vector Process(const Input& input)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_output[i] = m_channels[i].Process(input.m_channels[i]);
        }

        return m_output;
    }
};
