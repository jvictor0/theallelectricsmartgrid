#pragma once

#include <cmath>
#include <cstdint>
#include "SampleTimer.hpp"

struct AudioCallbackDiagnostics
{
    struct Observation
    {
        uint64_t m_sequence = 0;
        uint64_t m_gapUs = 0;
        uint64_t m_previousBudgetUs = 0;
    };

    uint64_t m_sequence = 0;
    uint64_t m_previousStartUs = 0;
    uint64_t m_previousBudgetUs = 0;

    Observation Observe(uint64_t startUs, int numFrames, double sampleRate)
    {
        Observation result;
        result.m_sequence = ++m_sequence;
        result.m_gapUs = m_sequence > 1 ? startUs - m_previousStartUs : 0;
        result.m_previousBudgetUs = m_previousBudgetUs;
        m_previousStartUs = startUs;
        m_previousBudgetUs = sampleRate > 0.0
            ? static_cast<uint64_t>(std::llround(numFrames * 1000000.0 / sampleRate))
            : 0;
        return result;
    }

    void Reset()
    {
        m_sequence = 0;
        m_previousStartUs = 0;
        m_previousBudgetUs = 0;
    }

    static bool CanRender(double sampleRate)
    {
        return sampleRate == static_cast<double>(SampleTimer::x_sampleRate);
    }
};
