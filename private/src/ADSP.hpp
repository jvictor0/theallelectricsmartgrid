#pragma once

#include "PhaseUtils.hpp"
#include "Slew.hpp"
#include "SampleTimer.hpp"

struct ADSP
{
    enum class State : int
    {
        Idle,
        Attack,
        Decay,
        SustainPhasor,
        Release,
    };

    struct ADSPControl
    {
        ParamSlew m_phasorSlew;
        bool m_release;
        bool m_trig;

        ADSPControl()
            : m_phasorSlew(1.0)
            , m_release(false)
            , m_trig(false)
        {
        }

        void Reset()
        {
            m_phasorSlew.m_target = 0.0f;
            m_phasorSlew.m_filter.m_output = 0.0f;
            m_release = false;
            m_trig = false;
        }
    };
    
    struct Input
    {
        float m_attackIncrement;
        float m_decayIncrement;
        float m_sustainLevel;
        float m_releaseIncrement;
        float m_phasorMult;

        float m_phasor;
        bool m_release;
        bool m_trig;

        Input()
            : m_attackIncrement(0.0f)
            , m_decayIncrement(0.0f)
            , m_sustainLevel(0.0f)
            , m_releaseIncrement(0.0f)
            , m_release(false)
            , m_trig(false)
        {
        }

        void Set(ADSPControl& control, float phasor)
        {
            m_phasor = phasor;
            m_release = control.m_release;
            m_trig = control.m_trig;
        }
    };

    struct InputSetter
    {
        static constexpr float x_sampleRate = 48000.0f;
        static constexpr float x_attackTimeMin = 0.001f;
        static constexpr float x_attackTimeMax = 2.5f;
        static constexpr float x_decayTimeMin = 0.01f;
        static constexpr float x_decayTimeMax = 10.0f;
        static constexpr float x_maxPhasorMult = 8.0f;

        PhaseUtils::ExpParam m_attack;
        PhaseUtils::ExpParam m_decay;
        PhaseUtils::ZeroedExpParam m_phasorMult;

        InputSetter()
            : m_attack(1 / (x_sampleRate * x_attackTimeMax), 1 / (x_sampleRate * x_attackTimeMin))
            , m_decay(1 / (x_sampleRate * x_decayTimeMax), 1 / (x_sampleRate * x_decayTimeMin))
        {
            m_phasorMult.SetBaseByCenter(2.0 / x_maxPhasorMult);
        }

        void Set(float attack, float decay, float sustain, float phasorMult, Input& input)
        {
            input.m_attackIncrement = m_attack.Update(1.0 - attack);
            input.m_decayIncrement = m_decay.Update(1.0 - decay);
            input.m_sustainLevel = sustain;
            input.m_releaseIncrement = 1.0 / x_sampleRate;
            input.m_phasorMult = x_maxPhasorMult * m_phasorMult.Update(1.0 - phasorMult);
        }
    };

    State m_state;
    bool m_changed;
    float m_output;

    ADSP()
      : m_state(State::Idle)
      , m_changed(false)
      , m_output(0.0f)
    {
    }

    float SustainPhasor(Input& input)
    {
        return input.m_sustainLevel * (1.0 - input.m_phasor * input.m_phasorMult);
    }

    float Process(Input& input)
    {
        m_changed = false;
        if (input.m_trig)
        {
            m_changed = m_state != State::Attack;
            m_state = State::Attack;
        }
        else if (input.m_release && m_state != State::Release && m_state != State::Idle)
        {
            m_changed = m_state != State::Release;
            m_state = State::Release;
        }

        switch (m_state)
        {
            case State::Idle:
            {
                break;
            }
            case State::Attack:
            {
                m_output += input.m_attackIncrement;
                if (m_output >= 1.0f)
                {
                    m_state = State::Decay;
                    m_output = 1.0f;
                    m_changed = true;
                }

                break;
            }
            case State::Decay:
            {
                m_output -= input.m_decayIncrement;
                float sustainPhasor = SustainPhasor(input);
                if (m_output <= sustainPhasor)
                {
                    m_changed = true;
                    m_state = State::SustainPhasor;
                    m_output = sustainPhasor;
                }
                else if (m_output <= 0.0f)
                {
                    m_changed = true;
                    m_state = State::Idle;
                    m_output = 0.0f;
                }

                break;
            }
            case State::SustainPhasor:
            {
                m_output = SustainPhasor(input);
                if (m_output <= 0.0f)
                {
                    m_changed = true;
                    m_state = State::Idle;
                    m_output = 0.0f;
                }

                break;
            }
            case State::Release:
            {
                m_output -= input.m_releaseIncrement;
                if (m_output <= 0.0f)
                {
                    m_changed = true;
                    m_state = State::Idle;
                    m_output = 0.0f;
                }

                break;
            }
        }

        return m_output;
    }
};