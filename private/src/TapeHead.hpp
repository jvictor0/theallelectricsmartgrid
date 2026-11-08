#pragma once

#include "PhaseUtils.hpp"
#include "Resynthesis.hpp"
#include "SampleTimer.hpp"
#include "TheoryOfTime.hpp"
#include "WriteTapeHead.hpp"
#include <cmath>

// Follows the theory of time's write tape head, which runs in engine samples.
// Read positions are in host samples, as the delay line stores them.
//
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

    int m_loopSelector;

    ReadTapeHead()
        : m_loopSelector(TheoryOfTimeBase::x_globalLoop)
    {
    }

    static double ToHostSamples(double engineSamples)
    {
        return engineSamples / static_cast<double>(SampleTimer::x_oversample);
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

    void Update(Input& input)
    {
        if (!input.m_theoryOfTime)
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

        const WriteTapeHead& writeTapeHead = input.m_theoryOfTime->m_writeTapeHead;
        double writePosition = ToHostSamples(writeTapeHead.m_actualPosition);
        double globalPeriodSamples = ToHostSamples(writeTapeHead.m_globalPeriodSamples);
        double loopSamples = ComputeLoopSamples(input.m_theoryOfTime, m_loopSelector, globalPeriodSamples);
        double effectiveDelaySamples = loopSamples * input.m_bufferFraction;
        double hopSamples = static_cast<double>(Resynthesizer::GetGrainLaunchSamples());
        if (globalPeriodSamples <= 0.0 || loopSamples <= 0.0)
        {
            m_relativePosition = writeTapeHead.m_relativePosition;
            m_actualPosition = writePosition;
            return;
        }

        double loopPhasor = loopSamples / globalPeriodSamples;
        double effectiveDelayPhasor = effectiveDelaySamples / globalPeriodSamples;
        double hopPhasor = hopSamples / globalPeriodSamples;

        double relativeWrapTop = writeTapeHead.m_relativePosition - hopPhasor;
        double relativeWrapBottom = relativeWrapTop - loopPhasor;
        double relativeProjectedPosition =
            writeTapeHead.m_relativePosition * input.m_readHeadSpeed - effectiveDelayPhasor;
        m_relativePosition = PhaseUtils::WrapMod(relativeWrapBottom, relativeWrapTop, relativeProjectedPosition);

        // Translate the displayed phase displacement into buffer coordinates.
        // The write head's continuity glue must not be scaled by read speed.
        //
        m_actualPosition = writePosition +
            (m_relativePosition - writeTapeHead.m_relativePosition) * globalPeriodSamples;
    }
};
