#pragma once
#include <cstddef>
#include <cmath>
#include "plugin.hpp"
#include "Filter.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "MorphingWaveTable.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "SampleTop.hpp"

// Each channel reads its own pair of wavetables; the phase distortion runs
// across all channels at once.
//
template <size_t Size>
struct VectorPhaseShaperGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    MorphingWaveTableGeneric<float, Size> m_morphingWaveTable;
    Vector m_phase;
    Vector m_freq;
    Vector m_vKnob;
    Vector m_vTarget;
    OPLowPassFilterGeneric<float, Size> m_v;
    Vector m_dKnob;
    Vector m_dTarget;
    OPLowPassFilterGeneric<float, Size> m_d;
    Vector m_out;
    Vector m_dScale;
    Vector m_phaseMod;
    SampleTop m_top[Size];

    struct Input
    {
        Vector m_freq;
        Vector m_v;
        Vector m_d;
        Vector m_phaseMod;
        Vector m_morphHarmonics;
        Vector m_wtBlend;
        float m_maxFreq;

        Input()
            : m_freq(Vector::Splat(0))
            , m_v(Vector::Splat(0.5))
            , m_d(Vector::Splat(0.5))
            , m_phaseMod(Vector::Splat(0))
            , m_morphHarmonics(Vector::Splat(1))
            , m_wtBlend(Vector::Splat(0))
            , m_maxFreq(SampleTimer::HzToCycPerSamp(19200.0))
        {
        }
    };

    void Process(const Input& input)
    {
        m_freq = input.m_freq;
        SetDScale();
        SetDV(input.m_d, input.m_v);
        m_phaseMod = input.m_phaseMod;
        UpdatePhase();
        Evaluate(input);
    }

    VectorPhaseShaperGeneric()
        : m_phase(Vector::Splat(0))
        , m_freq(Vector::Splat(0))
        , m_vKnob(Vector::Splat(NAN))
        , m_vTarget(Vector::Splat(0))
        , m_dKnob(Vector::Splat(NAN))
        , m_dTarget(Vector::Splat(0))
        , m_out(Vector::Splat(0))
        , m_dScale(Vector::Splat(1))
        , m_phaseMod(Vector::Splat(0))
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
        }

        m_v.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(25.0));
        m_d.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(25.0));
        SetDV(Vector::Splat(0.5), Vector::Splat(0.5));
    }

    // 4 * freq / d < 0.5, so 8 * freq < d: dScale = max(0, 1 - 16 * freq)
    //
    void SetDScale()
    {
        m_dScale = Vector::Max(Vector::Splat(0), Vector::Splat(1) - m_freq * 16);
    }

    void SetDV(const Vector& d, const Vector& v)
    {
        if (d != m_dKnob)
        {
            m_dKnob = d;
            m_dTarget = (Vector::Splat(1) - Math::Cos2pi(d * 0.5)) * 0.5;
        }

        if (v != m_vKnob)
        {
            m_vKnob = v;
            m_vTarget = v * 4;
        }

        m_d.Process(m_dTarget);
        m_v.Process(m_vTarget);
    }

    void UpdatePhase()
    {
        m_phase += m_freq;
        Vector wrapped = m_phase - m_phase.Floor();
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
            if (1 <= m_phase[i])
            {
                m_top[i] = SampleTop::FromWrap(wrapped[i], m_freq[i]);
            }
        }

        m_phase = Vector::IfLess(m_phase, Vector::Splat(1), m_phase, wrapped);
    }

    // Quintic Hermite segment on [a, b] with zero second derivatives at both ends.
    //
    static Vector HermiteQuinticSegmentC2Zero(
        const Vector& a,
        const Vector& b,
        const Vector& yA,
        const Vector& mA,
        const Vector& yB,
        const Vector& mB,
        const Vector& t)
    {
        Vector h = b - a;
        Vector s = (t - a) / h;
        Vector slope = h * mA;
        Vector r1 = yB - (yA + slope);
        Vector r2 = h * mB - slope;

        Vector d = r1 * 10 - r2 * 4;
        Vector e = r2 * 7 - r1 * 15;
        Vector f = r1 * 6 - r2 * 3;

        return yA + s * (slope + s * s * (d + s * (e + s * f)));
    }

    // Two straight segments through (0, 0), (d, v) and (1, 1), joined by C2
    // quintic elbows of width 2 * delta at d and across the cycle wrap. The
    // elbows never overlap, so one quintic evaluates whichever elbow t is in.
    //
    static Vector PhiVPSQuinticElbow(const Vector& t, const Vector& v, const Vector& d)
    {
        Vector one = Vector::Splat(1);
        Vector slopeLeft = v / d;
        Vector slopeRight = (one - v) / (one - d);
        Vector delta = Vector::Min(d, one - d) * 0.25;

        Vector leftB = delta;
        Vector midA = d - delta;
        Vector midB = d + delta;
        Vector rightA = one - delta;

        Vector lineLeft = slopeLeft * t;
        Vector lineRight = v + slopeRight * (t - d);

        // Left elbow: R(1) - 1 at 0 to L(delta)
        // Middle elbow: L(d - delta) to R(d + delta)
        // Right elbow: R(1 - delta) to L(0) + 1 = 1 at 1
        //
        Vector midYA = slopeLeft * midA;
        Vector midYB = v + slopeRight * (midB - d);
        Vector rightYA = v + slopeRight * (rightA - d);
        Vector leftYA = v + slopeRight * (one - d) - 1;
        Vector leftYB = slopeLeft * leftB;

        Vector a = Vector::IfLess(midB, t, rightA, midA);
        Vector b = Vector::IfLess(midB, t, one, midB);
        Vector yA = Vector::IfLess(midB, t, rightYA, midYA);
        Vector yB = Vector::IfLess(midB, t, one, midYB);
        Vector mA = Vector::IfLess(midB, t, slopeRight, slopeLeft);
        Vector mB = Vector::IfLess(midB, t, slopeLeft, slopeRight);

        a = Vector::IfLess(leftB, t, a, Vector::Splat(0));
        b = Vector::IfLess(leftB, t, b, leftB);
        yA = Vector::IfLess(leftB, t, yA, leftYA);
        yB = Vector::IfLess(leftB, t, yB, leftYB);
        mA = Vector::IfLess(leftB, t, mA, slopeRight);
        mB = Vector::IfLess(leftB, t, mB, slopeLeft);

        Vector elbow = HermiteQuinticSegmentC2Zero(a, b, yA, mA, yB, mB, t);

        Vector result = Vector::IfLess(t, rightA, lineRight, elbow);
        result = Vector::IfLess(midB, t, result, elbow);
        result = Vector::IfLess(t, midA, lineLeft, result);
        return Vector::IfLess(leftB, t, result, elbow);
    }

    void Evaluate(const Input& input)
    {
        Vector d = (m_d.m_output - 0.5) * m_dScale + 0.5;
        Vector phase = m_phase + m_phaseMod;
        phase = phase - phase.Floor();
        Vector phi = PhiVPSQuinticElbow(phase, m_v.m_output, d);

        // A tiny negative phase can round to exactly one after adding its
        // integer wrap. One cycle is the same phase as zero.
        //
        phi = phi - phi.Floor();
        phi = Vector::IfLess(phi, Vector::Splat(1), phi, Vector::Splat(0));

        for (size_t i = 0; i < Size; ++i)
        {
            assert(0 <= phi[i]);
            assert(phi[i] < 1);
        }

        m_out = -m_morphingWaveTable.Evaluate(phi, m_freq, input.m_morphHarmonics, input.m_wtBlend);
    }

    AdaptiveWaveTable* SetLeft(size_t channel, AdaptiveWaveTable* left)
    {
        return m_morphingWaveTable.SetLeft(channel, left);
    }

    AdaptiveWaveTable* SetRight(size_t channel, AdaptiveWaveTable* right)
    {
        return m_morphingWaveTable.SetRight(channel, right);
    }

    AdaptiveWaveTable* SetLeft(AdaptiveWaveTable* left)
    {
        static_assert(Size == 1, "SetLeft without a channel requires one channel");
        return SetLeft(0, left);
    }

    AdaptiveWaveTable* SetRight(AdaptiveWaveTable* right)
    {
        static_assert(Size == 1, "SetRight without a channel requires one channel");
        return SetRight(0, right);
    }
};

using VectorPhaseShaperInternal = VectorPhaseShaperGeneric<1>;
using TrioVectorPhaseShaper = VectorPhaseShaperGeneric<3>;
