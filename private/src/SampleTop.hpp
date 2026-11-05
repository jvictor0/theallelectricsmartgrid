#pragma once

#include <algorithm>
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

    static SampleTop AndIdentity(bool hasInputs)
    {
        // Real offsets are in [-1, 0]; zero would discard every fractional top.
        // With no inputs, retain the original true event at the current sample.
        //
        return {true, hasInputs ? -1.0 : 0.0};
    }

    SampleTop operator&&(const SampleTop& other) const
    {
        return m_triggered && other.m_triggered
            ? SampleTop(true, std::max(m_offset, other.m_offset)) : SampleTop();
    }

    static SampleTop FromWrap(double phase, double frequency)
    {
        return {true, -phase / frequency};
    }

    void InterpolatePhases(double previous, double current, double cycleRatio, bool started)
    {
        if (m_triggered && !started)
        {
            m_offset = FromPhases(previous * cycleRatio, current * cycleRatio).m_offset;
        }
    }

    double GetPosition(double sampleIndex) const
    {
        return sampleIndex + m_offset;
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
