#pragma once
#include <cstddef>
#include <cmath>
#include "plugin.hpp"
#include "Slew.hpp"
#include "Filter.hpp"
#include "PhaseUtils.hpp"
#include "MorphingWaveTable.hpp"
#include "Math.hpp"

struct VectorPhaseShaperInternal
{
    MorphingWaveTable m_morphingWaveTable;
    float m_phase;
    float m_voct;
    float m_freq;
    float m_vKnob;
    float m_vTarget;
    OPLowPassFilter m_v;
    float m_floorV;
    float m_dKnob;
    float m_dTarget;
    OPLowPassFilter m_d;
    float m_b;
    float m_c;
    float m_out;
    float m_dScale;
    float m_phaseMod;
    bool m_top;

    struct Input
    {
        bool m_useVoct;
        float m_voct;
        float m_freq;
        float m_v;
        float m_d;
        float m_phaseMod;
        float m_morphHarmonics;
        float m_wtBlend;
        float m_maxFreq;

        Input()
            : m_useVoct(true)
            , m_voct(0)
            , m_freq(0)
            , m_v(0.5)
            , m_d(0.5)
            , m_phaseMod(0)
            , m_morphHarmonics(1.0)
            , m_wtBlend(0)
            , m_maxFreq(0.1)
        {
        }
    };

    void Process(Input& input, float deltaT)
    {
        if (input.m_useVoct)
        {
            SetFreq(input.m_voct, deltaT);
        }
        else
        {
            m_freq = input.m_freq;
            SetDScale();
        }

        SetDV(input.m_d, input.m_v);
        m_phaseMod = input.m_phaseMod;
        UpdatePhase();
        Evaluate(input);
    }
    
    VectorPhaseShaperInternal()
        : m_phase(0)
        , m_voct(NAN)
        , m_freq(0)
        , m_floorV(0)
        , m_b(0)
        , m_c(0)
        , m_out(0)
        , m_dScale(1.0f)
        , m_phaseMod(0)
        , m_top(false)
    {
        m_v.SetAlphaFromNatFreq(25.0 / 48000.0);
        m_d.SetAlphaFromNatFreq(25.0 / 48000.0);
        SetDV(0.5, 0.5);
    }

    void SetFreq(float voct, float delta)
    {
        if (voct != m_voct)
        {
            m_voct = voct;
            m_freq = PhaseUtils::VOctToNatural(voct, delta);
        }

        SetDScale();
    }

    void SetDScale()
    {
        // 4 * freq / d < 0.5
        // 8 * freq < d
        // dScale = max(0, 1 - 16 * freq)
        //
        m_dScale = std::max(0.0f, 1 - 16 * m_freq);
    }
    
    void SetDV(float d, float v)
    {
        if (d != m_dKnob)
        {
            m_dKnob = d;
            m_dTarget = (1.0 - Math::Cos2pi(d / 2)) / 2;
        }
        
        if (v != m_vKnob)
        {
            m_vKnob = v;
            m_vTarget = v * 4;
        }

        m_d.Process(m_dTarget);
        float oldV = m_v.m_output;
        m_v.Process(m_vTarget);
        if (oldV != m_v.m_output)
        {
            m_floorV = std::floor(m_v.m_output);
            m_b = m_v.m_output - m_floorV;
            m_c = Math::Cos2pi(m_b);
        }
    }
    
    void UpdatePhase()
    {
        m_top = false;
        m_phase += m_freq;
        while (m_phase >= 1)
        {
            m_top = true;
            m_phase -= 1;
        }
    }

    bool NeedsAntiAlias(float phi_vps)
    {
        if (m_floorV == 0)
        {
            return false;
        }
        else if (m_b < 0.5)
        {
            return m_floorV < phi_vps;
        }
        else
        {
            return m_floorV + 0.5 < phi_vps;
        }
    }

    float HermiteQuinticSegment(float a, float b, float y_a, float m_a, float c_a, float y_b, float m_b, float c_b, float t)
    {
        const float h  = b - a;
        const float s  = (t - a) / h;        // normalized in [0,1]
        const float s2 = s * s;
        const float s3 = s2 * s;
        const float s4 = s3 * s;
        const float s5 = s4 * s;

        // p(s) = A + B s + C s^2 + D s^3 + E s^4 + F s^5
        const float A = y_a;
        const float B = h * m_a;
        const float C = 0.5f * (h * h) * c_a;

        // Solve for D,E,F from end constraints at s = 1
        // System in compact RHS form:
        const float r1 = y_b      - (A + B + C);          // value at s=1
        const float r2 = h * m_b  - (B + 2.0f * C);        // first-deriv at s=1
        const float r3 = (h*h) * c_b - (2.0f * C);         // second-deriv at s=1

        // Elimination (exact, no matrix library):
        // F = 0.5*(r3 + 12 r1 - 6 r2)
        // E = 7 r2 - 15 r1 - r3
        // D = 10 r1 - 4 r2 + 0.5 r3
        const float F = 0.5f * (r3 + 12.0f * r1 - 6.0f * r2);
        const float E = 7.0f * r2 - 15.0f * r1 - r3;
        const float D = 10.0f * r1 - 4.0f * r2 + 0.5f * r3;

        return (((A + B * s + C * s2) + D * s3) + E * s4) + F * s5;
    }

    float HermiteQuinticSegment_C2Zero(float a, float b,
        float y_a, float m_a,
        float y_b, float m_b,
        float t)
    {
        const float h = b - a;
        const float invH = 1.0f / h;
        const float s = (t - a) * invH; // [0,1]

        const float B = h * m_a;

        const float r1 = y_b - (y_a + B);
        const float r2 = h * m_b - B;

        const float D = 10.0f * r1 - 4.0f * r2;
        const float E = 7.0f  * r2 - 15.0f * r1;
        const float F = 6.0f  * r1 - 3.0f * r2;

        const float s2 = s * s;

        // Horner: y = y_a + s*(B + s*(0 + s*(D + s*(E + s*F))))
        //
        return y_a + s * (B + s2 * (D + s * (E + s * F)));
    }

    float PhiVPSQuinticElbow(float phase, float v, float d)
    {
        float x = d;
        float y = v;
        float t = phase;

        // Slopes of the two straight segments
        const float mL = y / x;
        const float mR = (1.0f - y) / (1.0f - x);

        // Straight segments
        auto L = [mL](float tt) -> float
        {
            return mL * tt;
        };
        auto R = [mR, y, x](float tt) -> float
        {
            return y + mR * (tt - x);
        };

        // Elbow width
        const float delta = 0.25f * std::min(x, 1.0f - x);

        // Region boundaries
        const float left_a  = 0.0f;
        const float left_b  = delta;
        const float mid_a   = x - delta;
        const float mid_b   = x + delta;
        const float right_a = 1.0f - delta;
        const float right_b = 1.0f;

        if (t <= left_b)
        {
            // Left elbow: from R(t+1) - 1 at t=0, to L(t) at t=δ
            // Endpoint jets:
            //   at t=0:   y = R(1) - 1,    y' = mR, y'' = 0
            //   at t=δ:   y = L(δ),        y' = mL, y'' = 0
            const float y_a = R(1.0f) - 1.0f;
            const float m_a = mR;
            const float y_b = L(left_b);
            const float m_b = mL;
            return HermiteQuinticSegment_C2Zero(left_a, left_b, y_a, m_a, y_b, m_b, t);
        }

        if (t < mid_a)
        {
            // Left straight section
            return L(t);
        }

        if (t <= mid_b)
        {
            // Middle elbow: L at x-δ  ->  R at x+δ  (C^2)
            const float y_a = L(mid_a), m_a = mL;
            const float y_b = R(mid_b), m_b = mR;
            return HermiteQuinticSegment_C2Zero(mid_a, mid_b, y_a, m_a, y_b, m_b, t);
        }

        if (t < right_a)
        {
            // Right straight section
            return R(t);
        }

        // Right elbow: from R(t) at t=1-δ, to L(t-1) + 1 at t=1
        // Endpoint jets:
        //   at t=1-δ: y = R(1-δ), y' = mR, y'' = 0
        //   at t=1:   y = L(0) + 1 (=1), y' = mL, y'' = 0
        {
            const float y_a = R(right_a), m_a = mR;
            const float y_b = L(0.0f) + 1.0f;
            const float m_b = mL;
            return HermiteQuinticSegment_C2Zero(right_a, right_b, y_a, m_a, y_b, m_b, t);
        }
    }

    float PhiVPSCubicInterpolation(float phase, float v, float d)
    {
        float x = d;
        float y = v;
        float t = phase;

        const double xm1 = x - 1.0;
        const double x2  = x * x;
    
        // Optimal b3 (min ∫ f''^2 with f1''(0) = f2''(1))
        const double b3 = (y - x) / (2.0 * x * xm1 * xm1);
    
        // Segment 1: f1(t) = a1 t + a3 t^3   on [0, x]
        const double a1 = (x2 + x * y - 2.0 * y) / (2.0 * x * xm1);
        const double a3 = (y - x) / (2.0 * x2 * xm1);
    
        // Segment 2: f2(t) = b0 + b1 t + b2 t^2 + b3 t^3   on [x, 1]
        const double b0 = x * (x - y) / (2.0 * xm1 * xm1);
        const double b1 = (x * x * x + x2 * y - 4.0 * x2 + 2.0 * y) / (2.0 * x * xm1 * xm1);
        const double b2 = 3.0 * (x - y) / (2.0 * x * xm1 * xm1);
    
        if (t <= x)
        {
            return a1 * t + a3 * t * t * t;
        }
        else
        {
            const double t2 = t * t;
            const double t3 = t2 * t;
            return b0 + b1 * t + b2 * t2 + b3 * t3;
        }
    }

    void Evaluate(Input& input)
    {
        float d = (m_d.m_output - 0.5) * m_dScale + 0.5;
        float phase = m_phase + m_phaseMod;
        phase = phase - std::floor(phase);
        float phi_vps = PhiVPSQuinticElbow(phase, m_v.m_output, d);

        float maxHarmonics = input.m_morphHarmonics;

        phi_vps = phi_vps - std::floor(phi_vps);
        assert(0 <= phi_vps);
        assert(phi_vps < 1);
        m_out = - m_morphingWaveTable.Evaluate(phi_vps, m_freq, maxHarmonics, input.m_wtBlend);
    }

    void EvaluateOld(Input& input)
    {
        float d = (m_d.m_output - 0.5) * m_dScale + 0.5;
        float phi_vps;
        float phase = m_phase + m_phaseMod;
        phase = phase - std::floor(phase);
        if (phase < d)
        {
            phi_vps = phase * m_v.m_output / d;
        }
        else
        {
            phi_vps = m_v.m_output + (phase - d) * (1 - m_v.m_output) / (1 - d);
        }

        float maxHarmonics = input.m_morphHarmonics;
        
        if (!NeedsAntiAlias(phi_vps))
        {
            phi_vps = fmod(phi_vps, 1);
            m_out = - m_morphingWaveTable.Evaluate(phi_vps, m_freq, maxHarmonics, input.m_wtBlend);
        }
        else
        {
            if (m_b < 0.5)
            {
                float phi_as = fmod(phi_vps, 1) / (2 * m_b);
                float s = - m_morphingWaveTable.Evaluate(phi_as, m_freq, maxHarmonics, input.m_wtBlend);                
                
                float offset = m_morphingWaveTable.StartValue(input.m_wtBlend, m_freq, maxHarmonics);
                
                m_out = (1 - m_c) * (s + offset) / 2 - offset;
            }
            else
            {
                float phi_as = fmod(phi_vps - 0.5, 1) / (2 * (m_b - 0.5)) + 0.5;
                float s = - m_morphingWaveTable.Evaluate(phi_as, m_freq, maxHarmonics, input.m_wtBlend);
                float offset = m_morphingWaveTable.CenterValue(input.m_wtBlend, m_freq, maxHarmonics);            
                m_out = (1 + m_c) * (s + offset) / 2 - offset;                
            }
        }
    }            
    
    AdaptiveWaveTable* SetLeft(AdaptiveWaveTable* left)
    {
        return m_morphingWaveTable.SetLeft(left);
    }
    
    AdaptiveWaveTable* SetRight(AdaptiveWaveTable* right)
    {
        return m_morphingWaveTable.SetRight(right);
    }                
};

