#pragma once
#include "Slew.hpp"
#include "Filter.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "Saturator.hpp"

template <size_t Size>
struct LissajousLFOGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using Saturator = TanhSaturatorGeneric<float, Size, false>;

    struct Input
    {
        static constexpr float x_maxRadius = 5.0f;
        static constexpr float x_centerRadius = 1.0f;

        Vector m_multX;
        Vector m_multY;

        Vector m_centerX;
        Vector m_centerY;

        Vector m_phaseShift;

        PhaseUtils::ZeroedExpParamGeneric<float, Size> m_radius;

        Input()
            : m_multX(Vector::Splat(1))
            , m_multY(Vector::Splat(1))
            , m_centerX(Vector::Splat(0))
            , m_centerY(Vector::Splat(0))
            , m_phaseShift(Vector::Splat(0))
        {
            m_radius.SetMax(x_maxRadius);
            m_radius.SetBaseByCenter(x_centerRadius / x_maxRadius);
            m_radius.Update(Vector::Splat(0.5f));
        }

        // Past the last whole cycle the partial cycle is stretched to full
        // length and scaled down by its fraction.
        //
        static Vector Wrap(const Vector& t, const Vector& mult, Vector& amp)
        {
            Vector floorMult = mult.Floor();
            Vector partialAmp = mult - floorMult;
            amp = Vector::IfLess(floorMult, t, partialAmp, Vector::Splat(1));
            return Vector::IfLess(floorMult, t, (t - floorMult) / partialAmp, t - t.Floor());
        }

        void Compute(const Vector& phase, Vector& outX, Vector& outY) const
        {
            Vector t = phase + m_phaseShift;
            Vector shiftedX = t + 0.25f;
            Vector ampX;
            Vector ampY;
            Vector tx = Wrap((shiftedX - shiftedX.Floor()) * m_multX, m_multX, ampX);
            Vector ty = Wrap((t - t.Floor()) * m_multY, m_multY, ampY);

            Vector rad = m_radius.m_expParam;
            Vector centerWeight = Vector::Splat(1) - rad / x_maxRadius;
            outX = Saturator::Tanh(rad * ampX * Math::Sin2pi(tx) + centerWeight * m_centerX);
            outY = Saturator::Tanh(rad * ampY * Math::Sin2pi(ty) + centerWeight * m_centerY);
        }
    };

    LissajousLFOGeneric()
        : m_outputX(Vector::Splat(0))
        , m_outputY(Vector::Splat(0))
    {
    }

    Vector m_outputX;
    Vector m_outputY;

    void Process(const Vector& in, const Input& input)
    {
        input.Compute(in, m_outputX, m_outputY);
    }
};

using LissajousLFOInternal = LissajousLFOGeneric<1>;
using TrioLissajousLFO = LissajousLFOGeneric<3>;
