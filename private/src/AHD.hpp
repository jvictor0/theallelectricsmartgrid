#pragma once

#include <cmath>

#include "Slew.hpp"
#include "PhaseUtils.hpp"
#include "TheoryOfTimeBase.hpp"

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
        double m_phaseRatio;
        double m_envelopePeriodSamples;
        bool m_release;
        bool m_trig;

        AHDControl()
            : m_phaseRatio(1.0)
            , m_envelopePeriodSamples(48000.0)
            , m_release(false)
            , m_trig(false)
        {
        }

        void Reset()
        {
            m_release = false;
            m_trig = false;
        }
    };

    struct Input
    {
        double m_startGlobalPhase;
        double m_phaseRatio;
        double m_envelopePeriodSamples;
        TheoryOfTimeBase* m_theoryOfTime;
        float m_samplePosition;

        float m_attackIncrement;
        double m_holdLoops;
        float m_decayIncrement;

        float m_amplitude;
        bool m_amplitudePolarity;

        bool m_trig;
        bool m_release;

        Input()
            : m_startGlobalPhase(0.0)
            , m_phaseRatio(1.0)
            , m_envelopePeriodSamples(48000.0)
            , m_theoryOfTime(nullptr)
            , m_samplePosition(0.0f)
            , m_attackIncrement(0.0f)
            , m_holdLoops(0.0)
            , m_decayIncrement(0.0f)
            , m_amplitude(1.0f)
            , m_amplitudePolarity(true)
            , m_trig(false)
            , m_release(false)
        {
        }

        void Set(AHDControl& control)
        {
            if (control.m_trig)
            {
                // Trigs can only happen at the first sample of a microblock...
                //
                m_startGlobalPhase = m_theoryOfTime->GetPhase(
                    TheoryOfTimeBase::x_globalLoop,
                    0.0,
                    PhaseDomain::Modulated);
                m_phaseRatio = control.m_phaseRatio;
                m_envelopePeriodSamples = control.m_envelopePeriodSamples;
            }

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

            // Hold is in loop divisions (0 to 16).
            //
            input.m_holdLoops = static_cast<double>(m_hold.Update(hold));

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
            m_holdSlew.Update(input.m_holdLoops);
            m_amplitudeSlew.Update(input.m_amplitude);
        }

        void Process(Input& input)
        {
            input.m_attackIncrement = m_attackSlew.Process();
            input.m_decayIncrement = m_decaySlew.Process();
            input.m_holdLoops = m_holdSlew.Process();
            input.m_amplitude = m_amplitudeSlew.Process();
        }
    };

    State m_state;
    bool m_changed;
    float m_output;
    float m_rawOutput;
    float m_startOutput;
    float m_amplitude;

    AHD()
        : m_state(State::Idle)
        , m_changed(false)
        , m_output(0.0f)
        , m_rawOutput(0.0f)
        , m_startOutput(0.0f)
        , m_amplitude(1.0f)
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
                double phase = input.m_theoryOfTime->GetPhase(
                    TheoryOfTimeBase::x_globalLoop,
                    input.m_samplePosition,
                    PhaseDomain::Modulated);
                double samples = std::abs(phase - input.m_startGlobalPhase)
                    * input.m_phaseRatio * input.m_envelopePeriodSamples;
                double attackPos = samples * static_cast<double>(input.m_attackIncrement) + static_cast<double>(m_startOutput);
                if (attackPos < 1.0)
                {
                    m_rawOutput = static_cast<float>(attackPos);
                }
                else
                {
                    double attackEndSamples = (1.0 - static_cast<double>(m_startOutput)) / static_cast<double>(input.m_attackIncrement);
                    double holdSamples = input.m_holdLoops * input.m_envelopePeriodSamples;
                    double holdEndSamples = attackEndSamples + holdSamples;
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

        m_amplitude = input.m_amplitude;
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
