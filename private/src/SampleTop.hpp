#pragma once

#include <algorithm>
#include <cmath>

#include "SampleTimer.hpp"

struct SampleTop
{
    bool m_triggered;

    // Offset from the sample carrying this event, always in base-rate samples.
    // Halfway through a 4x oversample is -0.125, not -0.5.
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

    static SampleTop FromWrap(double phase, double frequency, double deltaT)
    {
        return {true, -phase / frequency * deltaT * SampleTimer::x_sampleRate};
    }

    void AccumulateOversample(const SampleTop& top, size_t index, size_t oversample)
    {
        if (top)
        {
            *this = top;
            m_offset += static_cast<double>(index % oversample + 1) / static_cast<double>(oversample) - 1.0;
        }
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

    double GetControlPosition(size_t uBlockIndex) const
    {
        // The control writer advances just after sample zero of each microblock.
        //
        return GetPosition(static_cast<double>(uBlockIndex)) / SampleTimer::x_controlFrameRate
            - (uBlockIndex == 0 ? 0.0 : 1.0);
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
