#pragma once

#include "QuadUtils.hpp"
#include "WaveTable.hpp"
#include "Slew.hpp"

struct QuadLFO
{
    QuadFloat m_phase;
    const WaveTable* m_sin;
    
    QuadFloat m_output;

    OPLowPassFilter m_slew[4];

    QuadLFO()
        : m_phase(0.0f, 0.0f, 0.0f, 0.0f)
        , m_sin(nullptr)
    {
        m_sin = &WaveTable::GetSine();
    }

    void SetSlew(float freq)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_slew[i].SetAlphaFromNatFreq(freq);
        }
    }

    static float GetPhase(int i, float phaseKnob)
    {
        if (phaseKnob < 1.0 / 3)
        {
            return 3 * phaseKnob;
        }
        else if (phaseKnob < 5.0 / 12.0)
        {
            phaseKnob = (phaseKnob - 1.0 / 3) * 3;
            return phaseKnob * (i % 2 == 0 ? 1 : -1);
        }
        else if (phaseKnob < 11.0 / 24.0)
        {
            phaseKnob = (phaseKnob - 5.0 / 12.0) * 3;
            float base = 0.25 * (i % 2 == 0 ? 1 : -1);
            return base + phaseKnob * (i / 2 == 0 ? 1 : -1);
        }
        else
        {
            phaseKnob = (phaseKnob - 11.0 / 24.0) * 3;
            float base = 0.25 * (i % 2 == 0 ? 1 : -1) + 0.125 * (i / 2 == 0 ? 1 : -1);
            return base + phaseKnob;
        }
    }

    struct Input
    {
        QuadFloat m_freq;
        QuadFloat m_phaseKnob;
        Input()
            : m_freq(1.0f, 1.0f, 1.0f, 1.0f)
            , m_phaseKnob(0.0f, 0.0f, 0.0f, 0.0f)
        {
        }
      
        QuadFloat GetPhaseOffset() const
        {
            return QuadFloat(
                QuadLFO::GetPhase(0, m_phaseKnob[1]),
                QuadLFO::GetPhase(1, m_phaseKnob[2]),
                QuadLFO::GetPhase(2, m_phaseKnob[3]),
                QuadLFO::GetPhase(3, m_phaseKnob[0]));
        }
    };

    void Process(const Input& input)
    {
        for (int i = 1; i < 4; ++i)
        {
            if (input.m_freq[i] / input.m_freq[0] < 1.0001 && 0.9999 < input.m_freq[i] / input.m_freq[0])
            {
                float toCatch = (std::abs(m_phase[i] - m_phase[0]) > 0.5 && m_phase[i] < m_phase[0]) ? m_phase[0] - 1 : m_phase[0];
                m_phase[i] = (1 - 5 * input.m_freq[i]) * m_phase[i] + 5 * input.m_freq[i] * toCatch;
            }
        }

        m_phase += input.m_freq;
        m_phase = m_phase.ModOne();

        QuadFloat phase = m_phase + input.GetPhaseOffset();
        phase = phase.ModOne();
        for (int i = 0; i < 4; ++i)
        {
            m_slew[i].SetAlphaFromNatFreq(input.m_freq[i] * 2);
            m_output[i] = m_slew[i].Process(m_sin->Evaluate(phase[i]));
        }
    }
};
