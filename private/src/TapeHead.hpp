#pragma once

#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
#include <cassert>

// Read and write positions share continuous master-cycle coordinates.
// Analysis-window safety is applied in real time after inversion by the grain
// manager. See formal/PhaseTime/Periodic.lean: read_head_bounds.
//
struct ReadTapeHead
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

    double m_phase = 0.0;
    int m_loopSelector = TheoryOfTimeBase::x_globalLoop;

    static double ComputeLoopPeriod(const TheoryOfTimeBase& time, int loopSelector)
    {
        int64_t ratio = time.GetCycleRatio(loopSelector);
        assert(ratio > 0);
        return 1.0 / static_cast<double>(ratio);
    }

    void Update(const Input& input)
    {
        assert(input.m_theoryOfTime);
        const auto& time = *input.m_theoryOfTime;
        bool allowLoopSwitch = input.m_requestedLoopSelector != m_loopSelector &&
            time.CrossedCycleBoundary(m_loopSelector, PhaseDomain::Modulated) &&
            time.CrossedCycleBoundary(input.m_requestedLoopSelector, PhaseDomain::Modulated);
        if (allowLoopSwitch)
        {
            m_loopSelector = input.m_requestedLoopSelector;
        }

        double write = time.m_phaseClock.m_phase;
        double period = ComputeLoopPeriod(time, m_loopSelector);
        double projected = write * input.m_readHeadSpeed - period * input.m_bufferFraction;
        m_phase = PhaseUtils::WrapMod(write - period, write, projected);
    }
};
