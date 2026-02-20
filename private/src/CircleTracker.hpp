#pragma once

#include <cmath>

struct CircleTracker
{
    double m_phase;
    int m_winding;
    bool m_top;

    void Reset(double phase)
    {
        m_phase = phase;
        m_winding = 0;
        m_top = false;
    }

    CircleTracker()
        : m_phase(0.0)
        , m_winding(0)
        , m_top(false)
    {
    }

    void Process(double phase)
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

    double UnWind() const
    {
        return static_cast<double>(m_phase) + m_winding;
    }

    double Distance(const CircleTracker& other) const
    {
        return std::abs(UnWind() - other.UnWind());
    }
};

struct CircleDistanceTracker
{
    CircleTracker m_start;
    CircleTracker m_now;

    void Reset(double phase)
    {
        m_start.Reset(phase);
        m_now.Reset(phase);
    }

    void Process(double phase)
    {
        m_now.Process(phase);
    }
    
    double Distance() const
    {
        return m_now.Distance(m_start);
    }
};