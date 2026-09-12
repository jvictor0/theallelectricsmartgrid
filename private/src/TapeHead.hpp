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
        int64_t cycleRatio = theoryOfTime->GetCycleRatio(loopSelector, sampleIdx);
        if (cycleRatio <= 0)
        {
            return 0.0;
        }

        return theoryOfTime->m_globalPeriodSamples / static_cast<double>(cycleRatio);
    }
};

struct WriteTapeHead : TapeHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        int m_sampleIndex;
        bool m_running;
        double m_globalPeriodSamples;

        Input()
            : m_theoryOfTime(nullptr)
            , m_sampleIndex(0)
            , m_running(false)
            , m_globalPeriodSamples(1.0)
        {
        }
    };

    double m_glue;
    double m_globalPeriodSamples;
    bool m_running;

    WriteTapeHead()
        : m_glue(0.0)
        , m_globalPeriodSamples(1.0)
        , m_running(false)
    {
    }

    void Update(Input& input)
    {
        if (!input.m_theoryOfTime)
        {
            return;
        }

        size_t sampleIdx = static_cast<size_t>(input.m_sampleIndex) % TheoryOfTimeBase::x_microBlockSize;
        double theoryPosition = input.m_theoryOfTime->GetPhase(TheoryOfTimeBase::x_globalLoop, sampleIdx, PhaseDomain::Modulated) * input.m_globalPeriodSamples;

        bool runningChanged = input.m_running != m_running;
        bool globalPeriodSamplesChanged = input.m_globalPeriodSamples != m_globalPeriodSamples;
        if (runningChanged || globalPeriodSamplesChanged)
        {
            m_glue = m_actualPosition - theoryPosition;
        }

        m_running = input.m_running;
        m_globalPeriodSamples = input.m_globalPeriodSamples;

        m_relativePosition = input.m_theoryOfTime->GetPhase(TheoryOfTimeBase::x_globalLoop, sampleIdx, PhaseDomain::Modulated);
        if (!m_running)
        {
            m_actualPosition = m_actualPosition + 1.0;
            m_glue = m_actualPosition - theoryPosition;
        }
        else
        {
            m_actualPosition = theoryPosition + m_glue;
        }
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
        int m_requestedLoopSelector;

        Input()
            : m_theoryOfTime(nullptr)
            , m_sampleIndex(0)
            , m_bufferFraction(0.0)
            , m_readHeadSpeed(1.0)
            , m_requestedLoopSelector(TheoryOfTimeBase::x_globalLoop)
        {
        }
    };

    WriteTapeHead* m_writeTapeHead;
    int m_loopSelector;

    ReadTapeHead()
        : m_writeTapeHead(nullptr)
        , m_loopSelector(TheoryOfTimeBase::x_globalLoop)
    {
    }

    void Update(Input& input)
    {
        if (!input.m_theoryOfTime || !m_writeTapeHead)
        {
            return;
        }

        bool allowLoopSwitch = input.m_requestedLoopSelector != m_loopSelector &&
            input.m_theoryOfTime->CrossedCycleBoundary(m_loopSelector, input.m_sampleIndex, PhaseDomain::Modulated) &&
            input.m_theoryOfTime->CrossedCycleBoundary(input.m_requestedLoopSelector, input.m_sampleIndex, PhaseDomain::Modulated);
        if (allowLoopSwitch)
        {
            m_loopSelector = input.m_requestedLoopSelector;
        }

        double loopSamples = ComputeLoopSamples(input.m_theoryOfTime, input.m_sampleIndex, m_loopSelector);
        double effectiveDelaySamples = loopSamples * input.m_bufferFraction;
        double hopSamples = static_cast<double>(Resynthesizer::GetGrainLaunchSamples());
        double globalPeriodSamples = m_writeTapeHead->m_globalPeriodSamples;
        if (globalPeriodSamples <= 0.0 || loopSamples <= 0.0)
        {
            m_relativePosition = m_writeTapeHead->m_relativePosition;
            m_actualPosition = m_writeTapeHead->m_actualPosition;
            return;
        }

        double loopPhasor = loopSamples / globalPeriodSamples;
        double effectiveDelayPhasor = effectiveDelaySamples / globalPeriodSamples;
        double hopPhasor = hopSamples / globalPeriodSamples;

        double relativeWrapTop = m_writeTapeHead->m_relativePosition - hopPhasor;
        double relativeWrapBottom = relativeWrapTop - loopPhasor;
        double relativeProjectedPosition =
            m_writeTapeHead->m_relativePosition * input.m_readHeadSpeed - effectiveDelayPhasor;
        m_relativePosition = PhaseUtils::WrapMod(relativeWrapBottom, relativeWrapTop, relativeProjectedPosition);

        double actualWrapTop = m_writeTapeHead->m_actualPosition - hopSamples;
        double actualWrapBottom = actualWrapTop - loopSamples;
        double actualProjectedPosition =
            m_writeTapeHead->m_actualPosition * input.m_readHeadSpeed - effectiveDelaySamples;
        m_actualPosition = PhaseUtils::WrapMod(actualWrapBottom, actualWrapTop, actualProjectedPosition);
    }
};
