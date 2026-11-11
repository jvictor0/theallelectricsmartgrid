// Inverse-only extraction from DelayLineMovableWriterGeneric at
// 5c3cb419952ff0dee4cfa9c18cb0e1b18874dbd2, before shared inverse work.
// Scatter, wrap, and scalar lookup bodies are verbatim. Write removes only
// audio/envelope writes and their log; its second argument adapts the harness.
// Keep the original index initialization, plain doubles, clock origin and step.
// Call once per host frame with positions in host samples, as old QuadDelay did.
//
#pragma once

#include "PhaseUtils.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>

template <size_t MaxDelaySamples>
struct OriginalTimeWarpInverter
{
    static constexpr size_t x_maxDelaySamples = MaxDelaySamples;
    double m_writeHeadInverse[x_maxDelaySamples];
    static constexpr size_t x_scatterBufferSize = 8;
    size_t m_scatterHead;
    double m_scatterBufferWarpedTime[x_scatterBufferSize] = {0.0};
    double m_scatterBufferTime[x_scatterBufferSize] = {0.0};
    double m_turnaroundWarpedTime;
    bool m_descending;
    double m_lastWarpedTime;
    size_t m_ascendingCount;
    double m_lastTime;

    OriginalTimeWarpInverter()
        : m_scatterHead(x_scatterBufferSize)
        , m_turnaroundWarpedTime(0.0)
        , m_descending(false)
        , m_lastWarpedTime(0.0)
        , m_ascendingCount(0)
        , m_lastTime(17700716)
    {
        for (size_t i = 0; i < x_maxDelaySamples; ++i)
        {
            m_writeHeadInverse[i] = i;
        }
    }

    void ScatterBufferToDelayLine()
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
            m_writeHeadInverse[WrapIndex(i)] = realTimeLow + alpha * (realTimeHigh - realTimeLow);
        }
    }

    static size_t WrapIndex(int64_t index)
    {
        return static_cast<size_t>(PhaseUtils::FloorMod(index, static_cast<int64_t>(x_maxDelaySamples)));
    }

    void Write(double time, double)
    {
        if (time < m_lastWarpedTime)
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
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = m_lastTime - 1.0;
            ++m_scatterHead;
            m_ascendingCount = 1;
            m_turnaroundWarpedTime = time;
            m_descending = false;
        }

        m_lastWarpedTime = time;
        if (!m_descending)
        {
            ++m_ascendingCount;
            m_scatterBufferWarpedTime[m_scatterHead % x_scatterBufferSize] = time;
            m_scatterBufferTime[m_scatterHead % x_scatterBufferSize] = m_lastTime;
            if (2 <= m_ascendingCount)
            {
                ScatterBufferToDelayLine();
            }

            ++m_scatterHead;
        }

        m_lastTime += 1.0;
    }

    double GetRealTime(double warpedTime)
    {
        double floorTime = std::floor(warpedTime);
        size_t i0 = WrapIndex(static_cast<int64_t>(floorTime));
        size_t i1 = (i0 + 1) % x_maxDelaySamples;
        double alpha = warpedTime - floorTime;
        return m_writeHeadInverse[i0] + alpha * (m_writeHeadInverse[i1] - m_writeHeadInverse[i0]);
    }
};
