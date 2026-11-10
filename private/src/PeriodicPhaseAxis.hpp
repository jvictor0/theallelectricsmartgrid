#pragma once

#include <cassert>
#include <cmath>

// Explicit logical periods need no observed start events. Points ahead of the
// cursor read the same phase of the preceding cycle. A zero period denotes an
// event-driven scope. See formal/PhaseTime/Periodic.lean.
//
struct PeriodicPhaseAxis
{
    double m_origin = 0.0;
    double m_period = 0.0;

    double Start(double now) const
    {
        assert(m_period > 0.0);
        return m_origin + std::floor((now - m_origin) / m_period) * m_period;
    }

    double Coordinate(double now, double position) const
    {
        double coordinate = Start(now) + position * m_period;
        return coordinate <= now ? coordinate : coordinate - m_period;
    }
};
