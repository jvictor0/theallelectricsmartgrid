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
    QuadDelay* m_delay;
    std::unique_ptr<QuadDelay::Writer::Inverter> m_inverter;

    explicit DelayInverterRig(QuadDelay* delay)
        : m_delay(delay)
        , m_inverter(std::make_unique<QuadDelay::Writer::Inverter>())
    {
        m_delay->m_delayLine.m_inverter = m_inverter.get();
    }

    QuadFloat Process(QuadDelay::Input& input)
    {
        SampleTimer::IncrementSample();
        m_inverter->Write(input.m_writeHeadPosition * static_cast<double>(SampleTimer::x_oversample),
            static_cast<double>(SampleTimer::GetSample()));
        return m_delay->Process(input);
    }
};
