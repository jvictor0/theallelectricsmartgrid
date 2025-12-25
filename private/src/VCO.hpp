#pragma once

#include "Math.hpp"

struct VCO
{
    float m_output;
    float m_phase;

    VCO()
    {
        m_output = 0;
        m_phase = 0;
    }

    float Process(float freq)
    {
        m_phase += freq;
        while (m_phase >= 1)
        {
            m_phase -= 1;
        }
        
        m_output = Math::Sin2pi(m_phase);
        return m_output;
    }
};