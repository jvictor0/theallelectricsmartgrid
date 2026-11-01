#pragma once
#include <cmath>
#include <cstdint>
#include <cassert>
#include "AsyncLogger.hpp"
#include "MultiChannelNumber.hpp"
#include "SampleTimer.hpp"

namespace PhaseUtils
{
inline int64_t FloorDiv(int64_t value, int64_t divisor)
{
    assert(divisor > 0);
    return value / divisor - (value % divisor < 0 ? 1 : 0);
}

inline int64_t FloorMod(int64_t value, int64_t divisor)
{
    assert(divisor > 0);
    int64_t remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

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

inline double WrapMod(double a, double b, double x)
{
    double c = b - a;
    return x - c * std::floor((x - a) / c);
}

inline void HalfRangeCrossfade(float value, float& left, float& right)
{
    if (value <= 0.0f)
    {
        left = 1.0f;
        right = 0.0f;
    }
    else if (value < 0.5f)
    {
        left = 1.0f;
        right = 2.0f * value;
    }
    else if (value < 1.0f)
    {
        left = 2.0f * (1.0f - value);
        right = 1.0f;
    }
    else
    {
        left = 0.0f;
        right = 1.0f;
    }
}

template <typename Number, size_t Size>
struct ExpParamGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    Vector m_baseParam;
    Vector m_expParam;
    Number m_max;
    Number m_log2Base;
    Number m_factor;

    ExpParamGeneric()
        : ExpParamGeneric(static_cast<Number>(2))
    {
    }

    explicit ExpParamGeneric(Number base)
        : m_baseParam(Vector::Splat(0))
        , m_expParam(Vector::Splat(1))
        , m_max(base)
        , m_log2Base(std::log2(base))
        , m_factor(1)
    {
    }

    explicit ExpParamGeneric(Number min, Number max)
        : m_baseParam(Vector::Splat(0))
        , m_expParam(Vector::Splat(min))
        , m_max(max)
        , m_log2Base(std::log2(max / min))
        , m_factor(min)
    {
    }

    // factor * base^value, computed as factor * 2^(value * log2(base)) with the
    // fast vector Exp2 (a few float ulps from std::pow).
    //
    static Vector Evaluate(Number factor, Number log2Base, const Vector& value)
    {
        return (value * log2Base).Exp2() * factor;
    }

    Vector Update(const Vector& value)
    {
        if (m_baseParam != value)
        {
            m_baseParam = value;
            m_expParam = Evaluate(m_factor, m_log2Base, value);
        }

        return m_expParam;
    }

    static Vector Compute(Number min, Number max, const Vector& value)
    {
        return Evaluate(min, std::log2(max / min), value);
    }

    Vector Update(Number min, Number max, const Vector& value)
    {
        if (m_baseParam != value || m_max != max || m_factor != min)
        {
            m_factor = min;
            m_max = max;
            m_log2Base = std::log2(max / min);
            m_expParam = Evaluate(m_factor, m_log2Base, value);
            m_baseParam = value;
        }

        return m_expParam;
    }
};

using ExpParam = ExpParamGeneric<float, 1>;

struct SimpleOsc
{
    float m_phase;
    ExpParam m_phaseDelta;

    SimpleOsc()
        : m_phase(0)
        , m_phaseDelta(SampleTimer::HzToCycPerSamp(0.025), SampleTimer::HzToCycPerSamp(1.0))
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

template <typename Number, size_t Size>
struct ZeroedExpParamGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    Number m_base;
    Number m_log2Base;
    Vector m_expParam;
    Vector m_baseParam;
    Number m_max;

    explicit ZeroedExpParamGeneric(Number base)
        : m_base(base)
        , m_log2Base(std::log2(base))
        , m_expParam(Vector::Splat(0))
        , m_baseParam(Vector::Splat(0))
        , m_max(1)
    {
    }

    ZeroedExpParamGeneric()
        : ZeroedExpParamGeneric(static_cast<Number>(20))
    {
    }

    void SetBaseByCenter(Number center)
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
        Number discriminant = 1 - 4 * center * (1 - center);
        Number sqrt_base = (1 + std::sqrt(discriminant)) / (2 * center);
        m_base = sqrt_base * sqrt_base;
        m_log2Base = std::log2(m_base);
        m_expParam = Evaluate(m_base, m_log2Base, m_baseParam);
    }

    void SetMax(Number max)
    {
        m_max = max;
    }

    // (base^value - 1) / (base - 1), computed with base^value as
    // 2^(value * log2(base)) through the fast vector Exp2.
    //
    static Vector Evaluate(Number base, Number log2Base, const Vector& value)
    {
        return ((value * log2Base).Exp2() - static_cast<Number>(1)) / (base - 1);
    }

    static float Compute(float base, float value)
    {
        using Mono = MultiChannelNumber<float, 1>;
        return ((Mono(value) * std::log2(base)).Exp2() - 1.0f) / (base - 1);
    }

    static Vector Compute(Number base, const Vector& value)
    {
        return Evaluate(base, std::log2(base), value);
    }

    Vector Update(const Vector& value)
    {
        if (m_baseParam != value)
        {
            m_baseParam = value;
            m_expParam = Evaluate(m_base, m_log2Base, value) * m_max;
        }

        return m_expParam;
    }
};

using ZeroedExpParam = ZeroedExpParamGeneric<float, 1>;


} // namespace PhaseUtils
