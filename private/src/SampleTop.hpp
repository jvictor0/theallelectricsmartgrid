#pragma once

#include <cmath>

struct SampleTop
{
    bool m_triggered;

    // Offset in engine samples from the sample carrying this event.
    //
    double m_offset;

    SampleTop()
        : SampleTop(false, 0.0)
    {
    }

    SampleTop(bool triggered)
        : SampleTop(triggered, 0.0)
    {
    }

    SampleTop(bool triggered, double offset)
        : m_triggered(triggered)
        , m_offset(offset)
    {
    }

    explicit operator bool() const
    {
        return m_triggered;
    }

    static SampleTop FromWrap(double phase, double frequency)
    {
        return {true, -phase / frequency};
    }

    static SampleTop FromPhases(double previous, double current)
    {
        if (std::floor(previous) == std::floor(current))
        {
            return {};
        }

        double boundary = std::floor(current) + (current < previous ? 1.0 : 0.0);
        return {true, (boundary - current) / (current - previous)};
    }
};
