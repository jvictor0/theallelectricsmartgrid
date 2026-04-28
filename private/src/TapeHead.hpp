#pragma once

#include "PhaseUtils.hpp"
#include "Resynthesis.hpp"
#include "TheoryOfTime.hpp"
#include <cmath>

struct TapeHead
{
    double m_relativePosition;
    double m_actualPosition;

    TapeHead()
        : m_relativePosition(0.0)
        , m_actualPosition(0.0)
    {
    }

    double ComputeLoopSamples(TheoryOfTime* theoryOfTime, int sampleIndex, int loopSelector)
    {
        if (!theoryOfTime)
        {
            return 0.0;
        }

        size_t sampleIdx = static_cast<size_t>(sampleIndex) % TheoryOfTimeBase::x_microBlockSize;
        int externalLoopMult = theoryOfTime->GetLoopExternalMultiplier(sampleIdx, loopSelector);
        if (externalLoopMult <= 0)
        {
            return 0.0;
        }

        return theoryOfTime->m_masterLoopSamples / static_cast<double>(externalLoopMult);
    }
};

struct WriteTapeHead : TapeHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        int m_sampleIndex;
        bool m_running;
        double m_masterLoopSamples;
        int m_requestedLoopSelector;

        Input()
            : m_theoryOfTime(nullptr)
            , m_sampleIndex(0)
            , m_running(false)
            , m_masterLoopSamples(1.0)
            , m_requestedLoopSelector(TheoryOfTime::x_masterLoop)
        {
        }
    };

    double m_glue;
    double m_masterLoopSamples;
    int m_loopSelector;
    bool m_running;

    WriteTapeHead()
        : m_glue(0.0)
        , m_masterLoopSamples(1.0)
        , m_loopSelector(TheoryOfTime::x_masterLoop)
        , m_running(false)
    {
    }

    void Update(Input& input)
    {
        if (!input.m_theoryOfTime)
        {
            return;
        }

        if (!input.m_running)
        {
            if (m_running)
            {
                m_running = false;
                m_glue = m_actualPosition;
            }

            m_glue = m_glue + 1.0;
        }
        else if (!m_running)
        {
            m_running = true;
        }

        if (m_masterLoopSamples != input.m_masterLoopSamples)
        {
            m_glue = m_actualPosition - (m_actualPosition - m_glue) * input.m_masterLoopSamples / m_masterLoopSamples;
            m_masterLoopSamples = input.m_masterLoopSamples;
        }

        bool allowLoopSwitch = input.m_requestedLoopSelector != m_loopSelector &&
            input.m_theoryOfTime->GetIndirectTop(input.m_sampleIndex, m_loopSelector) &&
            input.m_theoryOfTime->GetIndirectTop(input.m_sampleIndex, input.m_requestedLoopSelector);
        if (allowLoopSwitch)
        {
            m_loopSelector = input.m_requestedLoopSelector;
        }

        size_t sampleIdx = static_cast<size_t>(input.m_sampleIndex) % TheoryOfTimeBase::x_microBlockSize;
        m_relativePosition = input.m_theoryOfTime->GetIndirectPhasor(sampleIdx);
        m_actualPosition = input.m_theoryOfTime->m_globalPhase.UnWind() * input.m_masterLoopSamples + m_glue;
    }
};

struct ReadTapeHead : TapeHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        int m_sampleIndex;
        double m_bufferFraction;
        double m_readHeadSpeed;

        Input()
            : m_theoryOfTime(nullptr)
            , m_sampleIndex(0)
            , m_bufferFraction(0.0)
            , m_readHeadSpeed(1.0)
        {
        }
    };

    WriteTapeHead* m_writeTapeHead;

    ReadTapeHead()
        : m_writeTapeHead(nullptr)
    {
    }

    void Update(Input& input)
    {
        if (!input.m_theoryOfTime || !m_writeTapeHead)
        {
            return;
        }

        int loopSelector = m_writeTapeHead->m_loopSelector;
        double loopSamples = ComputeLoopSamples(input.m_theoryOfTime, input.m_sampleIndex, loopSelector);
        double effectiveDelaySamples = loopSamples * input.m_bufferFraction;
        double hopSamples = static_cast<double>(Resynthesizer::GetGrainLaunchSamples());
        double masterLoopSamples = m_writeTapeHead->m_masterLoopSamples;
        if (masterLoopSamples <= 0.0 || loopSamples <= 0.0)
        {
            m_relativePosition = m_writeTapeHead->m_relativePosition;
            m_actualPosition = m_writeTapeHead->m_actualPosition;
            return;
        }

        double actualWrapTop = m_writeTapeHead->m_actualPosition - hopSamples;
        double actualWrapBottom = actualWrapTop - loopSamples;
        double actualProjectedPosition =
            m_writeTapeHead->m_actualPosition * input.m_readHeadSpeed - effectiveDelaySamples;
        m_actualPosition = PhaseUtils::WrapMod(actualWrapBottom, actualWrapTop, actualProjectedPosition);

        double actualDelaySamples = m_writeTapeHead->m_actualPosition - m_actualPosition;
        m_relativePosition = m_writeTapeHead->m_relativePosition - actualDelaySamples / masterLoopSamples;
        m_relativePosition = m_relativePosition - std::floor(m_relativePosition);
    }
};
