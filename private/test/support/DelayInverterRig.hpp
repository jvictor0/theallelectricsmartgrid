#pragma once

// Stands in for the theory of time when a test drives QuadDelay with its own
// write positions. Owns the delay's time warp inverter and records the input's
// write position into it before each engine sample is processed.
//

#include <memory>

#include "QuadDelay.hpp"
#include "SampleTimer.hpp"

struct DelayInverterRig
{
    using Inverter = TimeWarpInverter<1 << 20>;
    QuadDelay* m_delay;
    std::unique_ptr<Inverter> m_inverter;

    explicit DelayInverterRig(QuadDelay* delay)
        : m_delay(delay)
        , m_inverter(std::make_unique<Inverter>())
    {
        m_delay->m_delayLine.m_inverter = m_inverter.get();
    }

    QuadFloat Process(QuadDelay::Input& input)
    {
        SampleTimer::IncrementSample();
        m_inverter->Write(input.m_writeHeadPhase,
            static_cast<double>(SampleTimer::GetSample()));
        return m_delay->Process(input);
    }
};
