#pragma once

#include "Filter.hpp"
#include "PolyXFaderTables.hpp"
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

        struct Prepared
        {
            DoubleVector m_phaseShift;
            Vector m_floorMult;
            Vector m_partialAmp;
            Vector m_inversePartial;
            Vector m_attack;
            Vector m_inverseAttack;
            Vector m_inverseDecay;

            explicit Prepared(const Input& input)
            {
                m_phaseShift = input.m_phaseShift.template Convert<double>();
                m_floorMult = input.m_mult.Floor();
                m_partialAmp = input.m_mult - m_floorMult;

                // Integer multipliers have no partial lobe; keep its unused
                // reciprocal finite.
                //
                Vector denominator = Vector::IfLess(Vector(), m_partialAmp, m_partialAmp, Vector::Splat(1));
                m_inversePartial = Vector::Splat(1) / denominator;
                m_attack = Vector::Max(Vector::Splat(0.001f), Vector::Min(Vector::Splat(0.999f), input.m_attackFrac));
                m_inverseAttack = Vector::Splat(1) / m_attack;
                m_inverseDecay = Vector::Splat(1) / (Vector::Splat(1) - m_attack);
            }
        };

        Vector ComputePhase(size_t i) const
        {
            return ComputePhase(i, Prepared(*this));
        }

        Vector ComputePhase(size_t i, const Prepared& prepared) const
        {
            double loopPhase = m_theoryOfTime->GetPhase(i, m_phaseDomain);
            DoubleVector phase = DoubleVector::Splat(loopPhase) + prepared.m_phaseShift + 0.75;
            Vector t = (phase - phase.Floor()).template Convert<float>() * m_mult;
            Vector partialT = (t - prepared.m_floorMult) * prepared.m_inversePartial;
            Vector wholeT = t - t.Floor();
            Vector amp = Vector::IfLess(prepared.m_floorMult, t, prepared.m_partialAmp, Vector::Splat(1));
            t = Vector::IfLess(prepared.m_floorMult, t, partialT, wholeT);
            Vector rise = t * prepared.m_inverseAttack;
            Vector decay = Vector::Splat(1) - (t - prepared.m_attack) * prepared.m_inverseDecay;
            return amp * Shape(m_shape, Vector::IfLess(prepared.m_attack, t, decay, rise));
        }

        // Morph linear -> sine -> the fixed, gain-8 saturated sine.
        //
        static Vector Shape(const Vector& shape, const Vector& in)
        {
            Vector sine;
            Vector hard;
            PolyXFaderTables::Evaluate(in, sine, hard);
            Vector a = Vector::Min(Vector::Splat(1), Vector::Max(Vector(), shape * 2));
            Vector b = Vector::Min(Vector::Splat(1), Vector::Max(Vector(), shape * 2 - 1));
            return in * (Vector::Splat(1) - a) + sine * (a - b) + hard * b;
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
    // Double state: slow loops give mix slews whose coefficients are below
    // float resolution at the engine rate.
    //
    OPLowPassFilterGeneric<double, Size> m_mixSlew;
    DoubleVector m_mixSlewFreq;
    OPLowPassFilterGeneric<float, Size> m_shSlew;
    Vector m_rawOutput;
    Vector m_output;
    SampleTop m_top[Size];
    Vector m_shValue;

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

            Vector center = m_center * static_cast<float>(m_size);
            Vector preSlope = m_slope * static_cast<float>(m_size);
            Vector safeSlope = Vector::Max(preSlope, Vector::Splat(0.5f));
            Vector slope = Vector::IfLess(preSlope, Vector::Splat(0.5f), Vector::Splat(2), Vector::Splat(1) / safeSlope);
            for (size_t i = 0; i < m_size; ++i)
            {
                Vector distance = (Vector::Splat(static_cast<float>(i)) - center).Abs();
                Vector weight = Vector::Max(Vector(), Vector::Splat(1) - Vector::Max(Vector(), distance - 0.25f) * slope);
                if (i == 0)
                {
                    weight = Vector::IfLess(center, Vector::Splat(0.25f), Vector::Splat(1), weight);
                }

                if (i + 1 == m_size)
                {
                    weight = Vector::IfLess(center, Vector::Splat(static_cast<float>(m_size) - 0.25f), weight, Vector::Splat(1));
                }

                m_weights[i] = weight;
                m_totalWeight += m_weights[i];
            }
        }

        if (m_size == 0)
        {
            return;
        }

        typename Input::Prepared prepared(input);

        for (size_t lane = 0; lane < Size; ++lane)
        {
            m_top[lane] = SampleTop::AndIdentity(m_totalWeight[lane] > 0.0f);
        }

        int64_t maxCycleRatio[Size] = {};
        Vector output;
        for (size_t i = 0; i < m_size; ++i)
        {
            if (!AnyNonZero(m_weights[i]))
            {
                continue;
            }

            int64_t cycleRatio = input.m_theoryOfTime->GetCycleRatio(i);
            SampleTop crossed = input.m_theoryOfTime->CrossedCycleBoundary(i, input.m_phaseDomain);
            for (size_t lane = 0; lane < Size; ++lane)
            {
                if (m_weights[i][lane] != 0)
                {
                    maxCycleRatio[lane] = std::max(maxCycleRatio[lane], cycleRatio);
                    m_top[lane] = m_top[lane] && crossed;
                }
            }

            m_values[i] = input.ComputePhase(i, prepared);
            output += m_values[i] * m_weights[i] * input.m_externalWeights[i];
        }

        // Avoid division by zero if all weights are zero
        //
        output = Vector::IfLess(Vector(), m_totalWeight, output / m_totalWeight, output);

        DoubleVector slewFreq;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            slewFreq[lane] = 8.0 * input.m_mult[lane] * maxCycleRatio[lane] / input.m_theoryOfTime->m_globalPeriodSamples;
            assert(slewFreq[lane] >= 0.0);
        }

        if (slewFreq != m_mixSlewFreq)
        {
            m_mixSlewFreq = slewFreq;

            // A zero slew frequency holds the mix. Stand in a valid cutoff for
            // those lanes so the alpha computation stays in range.
            //
            DoubleVector validFreq = DoubleVector::IfLess(DoubleVector(), slewFreq, slewFreq, DoubleVector::Splat(0.25));
            // Compute alpha directly in float SIMD, then retain double state.
            // Unlike 1 - exp(-omega), this preserves tiny positive coefficients.
            //
            Vector alpha = OPLowPassFilterGeneric<float, Size>::AlphaFromNatFreqApprox(validFreq.template Convert<float>());
            m_mixSlew.m_alpha = DoubleVector::IfLess(DoubleVector(), slewFreq, alpha.template Convert<double>(), DoubleVector());
        }

        output = m_mixSlew.Process(output.template Convert<double>()).template Convert<float>();
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
