#pragma once

#include "PhaseUtils.hpp"
#include "Resynthesis.hpp"
#include "SampleTimer.hpp"
#include "TheoryOfTime.hpp"
#include <cmath>

// Delay tape positions in host samples. Update runs every engine sample.
//
struct TapeHead
{
    double m_relativePosition;
    double m_actualPosition;

    TapeHead()
        : m_relativePosition(0.0)
        , m_actualPosition(0.0)
    {
    }

    static double ComputeLoopSamples(TheoryOfTime* theoryOfTime, int loopSelector, double globalPeriodSamples)
    {
        if (!theoryOfTime)
        {
            return 0.0;
        }

        int64_t cycleRatio = theoryOfTime->GetCycleRatio(loopSelector);
        if (cycleRatio <= 0)
        {
            return 0.0;
        }

        return globalPeriodSamples / static_cast<double>(cycleRatio);
    }
};

struct WriteTapeHead : TapeHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        bool m_running;

        // Global loop period in host samples.
        //
        double m_globalPeriodSamples;

        Input()
            : m_theoryOfTime(nullptr)
            , m_running(false)
            , m_globalPeriodSamples(1.0)
        {
        }
    };

    double m_glue;
    double m_globalPeriodSamples;
    double m_displayPhaseOffset;
    bool m_running;

    WriteTapeHead()
        : m_glue(0.0)
        , m_globalPeriodSamples(1.0)
        , m_displayPhaseOffset(0.0)
        , m_running(false)
    {
    }

    void Update(Input& input)
    {
        if (!input.m_theoryOfTime)
        {
            return;
        }

        double theoryPhase = input.m_theoryOfTime->GetPhase(TheoryOfTimeBase::x_globalLoop, PhaseDomain::Modulated);
        double theoryPosition = theoryPhase * input.m_globalPeriodSamples;

        bool runningChanged = input.m_running != m_running;
        bool globalPeriodSamplesChanged = input.m_globalPeriodSamples != m_globalPeriodSamples;
        if (runningChanged || globalPeriodSamplesChanged)
        {
            m_glue = m_actualPosition - theoryPosition;
        }

        if (runningChanged && input.m_running)
        {
            // Playback rejoins musical phase on start. Keep the display origin
            // continuous so the waveform recorder does not interpolate a reset.
            //
            m_displayPhaseOffset += m_relativePosition - theoryPhase;
        }

        m_running = input.m_running;
        m_globalPeriodSamples = input.m_globalPeriodSamples;

        if (!m_running)
        {
            const double hostStep = 1.0 / static_cast<double>(SampleTimer::x_oversample);
            m_actualPosition += hostStep;
            if (m_globalPeriodSamples > 0.0)
            {
                m_relativePosition += hostStep / m_globalPeriodSamples;
            }

            m_glue = m_actualPosition - theoryPosition;
        }
        else
        {
            m_relativePosition = theoryPhase;
            m_actualPosition = theoryPosition + m_glue;
        }
    }

    double GetDisplayPosition() const
    {
        return m_relativePosition + m_displayPhaseOffset;
    }
};

struct ReadTapeHead : TapeHead
{
    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        double m_bufferFraction;
        double m_readHeadSpeed;
        int m_requestedLoopSelector;

        Input()
            : m_theoryOfTime(nullptr)
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
            input.m_theoryOfTime->CrossedCycleBoundary(m_loopSelector, PhaseDomain::Modulated) &&
            input.m_theoryOfTime->CrossedCycleBoundary(input.m_requestedLoopSelector, PhaseDomain::Modulated);
        if (allowLoopSwitch)
        {
            m_loopSelector = input.m_requestedLoopSelector;
        }

        double globalPeriodSamples = m_writeTapeHead->m_globalPeriodSamples;
        double loopSamples = ComputeLoopSamples(input.m_theoryOfTime, m_loopSelector, globalPeriodSamples);
        double effectiveDelaySamples = loopSamples * input.m_bufferFraction;
        double hopSamples = static_cast<double>(Resynthesizer::GetGrainLaunchSamples());
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

        // Translate the displayed phase displacement into buffer coordinates.
        // The write head's continuity glue must not be scaled by read speed.
        //
        m_actualPosition = m_writeTapeHead->m_actualPosition +
            (m_relativePosition - m_writeTapeHead->m_relativePosition) * globalPeriodSamples;
    }
};
