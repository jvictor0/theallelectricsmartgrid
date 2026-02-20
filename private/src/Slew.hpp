#pragma once

#include "Filter.hpp"
#include "PhaseUtils.hpp"

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

struct ParamSlew
{
    OPLowPassFilter m_filter;
    float m_target;

    ParamSlew()
        : ParamSlew(1.0f)
    {
    }

    ParamSlew(float relativeSampleRate)
        : m_target(0.0f)
    {
        m_filter.SetAlphaFromNatFreq((1000.0f / 48000.0f) / relativeSampleRate);
    }

    void Update(float value)
    {
        m_target = value;
    }

    float Process()
    {
        return m_filter.Process(m_target);
    }
};

struct ParamSlewDouble
{
    OPLowPassFilterDouble m_filter;
    double m_target;

    ParamSlewDouble()
        : ParamSlewDouble(1.0)
    {
    }

    ParamSlewDouble(double relativeSampleRate)
        : m_target(0.0)
    {
        m_filter.SetAlphaFromNatFreq((1000.0 / 48000.0) / relativeSampleRate);
    }

    void Update(double value)
    {
        m_target = value;
    }

    double Process()
    {
        return m_filter.Process(m_target);
    }
};

struct ExpParamSlew
{
    OPLowPassFilter m_filter;
    PhaseUtils::ExpParam m_expParam;

    ExpParamSlew()
        : ExpParamSlew(1.0f, 2.0f)
    {
    }

    ExpParamSlew(float relativeSampleRate, float base)
        : m_expParam(base)
    {
        m_filter.SetAlphaFromNatFreq((1000.0f / 48000.0f) / relativeSampleRate);
    }

    ExpParamSlew(float relativeSampleRate, float min, float max)
        : m_expParam(min, max)
    {
        m_filter.SetAlphaFromNatFreq((1000.0f / 48000.0f) / relativeSampleRate);
    }

    void Update(float value)
    {
        m_expParam.Update(value);
    }

    float Process()
    {
        return m_filter.Process(m_expParam.m_expParam);
    }
};

struct ZeroedExpParamSlew
{
    OPLowPassFilter m_filter;
    PhaseUtils::ZeroedExpParam m_expParam;

    ZeroedExpParamSlew()
        : ZeroedExpParamSlew(1.0f, 20.0f)
    {
    }

    ZeroedExpParamSlew(float relativeSampleRate, float base)
        : m_expParam(base)
    {
        m_filter.SetAlphaFromNatFreq((1000.0f / 48000.0f) / relativeSampleRate);
    }

    void SetBaseByCenter(float center)
    {
        m_expParam.SetBaseByCenter(center);
    }

    void Update(float value)
    {
        m_expParam.Update(value);
    }

    float Process()
    {
        return m_filter.Process(m_expParam.m_expParam);
    }
};