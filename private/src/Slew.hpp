#pragma once

#include "Filter.hpp"

struct Slew
{
    float m_val;

    Slew() : m_val(0)
    {
    }

    Slew(float val) : m_val(val)
    {
    }

    float SlewDown(float dt, float time, float trg)
    {
        if (m_val <= trg || time < dt)
        {
            m_val = trg;
        }
        else
        {
            m_val = std::max<float>(m_val - dt / time, trg);
        }

        return m_val;
    }
    
    static float Process(float src, float trg, float alpha)
    {
        return src + alpha * (trg - src);
    }
};

struct FixedSlew
{
    float m_val;
    float m_maxDistance;

    FixedSlew() : m_val(0), m_maxDistance(1.0/128.0)
    {
    }

    FixedSlew(float maxDistance) : m_val(0), m_maxDistance(maxDistance)
    {
    }

    float Process(float trg)
    {
        if (m_val < trg)
        {
            m_val = std::min<float>(m_val + m_maxDistance, trg);
        }
        else if (m_val > trg)
        {
            m_val = std::max<float>(m_val - m_maxDistance, trg);
        }

        return m_val;
    }
};

struct OPSlew
{
    float m_val;
    float m_alpha;

    OPSlew() : m_val(0), m_alpha(0)
    {
    }

    OPSlew(float alpha) : m_val(0), m_alpha(alpha)
    {
    }

    void SetAlphaFromTime(float tau, float dt)
    {
        m_alpha = dt / (tau + dt);
    }

    float Process(float trg)
    {
        m_val += m_alpha * (trg - m_val);
        return m_val;
    }
};

struct BiDirectionalSlew
{
    float m_output;
    OPLowPassFilter m_slewUp;
    OPLowPassFilter m_slewDown;

    BiDirectionalSlew() : m_output(0)
    {
    }

    void SetSlewUp(float cyclesPerSample)
    {
        m_slewUp.SetAlphaFromNatFreq(cyclesPerSample);
    }

    void SetSlewDown(float cyclesPerSample)
    {
        m_slewDown.SetAlphaFromNatFreq(cyclesPerSample);
    }

    static float Process(float src, float trg, float slewUpAlpha, float slewDownAlpha)
    {
        if (src < trg)
        {
            return src + slewUpAlpha * (trg - src);
        }
        else
        {
            return src + slewDownAlpha * (trg - src);
        }
    }

    float Process(float trg)
    {
        m_output = Process(m_output, trg, m_slewUp.m_alpha, m_slewDown.m_alpha);
        return m_output;
    }
};

struct SlewUp
{
    float m_output;
    OPLowPassFilter m_slewUp;

    SlewUp() 
        : m_output(0)
    {
    }

    void SetSlewUp(float cyclesPerSample)
    {
        m_slewUp.SetAlphaFromNatFreq(cyclesPerSample);
    }

    static float Process(float src, float trg, float slewUpAlpha)
    {
        if (src < trg)
        {
            return src + slewUpAlpha * (trg - src);
        }
        else
        {
            return trg;
        }
    }

    float Process(float trg)
    {
        m_output = Process(m_output, trg, m_slewUp.m_alpha);
        return m_output;
    }
};