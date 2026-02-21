#pragma once

#include "Slew.hpp"
#include "PhaseUtils.hpp"

struct AHD
{
    enum class State : int
    {
        Idle,
        Running,
        Release,
    };

    struct AHDControl
    {
        double m_samples;
        double m_envelopeTimeSamples;
        bool m_release;
        bool m_trig;


        AHDControl()
            : m_samples(0.0)
            , m_envelopeTimeSamples(48000.0)
            , m_release(false)
            , m_trig(false)
        {
        }

        void Reset()
        {
            m_samples = 0.0;
            m_release = false;
            m_trig = false;
        }
    };

    struct Input
    {
        OPLowPassFilterDouble m_samples;
        double m_envelopeTimeSamples;

        float m_attackIncrement;
        double m_holdSamples;
        float m_decayIncrement;

        float m_amplitude;
        bool m_amplitudePolarity;

        bool m_trig;
        bool m_release;

        Input()
            : m_envelopeTimeSamples(48000.0)
            , m_attackIncrement(0.0f)
            , m_holdSamples(0.0)
            , m_decayIncrement(0.0f)
            , m_amplitude(1.0f)
            , m_amplitudePolarity(true)
            , m_trig(false)
            , m_release(false)
        {
            m_samples.SetAlphaFromNatFreq(1000.0 / 48000.0);
        }

        void Set(AHDControl& control)
        {
            if (control.m_trig)
            {
                m_samples.m_output = 0.0;
            }
            else
            {
                m_samples.Process(control.m_samples);
            }
            
            m_envelopeTimeSamples = control.m_envelopeTimeSamples;
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

        PhaseUtils::ExpParam m_attack;
        PhaseUtils::ZeroedExpParam m_hold;
        PhaseUtils::ExpParam m_decay;

        InputSetter()
            : m_attack(1.0f / (x_sampleRate * x_attackTimeMax), 1.0f / (x_sampleRate * x_attackTimeMin))
            , m_decay(1.0f / (x_sampleRate * x_decayTimeMax), 1.0f / (x_sampleRate * x_decayTimeMin))
        {
            m_hold.SetBaseByCenter(1.0 / 32);
            m_hold.SetMax(16.0);
        }

        void Set(float attack, float hold, float decay, float amplitude, bool amplitudePolarity, Input& input)
        {
            input.m_attackIncrement = m_attack.Update(1.0f - attack);
            input.m_decayIncrement = m_decay.Update(1.0f - decay);

            // Hold is in loop divisions (0 to 16), convert to samples
            //
            double holdLoops = static_cast<double>(m_hold.Update(hold));
            input.m_holdSamples = holdLoops * input.m_envelopeTimeSamples;

            input.m_amplitude = amplitude;
            input.m_amplitudePolarity = amplitudePolarity;
        }
    };

    State m_state;
    bool m_changed;
    float m_output;
    float m_rawOutput;
    float m_startOutput;

    AHD()
        : m_state(State::Idle)
        , m_changed(false)
        , m_output(0.0f)
        , m_rawOutput(0.0f)
        , m_startOutput(0.0f)
    {
    }
    
    float Process(Input& input)
    {
        m_changed = false;
        if (input.m_trig)
        {
            m_changed = m_state != State::Running;
            m_state = State::Running;
            m_startOutput = m_rawOutput;
        }
        else if (input.m_release && m_state != State::Idle)
        {
            m_changed = m_state != State::Release;
            m_state = State::Release;
        }

        switch (m_state)
        {
            case State::Idle:
            {
                m_rawOutput = 0.0f;
                break;
            }
            case State::Running:
            {
                double samples = input.m_samples.m_output;
                double attackPos = samples * static_cast<double>(input.m_attackIncrement) + static_cast<double>(m_startOutput);
                if (attackPos < 1.0)
                {
                    m_rawOutput = static_cast<float>(attackPos);
                }
                else
                {
                    double attackEndSamples = (1.0 - static_cast<double>(m_startOutput)) / static_cast<double>(input.m_attackIncrement);
                    double holdEndSamples = attackEndSamples + input.m_holdSamples;
                    if (samples < holdEndSamples)
                    {
                        m_rawOutput = 1.0f;
                    }
                    else
                    {
                        double decayPos = 1.0 - (samples - holdEndSamples) * static_cast<double>(input.m_decayIncrement);
                        if (decayPos < 0.0)
                        {
                            m_rawOutput = 0.0f;
                            m_state = State::Idle;
                            m_changed = true;
                        }
                        else
                        {
                            m_rawOutput = static_cast<float>(decayPos);
                        }
                    }
                }

                break;
            }
            case State::Release:
            {
                m_rawOutput -= input.m_decayIncrement;
                if (m_rawOutput <= 0.0f)
                {
                    m_changed = true;
                    m_state = State::Idle;
                    m_rawOutput = 0.0f;
                }

                break;
            }
        }

        if (input.m_amplitudePolarity)
        {
            m_output = m_rawOutput * input.m_amplitude;
        }
        else
        {
            m_output = (1.0f - input.m_amplitude) + m_rawOutput * input.m_amplitude;
        }

        return m_output;
    }
};
