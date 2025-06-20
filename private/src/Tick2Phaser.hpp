#pragma once

#include <cmath>

struct Tick2Phaser
{
    static constexpr float x_alpha = 10;
    double m_phaseIncrement;
    double m_samplesSinceLastTick;
    double m_output;
    double m_prePhase;
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

    Tick2Phaser()
    {
        m_phaseIncrement = 0;
        m_samplesSinceLastTick = 1;
        m_output = 0;
        m_prePhase = 0;
        m_tickCount = 0;
    }

    void Reset()
    {
        m_reset = false;
        m_tickCount = 0;
        m_prePhase = 0;
    }

    void ComputeOutput(const Input& input)
    {
        double base = static_cast<double>(m_tickCount % input.m_ticksPerPhase) / input.m_ticksPerPhase;
        double phase = m_prePhase / std::pow(1 + std::pow(m_prePhase, x_alpha), 1.0 / x_alpha);
        m_output = base + phase / input.m_ticksPerPhase;
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
                m_tickCount++;
                m_phaseIncrement = 1.0f / m_samplesSinceLastTick;
                m_prePhase = 0;            
            }

            m_samplesSinceLastTick = 0;
        }
        else
        {
            m_prePhase += m_phaseIncrement;
            ++m_samplesSinceLastTick;
        }

        ComputeOutput(input);
    }
};
