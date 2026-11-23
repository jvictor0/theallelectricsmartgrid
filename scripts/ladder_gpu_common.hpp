// Research benchmark types shared by the Metal shader and its CPU reference.
// Production saturation formula: private/src/Saturator.hpp, vector Tanh.
//
#pragma once

#ifdef __METAL_VERSION__
#include <metal_stdlib>
using namespace metal;
#define LADDER_THREAD thread
#else
#include <cmath>
#include <algorithm>
using std::fma;
using std::fabs;
using std::fmax;
using std::fmin;
#define LADDER_THREAD
#endif

namespace LadderGPU
{
struct Parameters
{
    float m_alpha;
    float m_decay;
    float m_feedback;
    float m_gain;
    float m_tanhGain;
};

struct History
{
    float m_a;
    float m_b;
    float m_c;
    float m_d;
};

struct Partial
{
    float m_energy;
    float m_real;
    float m_imaginary;
    float m_dc;
    float m_peak;
};

struct Dispatch
{
    unsigned m_count;
    unsigned m_begin;
    unsigned m_end;
    unsigned m_synthetic;
};

inline float RationalTanh(float x)
{
    const float y = x*(x*x+27.0f)/fma(x*9.0f, x, 27.0f);
    return fmin(1.0f, fmax(-1.0f, y));
}

inline float Step(LADDER_THREAD History& state, Parameters p, float input)
{
    state.m_a = fma(p.m_alpha, input-p.m_feedback*state.m_d, p.m_decay*state.m_a);
    state.m_b = fma(p.m_alpha, RationalTanh(p.m_gain*state.m_a)/p.m_tanhGain, p.m_decay*state.m_b);
    state.m_c = fma(p.m_alpha, RationalTanh(p.m_gain*state.m_b)/p.m_tanhGain, p.m_decay*state.m_c);
    state.m_d = fma(p.m_alpha, RationalTanh(p.m_gain*state.m_c)/p.m_tanhGain, p.m_decay*state.m_d);
    return (1.0f+p.m_feedback)*(RationalTanh(p.m_gain*state.m_d)/p.m_tanhGain);
}

// Compensated float accumulation prevents long sums from losing small terms.
// The CPU combines completed segments in double precision.
//
struct Sum
{
    float m_value = 0;
    float m_error = 0;

    void Add(float value)
    {
        const float adjusted = value-m_error;
        const float next = m_value+adjusted;
        m_error = (next-m_value)-adjusted;
        m_value = next;
    }
};

struct Measurement
{
    Sum m_energy;
    Sum m_real;
    Sum m_imaginary;
    Sum m_dc;
    float m_peak = 0;

    void Add(float output, float input, float quadrature)
    {
        m_energy.Add(output*output);
        m_real.Add(output*input);
        m_imaginary.Add(-output*quadrature);
        m_dc.Add(output);
        m_peak = fmax(m_peak, fabs(output));
    }

    Partial Get() const
    {
        return {m_energy.m_value, m_real.m_value, m_imaginary.m_value, m_dc.m_value, m_peak};
    }
};
}
#undef LADDER_THREAD
