#pragma once

#include "Slew.hpp"
#include "Math.hpp"

struct PolyXFaderInternal
{    
    struct Input
    {
        double* m_values;
        bool* m_top;

        float m_attackFrac;
        float m_shape;
        float m_mult;

        float m_phaseShift;

        size_t m_size;
        float m_slope;
        float m_center;

        float m_externalWeights[16];

        Input()
            : m_values(nullptr)
            , m_attackFrac(0.0f)
            , m_shape(0.0f)
            , m_mult(0.0f)
            , m_size(0)
            , m_slope(0.0f)
            , m_center(0.0f)
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
            float t = m_values[i] + m_phaseShift + 0.75;
            t = (t - std::floorf(t)) * mult;
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
            if (shape < 0.45)
            {
                shape *= 2;
                float fullShape = (- Math::Cos2pi(in / 2) + 1) / 2;
                return in * shape + fullShape * (1 - shape);
            }
            else if (shape < 0.55)
            {
                return in;
            }
            else
            {
                return in;
            }
        }
    };

    PolyXFaderInternal()
        : m_size(0)
        , m_slope(0.0f)
        , m_center(0.0f)
        , m_slew(1.0/128)
        , m_output(0.0f)
    {
        for (size_t i = 0; i < 16; ++i)
        {
            m_valuesPreQuantize[i] = 0;
            m_valuesPostQuantize[i] = 0;
            m_weights[i] = 0;
        }
    }
    
    size_t m_size;
    float m_slope;
    float m_center;
    float m_valuesPreQuantize[16];
    float m_valuesPostQuantize[16];
    float m_weights[16];
    float m_totalWeight;
    FixedSlew m_slew;
    float m_output;    
    bool m_top;

    float ComputeWeight(size_t index)
    {
        float center = m_center * m_size;
        float preSlope = m_slope * m_size;
        float slope = preSlope < 0.5 ? 2 : 1.0 / preSlope;
        if (center < 0.25 && index == 0)
        {
            return 1.0f;
        }
        else if (center > m_size - 0.25 && index == m_size - 1)
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
        
        m_top = true;
        for (size_t i = 0; i < m_size; ++i)
        {
            if (m_weights[i] != 0)
            {
                m_top = m_top && input.m_top[i];
                m_valuesPostQuantize[i] = Quantize(input, i, input.ComputePhase(i));
                output += m_valuesPostQuantize[i] * m_weights[i] * input.m_externalWeights[i];
            }
        }

        // Avoid division by zero if all weights are zero
        //
        if (m_totalWeight > 0.0f)
        {
            output /= m_totalWeight;
        }

        m_output = m_slew.Process(output);
    }

    float Quantize(Input& input, size_t i, float value)
    {
        if (input.m_shape <= 0.55)
        {
            m_valuesPreQuantize[i] = value;
            return value;
        }
        else
        {
            int numBits = std::max<int>(0, std::round(16 * (1 - input.m_shape)));
            float thisQuanized = std::round(value * (1 << numBits)) / (1 << numBits);
            float oldQuanized = std::round(m_valuesPreQuantize[i] * (1 << numBits)) / (1 << numBits);
            if (thisQuanized == oldQuanized)
            {
                return m_valuesPostQuantize[i];
            }
            else
            {
                m_valuesPreQuantize[i] = value;
                return thisQuanized;
            }
        }   
    }
};
