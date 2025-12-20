#pragma once

struct VCO
{
    float m_output;
    float m_phase;
    const WaveTable* m_waveTable;

    VCO()
    {
        m_output = 0;
        m_phase = 0;
        m_waveTable = &WaveTable::GetSine();
    }

    float Process(float freq)
    {
        m_phase += freq;
        while (m_phase >= 1)
        {
            m_phase -= 1;
        }
        
        m_output = m_waveTable->Evaluate(m_phase);
        return m_output;
    }
};