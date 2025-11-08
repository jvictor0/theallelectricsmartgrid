#pragma once

#include <cmath>

struct CircleTracker
{
    float m_phase;
    int m_winding;
    bool m_top;

    void Reset(float phase)
    {
        m_phase = phase;
        m_winding = 0;
        m_top = false;
    }

    CircleTracker()
    {
        Reset(0);
    }

    void Process(float phase)
    {
        if (std::abs(phase - m_phase) > 0.5f)
        {
            m_top = true;
            if (m_phase < phase)
            {
                m_winding--;
            }
            else
            {
                m_winding++;
            }
        }
        else
        {
            m_top = false;
        }

        m_phase = phase;
    }

    float UnWind() const
    {
        return m_phase + m_winding;
    }

    float Distance(const CircleTracker& other) const
    {
        return std::abs(UnWind() - other.UnWind());
    }
};

struct CircleDistanceTracker
{
    CircleTracker m_start;
    CircleTracker m_now;

    void Reset(float phase)
    {
        m_start.Reset(phase);
        m_now.Reset(phase);
    }

    void Process(float phase)
    {
        m_now.Process(phase);
    }
    
    float Distance() const
    {
        return m_now.Distance(m_start);
    }
};