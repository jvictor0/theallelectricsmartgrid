#pragma once

#include "QuadUtils.hpp"
#include "WaveTable.hpp"
#include "Slew.hpp"

struct QuadLFO
{
    float m_phase;
    const WaveTable* m_sin;
    
    QuadFloat m_output;

    OPSlew m_slew[4];

    QuadLFO()
        : m_phase(0.0f)
        , m_sin(nullptr)
    {
        m_sin = &WaveTable::GetSine();
    }

    void SetSlew(float time, float dt)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_slew[i].SetAlphaFromTime(time, dt);
        }
    }

    struct Input
    {
        float m_freq;
        float m_phaseOffset;
        float m_crossDepth;
        Input()
            : m_freq(1.0f)
            , m_phaseOffset(0.0f)
            , m_crossDepth(0.0f)
        {
        }
    };

    void Process(const Input& input)
    {
        m_phase += input.m_freq;
        if (1 <= m_phase)
        {
            m_phase -= 1;
        }

        QuadFloat phase = QuadFloat(m_phase, m_phase, m_phase, m_phase);
        phase += QuadFloat(0, 0.25, 0.5, 0.75) * input.m_phaseOffset;
        phase += m_output.Rotate90() * input.m_crossDepth;

        phase = phase.ModOne();
        for (int i = 0; i < 4; ++i)
        {
            m_output[i] = m_slew[i].Process(m_sin->Evaluate(phase[i]));
        }
    }
};
