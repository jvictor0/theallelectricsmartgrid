#pragma once

#include "PhaseUtils.hpp"

struct ADSR
{
    enum class State : int
    {
        Idle,
        Attack,
        Decay,
        DecayNoGate,
        Sustain,
        Release
    };
    
    struct Input
    {
        float m_attackIncrement;
        float m_decayIncrement;
        float m_sustainLevel;
        float m_releaseIncrement;

        bool m_gate;
        bool m_trig;

        Input()
            : m_attackIncrement(0.0f)
            , m_decayIncrement(0.0f)
            , m_sustainLevel(0.0f)
            , m_releaseIncrement(0.0f)
            , m_gate(false)
            , m_trig(false)
        {
        }
    };

    struct InputSetter
    {
        static constexpr float x_sampleRate = 48000.0f;
        static constexpr float x_attackTimeMin = 0.001f;
        static constexpr float x_attackTimeMax = 2.5f;
        static constexpr float x_decayTimeMin = 0.01f;
        static constexpr float x_decayTimeMax = 10.0f;

        PhaseUtils::ExpParam m_attack;
        PhaseUtils::ExpParam m_decay;
        PhaseUtils::ExpParam m_release;

        InputSetter()
            : m_attack(1 / (x_sampleRate * x_attackTimeMax), 1 / (x_sampleRate * x_attackTimeMin))
            , m_decay(1 / (x_sampleRate * x_decayTimeMax), 1 / (x_sampleRate * x_decayTimeMin))
            , m_release(1 / (x_sampleRate * x_decayTimeMax), 1 / (x_sampleRate * x_decayTimeMin))
        {
        }

        void Set(float attack, float decay, float sustain, float release, Input& input)
        {
            input.m_attackIncrement = m_attack.Update(1.0 - attack);
            input.m_decayIncrement = m_decay.Update(1.0 - decay);
            input.m_sustainLevel = sustain;
            input.m_releaseIncrement = m_release.Update(1.0 - release);
        }
    };

    State m_state;
    float m_output;

    ADSR()
        : m_state(State::Idle)
        , m_output(0.0f)
    {
    }

    float Process(Input& input)
    {
        if (input.m_gate)
        {
            if (m_state == State::Idle || m_state == State::DecayNoGate || m_state == State::Release || input.m_trig)
            {
                m_state = State::Attack;
            }
        }
        else
        {
            if (m_state == State::Decay || m_state == State::Attack)
            {                
                m_state = State::DecayNoGate;
            } 
            else if (m_state == State::Sustain)
            {
                m_state = State::Release;
            }
        }

        switch (m_state)
        {
            case State::Idle:
            {
                // Do nothing in idle state
                break;
            }
            case State::Attack:
            {
                m_output += input.m_attackIncrement;
                if (m_output >= 1.0f)
                {
                    m_state = input.m_gate ? State::Decay : State::DecayNoGate;
                    m_output = 1.0f;
                }

                break;
            }
            case State::Decay:
            case State::DecayNoGate:
            {
                m_output -= input.m_decayIncrement;
                if (m_output <= input.m_sustainLevel)
                {
                    if (m_state == State::Decay)
                    {
                        m_state = State::Sustain;
                        m_output = input.m_sustainLevel;
                    }
                    else
                    {
                        m_state = State::Release;
                    }
                }

                break;
            }
            case State::Sustain:
            {
                m_output = input.m_sustainLevel;
                break;
            }
            case State::Release:
            {
                m_output -= input.m_releaseIncrement;
                if (m_output <= 0.0f)
                {
                    m_state = State::Idle;
                    m_output = 0.0f;
                }

                break;
            }
        }

        return m_output;
    }
};