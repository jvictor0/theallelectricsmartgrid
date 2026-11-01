#pragma once

#include "Filter.hpp"
#include "Saturator.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "SampleTimer.hpp"

#include "TheoryOfTimeBase.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

// Crossfades the theory-of-time loops for Size voices at once. The loops are
// shared, so a loop is evaluated for every lane whenever any lane weights it.
//
template <size_t Size>
struct PolyXFaderGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;
    static constexpr size_t x_maxSize = 16;

    struct Input
    {
        TheoryOfTimeBase* m_theoryOfTime;
        PhaseDomain m_phaseDomain;
        float m_samplePosition;

        Vector m_attackFrac;
        Vector m_shape;
        Vector m_mult;
        Vector m_amplitude;

        Vector m_phaseShift;

        size_t m_size;
        Vector m_slope;
        Vector m_center;

        Vector m_externalWeights[x_maxSize];

        bool m_trig[Size];
        Vector m_shFade;

        Input()
            : m_theoryOfTime(nullptr)
            , m_phaseDomain(PhaseDomain::Modulated)
            , m_samplePosition(0.0f)
            , m_attackFrac()
            , m_shape()
            , m_mult()
            , m_amplitude(Vector::Splat(1))
            , m_phaseShift()
            , m_size(0)
            , m_slope()
            , m_center()
            , m_trig{}
            , m_shFade()
        {
            for (size_t i = 0; i < x_maxSize; ++i)
            {
                m_externalWeights[i] = Vector::Splat(1);
            }
        }

        Vector ComputePhase(size_t i) const
        {
            double loopPhase = m_theoryOfTime->GetPhase(i, m_samplePosition, m_phaseDomain);
            DoubleVector phase = DoubleVector::Splat(loopPhase) + m_phaseShift.template Convert<double>() + 0.75;
            Vector t = (phase - phase.Floor()).template Convert<float>() * m_mult;
            Vector floorMult = m_mult.Floor();

            // Past the last whole cycle the partial cycle is stretched to full
            // length and scaled down by its fraction.
            //
            Vector partialAmp = m_mult - floorMult;
            Vector partialT = (t - floorMult) / partialAmp;
            Vector wholeT = t - t.Floor();
            Vector amp = Vector::IfLess(floorMult, t, partialAmp, Vector::Splat(1));
            t = Vector::IfLess(floorMult, t, partialT, wholeT);

            // Clamp attackFrac to avoid division by zero at extremes
            //
            Vector safeAttackFrac = Vector::Max(Vector::Splat(0.001f), Vector::Min(Vector::Splat(0.999f), m_attackFrac));
            Vector rise = t / safeAttackFrac;
            Vector decay = Vector::Splat(1) - (t - safeAttackFrac) / (Vector::Splat(1) - safeAttackFrac);
            return amp * Shape(m_shape, Vector::IfLess(safeAttackFrac, t, decay, rise));
        }

        static Vector Shape(const Vector& shape, const Vector& in)
        {
            static constexpr float x_maxShapeDrive = 8.0f;
            using Saturator = TanhSaturatorGeneric<float, Size, true>;

            Vector sine = (Vector::Splat(1) - Math::Cos2pi(in * 0.5f)) * 0.5f;
            Vector towardSine = in + shape * 2 * (sine - in);

            Vector drive = (shape * 2 - 1) * x_maxShapeDrive;
            Vector saturated = (Saturator::Tanh(drive * (sine * 2 - 1)) / Saturator::Tanh(drive) + 1) * 0.5f;

            Vector shaped = Vector::IfLess(Vector::Splat(0.5f), shape, saturated, towardSine);
            return Vector::IfLess(Vector(), shape, shaped, in);
        }
    };

    PolyXFaderGeneric()
        : m_size(0)
        , m_amplitude(Vector::Splat(1))
        , m_slope()
        , m_center()
        , m_values{}
        , m_weights{}
        , m_totalWeight()
        , m_mixSlewFreq()
        , m_rawOutput()
        , m_output()
        , m_top{}
        , m_shValue()
    {
        m_shSlew.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(500.0));
    }

    size_t m_size;
    Vector m_amplitude;
    Vector m_slope;
    Vector m_center;
    Vector m_values[x_maxSize];
    Vector m_weights[x_maxSize];
    Vector m_totalWeight;
    OPLowPassFilterGeneric<float, Size> m_mixSlew;
    Vector m_mixSlewFreq;
    OPLowPassFilterGeneric<float, Size> m_shSlew;
    Vector m_rawOutput;
    Vector m_output;
    SampleTop m_top[Size];
    Vector m_shValue;

    static float ComputeWeight(size_t index, size_t size, float centerParam, float slopeParam)
    {
        float center = centerParam * size;
        float preSlope = slopeParam * size;
        float slope = preSlope < 0.5 ? 2 : 1.0 / preSlope;
        if (center < 0.25 && index == 0)
        {
            return 1.0f;
        }
        else if (center >= size - 0.25 && index == size - 1)
        {
            return 1.0f;
        }
        else if (index - 0.25 < center && center < index + 0.25)
        {
            return 1.0f;
        }
        else if (index < center)
        {
            return std::max<float>(0, 1.0f - (center - index - 0.25) * slope);
        }
        else
        {
            return std::max<float>(0, 1.0f - (index - 0.25 - center) * slope);
        }
    }

    static bool AnyNonZero(const Vector& values)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            if (values[lane] != 0)
            {
                return true;
            }
        }

        return false;
    }

    void Process(Input& input)
    {
        assert(input.m_size <= x_maxSize);
        if (m_size != input.m_size || m_slope != input.m_slope || m_center != input.m_center)
        {
            m_size = input.m_size;
            m_slope = input.m_slope;
            m_center = input.m_center;
            m_totalWeight = Vector();

            for (size_t i = 0; i < m_size; ++i)
            {
                for (size_t lane = 0; lane < Size; ++lane)
                {
                    m_weights[i][lane] = ComputeWeight(i, m_size, m_center[lane], m_slope[lane]);
                }

                m_totalWeight += m_weights[i];
            }
        }

        if (m_size == 0)
        {
            return;
        }

        for (size_t lane = 0; lane < Size; ++lane)
        {
            m_top[lane] = SampleTop::AndIdentity(m_totalWeight[lane] > 0.0f);
        }

        size_t sampleIndex = static_cast<size_t>(std::floor(input.m_samplePosition));
        size_t topIndex = static_cast<size_t>(std::round(input.m_samplePosition));
        int64_t maxCycleRatio[Size] = {};
        Vector output;
        for (size_t i = 0; i < m_size; ++i)
        {
            if (!AnyNonZero(m_weights[i]))
            {
                continue;
            }

            int64_t cycleRatio = input.m_theoryOfTime->GetCycleRatio(i, sampleIndex);
            SampleTop crossed = input.m_theoryOfTime->CrossedCycleBoundary(i, topIndex, input.m_phaseDomain);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                if (m_weights[i][lane] != 0)
                {
                    maxCycleRatio[lane] = std::max(maxCycleRatio[lane], cycleRatio);
                    m_top[lane] = m_top[lane] && crossed;
                }
            }

            m_values[i] = input.ComputePhase(i);
            output += m_values[i] * m_weights[i] * input.m_externalWeights[i];
        }

        // Avoid division by zero if all weights are zero
        //
        output = Vector::IfLess(Vector(), m_totalWeight, output / m_totalWeight, output);

        Vector slewFreq;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            slewFreq[lane] = static_cast<float>(8.0 * input.m_mult[lane] * maxCycleRatio[lane] / input.m_theoryOfTime->m_globalPeriodSamples);
            assert(slewFreq[lane] >= 0.0f);
        }

        if (slewFreq != m_mixSlewFreq)
        {
            m_mixSlewFreq = slewFreq;

            // A zero slew frequency holds the mix. Stand in a valid cutoff for
            // those lanes so the alpha computation stays in range.
            //
            Vector validFreq = Vector::IfLess(Vector(), slewFreq, slewFreq, Vector::Splat(0.25f));
            m_mixSlew.m_alpha = Vector::IfLess(Vector(), slewFreq, OPLowPassFilterGeneric<float, Size>::AlphaFromNatFreq(validFreq), Vector());
        }

        output = m_mixSlew.Process(output);
        m_amplitude = input.m_amplitude;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            if (input.m_trig[lane])
            {
                m_shValue[lane] = output[lane];
            }
        }

        Vector held = m_shSlew.Process(m_shValue);
        m_rawOutput = held * input.m_shFade + output * (Vector::Splat(1) - input.m_shFade);
        m_output = m_rawOutput * input.m_amplitude + (Vector::Splat(1) - input.m_amplitude) * 0.5f;
    }
};

using PolyXFaderInternal = PolyXFaderGeneric<1>;
using TrioPolyXFader = PolyXFaderGeneric<3>;
