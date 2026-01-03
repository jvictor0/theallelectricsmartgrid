#pragma once
#include <cmath>
#include "AsyncLogger.hpp"

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

inline double CubicLagrangeNonUniform(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, double x)
{
    static const double x_epsilon = 1e-12;
    
    // Calculate denominators for Lagrange basis polynomials
    //
    double d0 = (x0 - x1) * (x0 - x2) * (x0 - x3);
    double d1 = (x1 - x0) * (x1 - x2) * (x1 - x3);
    double d2 = (x2 - x0) * (x2 - x1) * (x2 - x3);
    double d3 = (x3 - x0) * (x3 - x1) * (x3 - x2);
    
    // Check for degeneracy
    //
    if (std::abs(d0) < x_epsilon || std::abs(d1) < x_epsilon || 
        std::abs(d2) < x_epsilon || std::abs(d3) < x_epsilon)
    {
        // Fall back to linear interpolation between middle points
        //
        double dx = x2 - x1;
        if (std::abs(dx) < x_epsilon)
        {
            return (y1 + y2) * 0.5;
        }

        double t = (x - x1) / dx;
        return y1 + t * (y2 - y1);
    }

    // Calculate Lagrange basis polynomials
    //
    double l0 = ((x - x1) * (x - x2) * (x - x3)) / d0;
    double l1 = ((x - x0) * (x - x2) * (x - x3)) / d1;
    double l2 = ((x - x0) * (x - x1) * (x - x3)) / d2;
    double l3 = ((x - x0) * (x - x1) * (x - x2)) / d3;
    
    return y0 * l0 + y1 * l1 + y2 * l2 + y3 * l3;
}

struct ExpParam
{
    float m_baseParam;
    float m_expParam;
    float m_max;
    float m_base;
    float m_factor;

    ExpParam()
        : ExpParam(2.0f)
    {
    }

    explicit ExpParam(float base)
        : m_baseParam(0)
        , m_expParam(1)
        , m_max(base)
        , m_base(base)
        , m_factor(1)
    {
    }

    explicit ExpParam(float min, float max)
        : m_baseParam(0)
        , m_expParam(min)
        , m_max(max)
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

    static float Compute(float min, float max, float value)
    {
        return min * std::powf(max / min, value);
    }

    float Update(float min, float max, float value)
    {
        if (m_baseParam != value || m_max != max || m_factor != min)
        {
            m_factor = min;
            m_max = max;
            m_base = max / min;
            m_expParam = m_factor * std::powf(m_base, value);
            m_baseParam = value;
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

    explicit ZeroedExpParam(float base)
        : m_base(base)
        , m_expParam(0)
        , m_baseParam(0)
    {
    }

    ZeroedExpParam()
        : ZeroedExpParam(20)
    {
    }

    void SetBaseByCenter(float center)
    {
        // (sqrt(base) - 1) / (base - 1) = center
        // sqrt(base) - 1 = center * (base - 1)
        // sqrt(base) - 1 = center * base - center
        // sqrt(base) = center * base - center + 1
        // sqrt(base) - center * base = 1 - center
        // base^0.5 - center * base = 1 - center
        // Let x = sqrt(base), then x^2 = base
        // x - center * x^2 = 1 - center
        // center * x^2 - x + (1 - center) = 0
        // Using quadratic formula: x = (1 ± sqrt(1 - 4*center*(1-center))) / (2*center)
        // We want the positive solution that gives base > 1
        //        
        float discriminant = 1 - 4 * center * (1 - center);
        float sqrt_base = (1 + std::sqrt(discriminant)) / (2 * center);
        m_base = sqrt_base * sqrt_base;
        m_expParam = (std::powf(m_base, m_baseParam) - 1) / (m_base - 1);
    }

    static float Compute(float base, float value)
    {
        return (std::powf(base, value) - 1) / (base - 1);
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

} // namespace PhaseUtils
