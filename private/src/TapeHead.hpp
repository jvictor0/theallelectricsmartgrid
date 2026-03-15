#pragma once

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

        return theoryOfTime->LoopSamples(sampleIndex, loopSelector);
    }

    double ComputeUnwoundPhasor(TheoryOfTime* theoryOfTime, int sampleIndex, int loopSelector)
    {
        double loopSamples = ComputeLoopSamples(theoryOfTime, sampleIndex, loopSelector);
        if (loopSamples == 0.0 || !std::isfinite(loopSamples))
        {
            return 0.0;
        }

        return theoryOfTime->PhasorUnwoundSamples(sampleIndex, loopSelector) / loopSamples;
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
            int oldLoopSelector = m_loopSelector;
            m_loopSelector = input.m_requestedLoopSelector;
            m_glue += input.m_theoryOfTime->PhasorUnwoundSamples(input.m_sampleIndex, oldLoopSelector)
                - input.m_theoryOfTime->PhasorUnwoundSamples(input.m_sampleIndex, m_loopSelector);
        }

        size_t sampleIdx = static_cast<size_t>(input.m_sampleIndex) % TheoryOfTimeBase::x_microBlockSize;
        m_relativePosition = input.m_theoryOfTime->GetIndirectPhasor(sampleIdx);
        m_actualPosition = input.m_theoryOfTime->PhasorUnwoundSamples(input.m_sampleIndex, m_loopSelector) + m_glue;
    }
};

struct ReadTapeHead : TapeHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        int m_sampleIndex;
        double m_bufferFraction;

        Input()
            : m_theoryOfTime(nullptr)
            , m_sampleIndex(0)
            , m_bufferFraction(0.0)
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
        double effectiveDelayPhasor = (loopSamples * input.m_bufferFraction) / m_writeTapeHead->m_masterLoopSamples;
        m_relativePosition = m_writeTapeHead->m_relativePosition - effectiveDelayPhasor;
        m_relativePosition = m_relativePosition - std::floor(m_relativePosition);

        double effectiveDelaySamples = loopSamples * input.m_bufferFraction;
        m_actualPosition = m_writeTapeHead->m_actualPosition - effectiveDelaySamples;
    }
};
