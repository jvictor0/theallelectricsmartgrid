// Frozen a8a6ca1 inverse; only the type name is changed for side-by-side density comparisons.
//
#pragma once

#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "TimeWarpView.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Scalar map from a write head's warped time back to the real sample time at
// which it was written. The caller supplies absolute engine timestamps. The inverse
// updates only while warped time ascends, so reversals read previously mapped
// history until forward motion replaces it.
//
template <size_t MaxSamples>
struct LegacyTimeWarpInverter : TimeWarpSource
{
    static constexpr size_t x_maxSamples = MaxSamples;
    static constexpr size_t x_scatterBufferSize = 8;

    static_assert(std::atomic<double>::is_always_lock_free);
    std::atomic<double> m_writeHeadInverse[x_maxSamples];
    double m_highestWrittenIndex = -std::numeric_limits<double>::infinity();

    size_t m_scatterHead;
    double m_scatterBufferWarpedTime[x_scatterBufferSize] = {0.0};
    double m_scatterBufferTime[x_scatterBufferSize] = {0.0};

    bool m_descending;
    double m_lastWarpedTime;
    size_t m_ascendingCount;

    double m_previousRealTime;

    LegacyTimeWarpInverter()
        : TimeWarpSource(MaxSamples)
        , m_scatterHead(x_scatterBufferSize)
        , m_descending(false)
        , m_lastWarpedTime(0.0)
        , m_ascendingCount(0)
        , m_previousRealTime(0.0)
    {
        for (size_t i = 0; i < x_maxSamples; ++i)
        {
            m_writeHeadInverse[i].store(std::numeric_limits<double>::quiet_NaN(), std::memory_order_relaxed);
        }
    }

    static size_t WrapIndex(int64_t index)
    {
        return static_cast<size_t>(PhaseUtils::FloorMod(index, static_cast<int64_t>(x_maxSamples)));
    }

    void ScatterBufferToInverse()
    {
        double timeLowSamples = m_scatterBufferWarpedTime[(m_scatterHead - 1) % x_scatterBufferSize];
        double timeHighSamples = m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize];
        double realTimeLow = m_scatterBufferTime[(m_scatterHead - 1) % x_scatterBufferSize];
        double realTimeHigh = m_scatterBufferTime[m_scatterHead % x_scatterBufferSize];

        int64_t timeLowIndex = static_cast<int64_t>(std::ceil(timeLowSamples));
        int64_t timeHighIndex = static_cast<int64_t>(std::ceil(timeHighSamples));

        for (int64_t i = timeLowIndex; i < timeHighIndex; ++i)
        {
            double alpha = (static_cast<double>(i) - timeLowSamples) / (timeHighSamples - timeLowSamples);
            m_writeHeadInverse[WrapIndex(i)].store(realTimeLow + alpha * (realTimeHigh - realTimeLow), std::memory_order_relaxed);
        }

        if (timeLowIndex < timeHighIndex)
        {
            m_highestWrittenIndex = std::max(m_highestWrittenIndex, static_cast<double>(timeHighIndex - 1));
            m_oldestWarpedTime = m_highestWrittenIndex - static_cast<double>(x_maxSamples - 1);
        }
    }

    // Records an observed pair. Unwritten entries remain unavailable, and the
    // previous real timestamp is retained only as an interpolation endpoint.
    //
    void Write(double warpedTime, double realTime)
    {
        assert(std::isfinite(warpedTime) && std::isfinite(realTime));
        m_previousObservedRealTime = m_hasObservation ? m_observedRealTime : realTime;
        m_previousObservedWarpedTime = m_hasObservation ? m_observedWarpedTime : warpedTime;
        m_observedWarpedTime = warpedTime;
        m_observedRealTime = realTime;
        m_hasObservation = true;
        if (warpedTime < m_lastWarpedTime)
        {
            m_descending = true;
            m_ascendingCount = 0;
        }
        else if (m_descending)
        {
            // Include the turnaround sample in the first forward interval.
            // Descending intervals still leave inverse history untouched.
            //
            m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize] = m_lastWarpedTime;
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = m_previousRealTime;
            ++m_scatterHead;
            m_ascendingCount = 1;
            m_descending = false;
        }

        m_lastWarpedTime = warpedTime;
        if (!m_descending)
        {
            ++m_ascendingCount;
            m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize] = warpedTime;
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = realTime;
            if (2 <= m_ascendingCount)
            {
                ScatterBufferToInverse();
            }

            ++m_scatterHead;
        }

        m_previousRealTime = realTime;
    }

    double ReadInverse(double warpedTime, double latestRealTime) const override
    {
        assert(std::isfinite(warpedTime));
        double floorTime = std::floor(warpedTime);
        size_t i0 = WrapIndex(static_cast<int64_t>(floorTime));
        size_t i1 = (i0 + 1) % x_maxSamples;
        double alpha = warpedTime - floorTime;
        double first = m_writeHeadInverse[i0].load(std::memory_order_relaxed);
        if (!std::isfinite(first) || first > latestRealTime)
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        if (alpha == 0.0)
        {
            return first;
        }

        double second = m_writeHeadInverse[i1].load(std::memory_order_relaxed);
        return std::isfinite(second) && second <= latestRealTime
            ? first + alpha * (second - first) : std::numeric_limits<double>::quiet_NaN();
    }

    double GetRealTime(double warpedTime) const
    {
        return ReadInverse(warpedTime, std::numeric_limits<double>::infinity());
    }

    // Each channel mapped from its own warped time.
    //
    template <size_t Size>
    MultiChannelNumber<double, Size> GetRealTime(const MultiChannelNumber<double, Size>& warpedTime)
    {
        MultiChannelNumber<double, Size> result;
        for (size_t i = 0; i < Size; ++i)
        {
            result[i] = GetRealTime(warpedTime[i]);
        }

        return result;
    }
};
