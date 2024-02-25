#pragma once

struct PeriodChecker
{
    float m_timeToCheck;
    float m_checkTime;
    bool m_shouldCheck;

    PeriodChecker(float checkTime=0.01)
        : m_timeToCheck(-1)
        , m_checkTime(checkTime)
        , m_shouldCheck(false)
    {
    }
    
    bool Process(float dt)
    {
        m_timeToCheck -= dt;
        if (m_timeToCheck < 0)
        {
            m_shouldCheck = true;
            m_timeToCheck = m_checkTime;
        }
        else
        {
            m_shouldCheck = false;
        }

        return m_shouldCheck;
    }

    bool ShouldCheck()
    {
        return m_shouldCheck;
    }
};
