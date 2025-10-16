#pragma once
#include <cmath>
#include "WaveTable.hpp"

namespace PhaseUtils
{
inline float HzToNatural(float hz, float deltaT)
{
    return hz * deltaT;
}

inline float VOctToHz(float vOct)
{
    static const float x_middleC = 261.6256f;
    return powf(2.0f, vOct) * x_middleC;
}

inline float VOctToNatural(float vOct, float deltaT)
{
    return HzToNatural(VOctToHz(vOct), deltaT);
}

struct ExpParam
{
    float m_baseParam;
    float m_expParam;
    float m_base;
    float m_factor;

    ExpParam()
        : ExpParam(2.0f)
    {
    }

    ExpParam(float base)
        : m_baseParam(0)
        , m_expParam(1)
        , m_base(base)
        , m_factor(1)
    {
    }

    explicit ExpParam(float min, float max)
        : m_baseParam(0)
        , m_expParam(min)
        , m_base(max / min)
        , m_factor(min)
    {
    }

    float Update(float value)
    {
        if (m_baseParam != value)
        {
            m_baseParam = value;
            m_expParam = m_factor * std::powf(m_base, value);
        }

        return m_expParam;
    }
};

struct SimpleOsc
{
    float m_phase;
    ExpParam m_phaseDelta;

    SimpleOsc()
        : m_phase(0)
        , m_phaseDelta(0.025f / 44100.0f, 1.0f / 44100.0f)
    {
    }

    void SetPhaseDelta(float phaseDelta)
    {
        m_phaseDelta.Update(phaseDelta);
    }

    void Process()
    {
        m_phase += m_phaseDelta.m_expParam;
        if (m_phase >= 1.0f)
        {
            m_phase -= 1.0f;
        }
    }
};

struct ZeroedExpParam
{
    float m_base;
    float m_expParam;
    float m_baseParam;

    ZeroedExpParam(float base)
        : m_base(base)
        , m_expParam(0)
        , m_baseParam(0)
    {
    }

    ZeroedExpParam()
        : ZeroedExpParam(20)
    {
    }

    float Update(float value)
    {
        if (m_baseParam != value)
        {
            m_baseParam = value;
            m_expParam = (std::powf(m_base, value) - 1) / (m_base - 1);
        }

        return m_expParam;
    }  
};

inline float SyncedEval(
    const WaveTable* waveTable,
    float phase,
    float mult,
    float offset)
{
    float thisPhase = fmod(phase * mult + offset, mult);
    float floorMult = floorf(mult);

    if (floorMult < thisPhase)
    {
        float amp = mult - floorMult;
        return amp * waveTable->Evaluate((thisPhase - floorMult) / amp);
    }
    else
    {
        return waveTable->Evaluate(thisPhase - floorf(thisPhase));
    }
}

} // namespace PhaseUtils
