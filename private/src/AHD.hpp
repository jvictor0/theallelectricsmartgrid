#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>

#include "AHDMemory.hpp"
#include "AHDProcess.hpp"
#include "Slew.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "TheoryOfTimeBase.hpp"

struct AHD : AHDMemory<double>
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
        bool m_reverseTrig;
        bool m_voiceCellCrossed;
        double m_forwardTriggerPhase;

        AHDControl()
            : m_phaseRatio(1.0)
            , m_envelopePeriodSamples(SampleTimer::SecondsToSamples(1.0))
            , m_release(false)
            , m_trig(false)
            , m_reverseTrig(false)
            , m_voiceCellCrossed(false)
            , m_forwardTriggerPhase(0.0)
        {
        }

        void Reset()
        {
            m_release = false;
            m_trig = false;
            m_reverseTrig = false;
            m_voiceCellCrossed = false;
        }
    };

    struct Input
    {
        double m_startGlobalPhase;
        double m_phaseRatio;
        double m_envelopePeriodSamples;
        TheoryOfTimeBase* m_theoryOfTime;

        float m_attackIncrement;
        double m_holdLoops;
        float m_decayIncrement;

        // Engine samples elapsed per Process call, used only for explicit release.
        //
        float m_releaseFactor;

        float m_amplitude;
        bool m_amplitudePolarity;

        bool m_trig;
        bool m_reverseTrig;
        bool m_release;
        bool m_voiceCellCrossed;
        float m_forwardStartOutput;

        Input()
            : m_startGlobalPhase(0.0)
            , m_phaseRatio(1.0)
            , m_envelopePeriodSamples(SampleTimer::SecondsToSamples(1.0))
            , m_theoryOfTime(nullptr)
            , m_attackIncrement(0.0f)
            , m_holdLoops(0.0)
            , m_decayIncrement(0.0f)
            , m_releaseFactor(1.0f)
            , m_amplitude(1.0f)
            , m_amplitudePolarity(true)
            , m_trig(false)
            , m_reverseTrig(false)
            , m_release(false)
            , m_voiceCellCrossed(false)
            , m_forwardStartOutput(0.0f)
        {
        }

        void Set(const AHDControl& control)
        {
            Set(control, 0.0f);
        }

        void Set(const AHDControl& control, float forwardStartOutput)
        {
            if (control.m_trig)
            {
                m_startGlobalPhase = m_theoryOfTime->GetPhase(
                    TheoryOfTimeBase::x_globalLoop,
                    PhaseDomain::Modulated);
            }
            else if (control.m_reverseTrig)
            {
                m_startGlobalPhase = control.m_forwardTriggerPhase;
                m_forwardStartOutput = forwardStartOutput;
            }

            if (control.m_trig || control.m_reverseTrig)
            {
                m_phaseRatio = control.m_phaseRatio;
                m_envelopePeriodSamples = control.m_envelopePeriodSamples;
            }

            m_release = control.m_release;
            m_trig = control.m_trig;
            m_reverseTrig = control.m_reverseTrig;
            m_voiceCellCrossed = control.m_voiceCellCrossed;
        }
    };

    struct InputSetter
    {
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
            : m_attack(SampleTimer::SecondsToCycPerSamp(attackTimeMax), SampleTimer::SecondsToCycPerSamp(attackTimeMin))
            , m_decay(SampleTimer::SecondsToCycPerSamp(decayTimeMax), SampleTimer::SecondsToCycPerSamp(decayTimeMin))
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

    State m_state = State::Idle;
    bool m_changed = false;
    bool m_triggered = false;
    bool m_voiceCellCrossed = false;
    float m_output = 0.0f;
    float m_amplitude = 1.0f;

    AHD()
        : AHDMemory<double>{}
    {
    }

    float Process(Input& input)
    {
        const auto before = m_state;
        const bool triggering = input.m_trig || input.m_reverseTrig;
        if (before != State::Idle || m_armed || triggering)
        {
            const TheoryOfTimeBase* time = input.m_theoryOfTime;
            assert(time);
            if (input.m_reverseTrig && !input.m_trig)
            {
                assert(std::isfinite(input.m_startGlobalPhase));
                assert(input.m_attackIncrement > 0 && input.m_decayIncrement > 0);
                assert(input.m_phaseRatio > 0 && input.m_envelopePeriodSamples > 0);
                assert(input.m_forwardStartOutput >= 0 && input.m_forwardStartOutput <= 1);
            }

            const bool releasing = !triggering && (input.m_release || before == State::Release);
            const double phaseDistance = releasing ? 0.0
                : time->GetPhase(TheoryOfTimeBase::x_globalLoop, PhaseDomain::Modulated) - input.m_startGlobalPhase;
            const double basicIncrement = releasing && before != State::Idle
                ? time->GetUnmodulatedGlobalPhaseIncrement() : 0.0;
            const auto status = AHDProcess::Process(*this, AHDProcess::Input{
                phaseDistance,
                std::abs(phaseDistance),
                input.m_phaseRatio,
                input.m_envelopePeriodSamples,
                input.m_attackIncrement,
                input.m_holdLoops,
                input.m_decayIncrement,
                basicIncrement,
                input.m_releaseFactor,
                input.m_trig,
                input.m_reverseTrig,
                input.m_forwardStartOutput,
                input.m_release});
            m_state = status.m_idle ? State::Idle : (status.m_releasing ? State::Release : State::Running);
        }

        m_changed = before != m_state;
        m_triggered = input.m_trig;
        m_voiceCellCrossed = input.m_voiceCellCrossed;
        m_amplitude = input.m_amplitude;
        const double scaled = m_rawOutput * m_amplitude;
        m_output = static_cast<float>(input.m_amplitudePolarity ? scaled : (1.0f - m_amplitude) + scaled);

        input.m_trig = false;
        input.m_reverseTrig = false;
        input.m_release = false;
        input.m_voiceCellCrossed = false;
        return m_output;
    }
};

// Size envelopes sharing one theory of time. The stage machine branches per
// channel, so each channel runs a scalar AHD; parameter mapping and outputs
// are vectors.
//
template <size_t Size>
struct AHDGeneric
{
    using Vector = MultiChannelNumber<float, Size>;

    struct Input
    {
        AHD::Input m_channels[Size];

        void SetTheoryOfTime(TheoryOfTimeBase* theoryOfTime)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_channels[i].m_theoryOfTime = theoryOfTime;
            }
        }

        void SetReleaseFactor(float releaseFactor)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_channels[i].m_releaseFactor = releaseFactor;
            }
        }

        void Set(size_t channel, AHD::AHDControl& control)
        {
            m_channels[channel].Set(control);
        }
    };

    struct InputSetter
    {
        PhaseUtils::ExpParamGeneric<float, Size> m_attack;
        PhaseUtils::ZeroedExpParamGeneric<float, Size> m_hold;
        PhaseUtils::ExpParamGeneric<float, Size> m_decay;

        InputSetter()
            : InputSetter(
                AHD::InputSetter::x_attackTimeMin,
                AHD::InputSetter::x_attackTimeMax,
                AHD::InputSetter::x_decayTimeMin,
                AHD::InputSetter::x_decayTimeMax)
        {
        }

        InputSetter(float attackTimeMin, float attackTimeMax, float decayTimeMin, float decayTimeMax)
            : m_attack(SampleTimer::SecondsToCycPerSamp(attackTimeMax), SampleTimer::SecondsToCycPerSamp(attackTimeMin))
            , m_hold()
            , m_decay(SampleTimer::SecondsToCycPerSamp(decayTimeMax), SampleTimer::SecondsToCycPerSamp(decayTimeMin))
        {
            m_hold.SetBaseByCenter(1.0f / 32);
            m_hold.SetMax(16.0f);
        }

        void Set(const Vector& attack, const Vector& hold, const Vector& decay, const Vector& amplitude, bool amplitudePolarity, Input& input)
        {
            Vector attackIncrement = m_attack.Update(Vector::Splat(1) - attack);
            Vector decayIncrement = m_decay.Update(Vector::Splat(1) - decay);

            // Hold is in loop divisions (0 to 16).
            //
            Vector holdLoops = m_hold.Update(hold);

            for (size_t i = 0; i < Size; ++i)
            {
                AHD::Input& channel = input.m_channels[i];
                channel.m_attackIncrement = attackIncrement[i];
                channel.m_decayIncrement = decayIncrement[i];
                channel.m_holdLoops = static_cast<double>(holdLoops[i]);
                channel.m_amplitude = amplitude[i];
                channel.m_amplitudePolarity = amplitudePolarity;
            }
        }
    };

    AHD m_channels[Size];
    Vector m_output;
    Vector m_rawOutput;

    AHDGeneric()
        : m_output()
        , m_rawOutput()
    {
    }

    Vector Process(Input& input)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_output[i] = m_channels[i].Process(input.m_channels[i]);
            m_rawOutput[i] = static_cast<float>(m_channels[i].m_rawOutput);
        }

        return m_output;
    }

    Vector GetOutput() const
    {
        return m_output;
    }

    Vector GetRawOutput() const
    {
        return m_rawOutput;
    }
};

using TrioAHD = AHDGeneric<3>;
