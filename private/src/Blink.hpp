#pragma once

#include <cmath>
#include "SampleTimer.hpp"

struct Blink
{
    bool m_blink;
    float m_time;
    
    Blink()
        : m_blink(false)
        , m_time(0)
    {
    }

    void Process()
    {
        m_time += SampleTimer::x_hostSamplePeriod;
        if (1.0 <= m_time)
        {
            m_time -= 1.0;
        }

        int slice = std::floor(m_time * 8);
        m_blink = slice == 0 || slice == 2;
    }
};