#pragma once

#include <cmath>
#include <cstddef>
#include <limits>

// The observation fields belong to the audio thread. UI readers use a captured
// view and the atomic inverse lookup, never these live observation fields.
//
struct TimeWarpSource
{
    const size_t m_capacity;
    double m_observedRealTime = 0.0;
    double m_previousObservedRealTime = 0.0;
    double m_observedWarpedTime = 0.0;
    double m_previousObservedWarpedTime = 0.0;
    double m_oldestWarpedTime = -std::numeric_limits<double>::infinity();
    bool m_hasObservation = false;

    explicit TimeWarpSource(size_t capacity)
        : m_capacity(capacity)
    {
    }

    virtual ~TimeWarpSource() = default;
    virtual double ReadInverse(double warpedTime, double latestRealTime) const = 0;
};

struct TimeWarpView
{
    const TimeWarpSource* m_source = nullptr;
    double m_realTime = 0.0;
    double m_warpedTime = 0.0;
    double m_oldestWarpedTime = 0.0;

    double GetRealTime(double warpedTime) const
    {
        if (!m_source || !std::isfinite(warpedTime)
            || std::floor(warpedTime) < m_oldestWarpedTime || warpedTime > m_warpedTime
            || std::ceil(warpedTime) > m_oldestWarpedTime + static_cast<double>(m_source->m_capacity - 1))
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return m_source->ReadInverse(warpedTime, m_realTime);
    }
};
