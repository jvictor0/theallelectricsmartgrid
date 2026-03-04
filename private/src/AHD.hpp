#pragma once

#include "Slew.hpp"
#include "PhaseUtils.hpp"
#include "CircleTracker.hpp"
#include "TheoryOfTime.hpp"
#include "SampleTimer.hpp"

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
        TheoryOfTimeBase* m_theoryOfTime;
        size_t m_loopIndex;
        float m_samplePosition;
        CircleDistanceTracker m_circleTracker;

        float m_attackIncrement;
        double m_holdSamples;
        float m_decayIncrement;

        float m_amplitude;
        bool m_amplitudePolarity;

        bool m_trig;
        bool m_release;

        Input()
            : m_envelopeTimeSamples(48000.0)
            , m_theoryOfTime(nullptr)
            , m_loopIndex(0)
            , m_samplePosition(0.0f)
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

                // Trigs can only happen at the first sample of a microblock...
                //
                m_circleTracker.Reset(m_theoryOfTime->GetIndirectPhasor(0, m_loopIndex));
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
            : InputSetter(x_attackTimeMin, x_attackTimeMax, x_decayTimeMin, x_decayTimeMax)
        {
        }

        InputSetter(float attackTimeMin, float attackTimeMax, float decayTimeMin, float decayTimeMax)
            : m_attack(1.0f / (x_sampleRate * attackTimeMax), 1.0f / (x_sampleRate * attackTimeMin))
            , m_decay(1.0f / (x_sampleRate * decayTimeMax), 1.0f / (x_sampleRate * decayTimeMin))
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

    struct SlewedInputSetter
    {
        InputSetter m_inputSetter;
        ParamSlew m_attackSlew;
        ParamSlew m_decaySlew;
        ParamSlew m_holdSlew;
        ParamSlew m_amplitudeSlew;

        SlewedInputSetter(float relativeSampleRate, float attackTimeMin, float attackTimeMax, float decayTimeMin, float decayTimeMax)
            : m_inputSetter(attackTimeMin, attackTimeMax, decayTimeMin, decayTimeMax)
            , m_attackSlew(relativeSampleRate)
            , m_decaySlew(relativeSampleRate)
            , m_holdSlew(relativeSampleRate)
            , m_amplitudeSlew(relativeSampleRate)
        {
        }

        void SetTargets(float attack, float hold, float decay, float amplitude)
        {
            Input input;
            m_inputSetter.Set(attack, hold, decay, amplitude, true, input);
            m_attackSlew.Update(input.m_attackIncrement);
            m_decaySlew.Update(input.m_decayIncrement);
            m_holdSlew.Update(input.m_holdSamples);
            m_amplitudeSlew.Update(input.m_amplitude);
        }

        void Process(Input& input)
        {
            input.m_attackIncrement = m_attackSlew.Process();
            input.m_decayIncrement = m_decaySlew.Process();
            input.m_holdSamples = m_holdSlew.Process();
            input.m_amplitude = m_amplitudeSlew.Process();
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
                double phase = input.m_theoryOfTime->GetInterpolatedIndirectPhasor(input.m_loopIndex, input.m_samplePosition);
                input.m_circleTracker.Process(phase);
                samples = input.m_circleTracker.Distance() * input.m_envelopeTimeSamples;
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
