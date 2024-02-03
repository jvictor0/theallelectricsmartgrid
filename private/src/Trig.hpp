#pragma once

struct Trig
{
    bool m_wasHigh;

    bool Process(float val)
    {
        if (val > 0)
        {
            if (!m_wasHigh)
            {
                m_wasHigh = true;
                return true;
            }
        }
        else
        {
            m_wasHigh = false;
        }

        return false;
    }
};
