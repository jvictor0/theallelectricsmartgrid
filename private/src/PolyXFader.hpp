#pragma once

#include "Filter.hpp"
#include "Math.hpp"

#include "TheoryOfTimeBase.hpp"

struct PolyXFaderInternal
{    
    struct Input
    {
        TheoryOfTimeBase* m_theoryOfTime;
        PhaseDomain m_phaseDomain;
        float m_samplePosition;

        float m_attackFrac;
        float m_shape;
        float m_mult;
        float m_amplitude;

        float m_phaseShift;

        size_t m_size;
        float m_slope;
        float m_center;

        float m_externalWeights[16];

        bool m_trig;
        float m_shFade;
        TanhSaturator<true> m_shapeSaturator;

        Input()
            : m_theoryOfTime(nullptr)
            , m_phaseDomain(PhaseDomain::Modulated)
            , m_samplePosition(0.0f)
            , m_attackFrac(0.0f)
            , m_shape(0.0f)
            , m_mult(0.0f)
            , m_amplitude(1.0f)
            , m_phaseShift(0.0f)
            , m_size(0)
            , m_slope(0.0f)
            , m_center(0.0f)
            , m_trig(false)
            , m_shFade(0.0f)
            , m_shapeSaturator(0.0f)
        {
            for (size_t i = 0; i < 16; ++i)
            {
                m_externalWeights[i] = 1.0f;
            }
        }

        float ComputePhase(size_t i)
        {
            float mult = m_mult;
            float amp = 1;
            double phase = m_theoryOfTime->GetPhase(i, m_samplePosition, m_phaseDomain) + m_phaseShift + 0.75;
            float t = static_cast<float>(phase - std::floor(phase)) * mult;
            float floorMult = std::floorf(mult);

            if (floorMult < t)
            {
                amp = mult - floorMult;
                t = (t - floorMult) / amp;
            }
            else
            {
                t = t - std::floorf(t);
            }
            
            // Clamp attackFrac to avoid division by zero at extremes
            //
            float safeAttackFrac = std::max(0.001f, std::min(0.999f, m_attackFrac));
            
            if (t <= safeAttackFrac)
            {
                return amp * Shape(m_shape, t / safeAttackFrac);
            }
            else
            {
                float decay = 1 - (t - safeAttackFrac) / (1 - safeAttackFrac);
                return amp * Shape(m_shape, decay);
            }
        }
        
        float Shape(float shape, float in)
        {
            if (shape <= 0.0f)
            {
                return in;
            }

            float sine = (1.0f - Math::Cos2pi(in / 2.0f)) / 2.0f;
            if (shape <= 0.5f)
            {
                return in + 2.0f * shape * (sine - in);
            }

            constexpr float x_maxShapeDrive = 8.0f;
            m_shapeSaturator.SetInputGain(x_maxShapeDrive * (2.0f * shape - 1.0f));
            return (1.0f + m_shapeSaturator.Process(2.0f * sine - 1.0f)) / 2.0f;
        }
    };

    PolyXFaderInternal()
        : m_size(0)
        , m_amplitude(1.0f)
        , m_slope(0.0f)
        , m_center(0.0f)
        , m_values{}
        , m_weights{}
        , m_totalWeight(0.0f)
        , m_mixSlewFreq(0.0f)
        , m_rawOutput(0.0f)
        , m_output(0.0f)
        , m_top(false)
        , m_shValue(0.0f)
    {
        m_shSlew.SetAlphaFromNatFreq(500.0f / 48000.0f);
    }
    
    size_t m_size;
    float m_amplitude;
    float m_slope;
    float m_center;
    float m_values[16];
    float m_weights[16];
    float m_totalWeight;
    OPLowPassFilter m_mixSlew;
    float m_mixSlewFreq;
    OPLowPassFilter m_shSlew;
    float m_rawOutput;
    float m_output;
    SampleTop m_top;
    float m_shValue;

    float ComputeWeight(size_t index)
    {
        float center = m_center * m_size;
        float preSlope = m_slope * m_size;
        float slope = preSlope < 0.5 ? 2 : 1.0 / preSlope;
        if (center < 0.25 && index == 0)
        {
            return 1.0f;
        }
        else if (center >= m_size - 0.25 && index == m_size - 1)
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

    void Process(Input& input)
    {
        if (m_size != input.m_size || m_slope != input.m_slope || m_center != input.m_center)
        {
            m_size = input.m_size;
            m_slope = input.m_slope;
            m_center = input.m_center;
            m_totalWeight = 0.0f;

            for (size_t i = 0; i < m_size; ++i)
            {
                m_weights[i] = ComputeWeight(i);
                m_totalWeight += m_weights[i];
            }
        }

        float output = 0.0f;
        if (m_size == 0)
        {
            return;
        }
        
        m_top = SampleTop::AndIdentity(m_totalWeight > 0.0f);
        size_t sampleIndex = static_cast<size_t>(std::floor(input.m_samplePosition));
        int64_t maxCycleRatio = 0;
        for (size_t i = 0; i < m_size; ++i)
        {
            if (m_weights[i] != 0)
            {
                maxCycleRatio = std::max(maxCycleRatio, input.m_theoryOfTime->GetCycleRatio(i, sampleIndex));
                m_top = m_top && input.m_theoryOfTime->CrossedCycleBoundary(i, static_cast<size_t>(std::round(input.m_samplePosition)), input.m_phaseDomain);
                m_values[i] = input.ComputePhase(i);
                output += m_values[i] * m_weights[i] * input.m_externalWeights[i];
            }
        }

        // Avoid division by zero if all weights are zero
        //
        if (m_totalWeight > 0.0f)
        {
            output /= m_totalWeight;
        }

        float slewFreq = static_cast<float>(8.0 * input.m_mult * maxCycleRatio / input.m_theoryOfTime->m_globalPeriodSamples);
        assert(slewFreq >= 0.0f);
        if (slewFreq != m_mixSlewFreq)
        {
            m_mixSlewFreq = slewFreq;
            if (slewFreq > 0.0f)
            {
                m_mixSlew.SetAlphaFromNatFreq(slewFreq);
            }
            else
            {
                m_mixSlew.m_alpha = 0.0f;
            }
        }

        output = m_mixSlew.Process(output);
        m_amplitude = input.m_amplitude;
        if (input.m_trig)
        {
            m_shValue = output;
        }
        
        float held = m_shSlew.Process(m_shValue);
        m_rawOutput = held * input.m_shFade + output * (1 - input.m_shFade);
        m_output = m_rawOutput * input.m_amplitude + (1 - input.m_amplitude) * 0.5;
    }

};
