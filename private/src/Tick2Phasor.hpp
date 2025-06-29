#pragma once

#include <cmath>

struct Tick2Phasor
{
    static constexpr float x_alpha = 10;
    double m_phaseIncrement;
    double m_samplesSinceLastTick;
    double m_output;
    double m_prePhase;
    double m_tickStart;
    double m_tickEnd;
    uint64_t m_tickCount;
    bool m_reset;

    struct Input
    {
        bool m_tick;
        bool m_reset;
        int m_ticksPerPhase;

        Input()
        {
            m_tick = false;
            m_reset = false;
            m_ticksPerPhase = 24 * 4 * 4;
        }
    };

    Tick2Phasor()
    {
        m_phaseIncrement = 0;
        m_samplesSinceLastTick = 1;
        m_output = 0;
        m_prePhase = 0;
        m_tickCount = 0;
        m_tickStart = 0;
        m_tickEnd = 1;
    }

    void Reset()
    {
        m_reset = false;
        m_tickCount = 0;
        m_prePhase = 0;
        m_tickStart = 0;
        m_tickEnd = 1;
        m_output = 0;
    }

    void ComputeOutput(const Input& input)
    {
        double phase = m_prePhase / std::pow(1 + std::pow(m_prePhase, x_alpha), 1.0 / x_alpha);
        m_output = m_tickStart + phase * (m_tickEnd - m_tickStart);
    }

    void Process(const Input& input)
    {
        if (input.m_reset)
        {
            m_reset = true;
        }

        if (input.m_tick)
        {
            if (m_reset)
            {
                Reset();
            }
            else
            {
                m_prePhase += m_phaseIncrement;            
                ComputeOutput(input);

                m_tickCount++;
                m_prePhase = 0;
            }

            m_phaseIncrement = 1.0f / m_samplesSinceLastTick;
            m_samplesSinceLastTick = 0;
            m_tickStart = m_output;
            m_tickEnd = static_cast<double>(m_tickCount % input.m_ticksPerPhase) / input.m_ticksPerPhase;
        }
        else
        {
            m_prePhase += m_phaseIncrement;
            ++m_samplesSinceLastTick;
        }

        ComputeOutput(input);
    }
};
