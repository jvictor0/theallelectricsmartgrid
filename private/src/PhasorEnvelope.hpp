#pragma once
#include <cstddef>
#include <cmath>
#include "plugin.hpp"
#include "Slew.hpp"

struct PhasorEnvelopeInternal
{
    enum class State
    {
        Off,
        Phasor,
        Release
    };

    float m_preEnvelope;
    size_t m_phasorFrames;
    size_t m_lastPhasorFrames;
    float m_phasorEndValue;
    State m_state;
    bool m_inPhasor;
    FixedSlew m_slew;

    PhasorEnvelopeInternal()
        : m_preEnvelope(0)
        , m_phasorFrames(0)
        , m_lastPhasorFrames(0)
        , m_state(State::Off)
        , m_inPhasor(false)
        , m_slew(1.0/128)
    {
    }
    
    struct Input
    {
        float m_gateFrac;
        float m_attackFrac;
        float m_attackShape;
        float m_decayShape;
        float m_in;

        Input()
            : m_gateFrac(0.5)
            , m_attackFrac(0.05)
            , m_attackShape(0)
            , m_decayShape(0)
        {
        }

        float ComputePhase(bool* attack)
        {
            *attack = false;
            if (m_gateFrac <= 0)
            {
                return 0;
            }
            
            float in = m_in / m_gateFrac;
            if (in >= 1 || in <= 0)
            {
                return 0;
            }

            // Clamp attackFrac to avoid division by zero at extremes
            //
            float safeAttackFrac = std::max(0.001f, std::min(0.999f, m_attackFrac));

            if (in <= safeAttackFrac)
            {
                *attack = true;
                return in / safeAttackFrac;
            }
            else
            {
                float release = 1 - (in - safeAttackFrac) / (1 - safeAttackFrac);
                return release;
            }
        }
    };

    float Shape(float shape, float in)
    {
        if (shape == 0)
        {
            return in;
        }
        else
        {
            return powf(in, powf(10, -shape));
        }
    }

    float UnShape(float shape, float in)
    {
        if (shape == 0)
        {
            return in;
        }
        else
        {
            return powf(in, 1 / powf(10, -shape));
        }
    }

    float ComputeRelease(Input& input)
    {
        if (m_state == State::Release && input.m_gateFrac > 1)
        {
            float delta = m_phasorEndValue / ((input.m_gateFrac - 1) * m_lastPhasorFrames);
            return std::max(0.0f, m_preEnvelope - delta);
        }

        return 0;
    }
    
    float Process(Input& input)
    {
        if (m_state == State::Phasor && (input.m_in <= 0 || input.m_in >= 1) && input.m_gateFrac > 1)
        {
            m_state = State::Release;
            if (input.m_gateFrac * input.m_attackFrac > 1)
            {
                m_preEnvelope = UnShape(input.m_decayShape, Shape(input.m_attackShape, m_preEnvelope));
            }

            m_phasorEndValue = m_preEnvelope;
            m_lastPhasorFrames = m_phasorFrames;
        }
        
        float releaseEnvelope = ComputeRelease(input);
        bool attack = false;
        if (input.m_in > 0 && input.m_in < 1)
        {
            if (!m_inPhasor)
            {
                m_inPhasor = true;
                m_phasorFrames = 0;
            }

            ++m_phasorFrames;
            
            float phase = input.ComputePhase(&attack);
            if (m_state == State::Release && attack)
            {
                float shapedRelease = Shape(input.m_decayShape, releaseEnvelope);
                float shapedPhase = Shape(input.m_attackShape, phase);
                if (shapedRelease <= shapedPhase)
                {
                    m_state = State::Phasor;
                    m_preEnvelope = phase;
                    return shapedPhase;
                }
                else
                {
                    m_preEnvelope = releaseEnvelope;
                    return shapedRelease;
                }
            }
            else if (releaseEnvelope <= phase)
            {
                if (m_state != State::Phasor)
                {
                    m_state = State::Phasor;
                }

                m_preEnvelope = phase;
            }
            else
            {
                m_preEnvelope = releaseEnvelope;
            }
        }
        else if (releaseEnvelope > 0)
        {
            m_inPhasor = false;
            m_preEnvelope = releaseEnvelope;
        }
        else
        {
            m_inPhasor = false;
            m_state = State::Off;
            m_preEnvelope = 0;
            return 0;
        }
            
        return m_slew.Process(Shape(attack ? input.m_attackShape : input.m_decayShape, m_preEnvelope));
    }
};

