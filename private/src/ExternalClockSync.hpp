#pragma once

#include "TheoryOfTime.hpp"

#include <cassert>
#include <cmath>
#include <limits>

struct ExternalClockSync
{
    static constexpr double x_defaultFreqOut = 1.0 / static_cast<double>(SampleTimer::x_sampleRate);

    enum class State
    {
        Stopped,
        Armed,
        Running
    };

    int m_samplesSinceLastClock;
    int m_ticksOffset;
    double m_freqOut;
    State m_state;

    struct Input
    {
        TheoryOfTime* m_theoryOfTime;
        bool m_clockTick;
        int m_ticksMultiplier;
        int m_loopIndex;

        Input()
            : m_theoryOfTime(nullptr)
            , m_clockTick(false)
            , m_ticksMultiplier(24)
            , m_loopIndex(TheoryOfTimeBase::x_masterLoop)
        {
        }
    };

    ExternalClockSync()
        : m_samplesSinceLastClock(0)
        , m_ticksOffset(0)
        , m_freqOut(x_defaultFreqOut)
        , m_state(State::Stopped)
    {
    }

    bool IsRunning() const
    {
        return m_state == State::Running;
    }

    void Start()
    {
        if (m_state == State::Stopped)
        {
            m_state = State::Armed;
        }
    }

    void Stop()
    {
        if (m_state != State::Stopped)
        {
            m_state = State::Stopped;
        }
    }

    double EffectiveFreq() const
    {
        if (m_freqOut <= 0.0)
        {
            return std::numeric_limits<double>::denorm_min();
        }

        return m_freqOut;
    }

    void Process(Input const& input)
    {
        assert(input.m_theoryOfTime != nullptr);
        assert(input.m_ticksMultiplier > 0);
        assert(input.m_loopIndex >= 0);
        assert(input.m_loopIndex < TheoryOfTimeBase::x_numLoops);

        if (m_state == State::Armed)
        {
            if (input.m_clockTick)
            {
                m_state = State::Running;
                m_ticksOffset = 1;
                m_samplesSinceLastClock = 0;
                return;
            }

            m_samplesSinceLastClock++;
            return;
        }

        if (m_state == State::Running)
        {
            if (input.m_clockTick)
            {
                assert(m_samplesSinceLastClock > 0);

                size_t j = SampleTimer::GetUBlockIndex();
                double currentPhase = input.m_theoryOfTime->GetUnwoundMasterIndependent(j);
                double multiplier = input.m_theoryOfTime->GetLoopExternalMultiplier(j, input.m_loopIndex);
                assert(multiplier > 0.0);

                double currentTicksOffset = static_cast<double>(m_ticksOffset) / input.m_ticksMultiplier;
                double nextTicksOffset = static_cast<double>(m_ticksOffset + 1) / input.m_ticksMultiplier;
                double currentExpectedBranch = std::round(currentPhase * multiplier - currentTicksOffset);
                double nextExpectedPhase = (currentExpectedBranch + nextTicksOffset) / multiplier;
                int samplesUntilNextClock = m_samplesSinceLastClock + 1;
                m_freqOut = (nextExpectedPhase - currentPhase) / samplesUntilNextClock;

                m_ticksOffset = (m_ticksOffset + 1) % input.m_ticksMultiplier;
                m_samplesSinceLastClock = 0;
            }
            else
            {
                m_samplesSinceLastClock++;
            }

            return;
        }

        assert(m_state == State::Stopped);

        if (input.m_clockTick)
        {
            assert(m_samplesSinceLastClock > 0);

            size_t j = SampleTimer::GetUBlockIndex();
            double multiplier = input.m_theoryOfTime->GetLoopExternalMultiplier(j, input.m_loopIndex);
            assert(multiplier > 0.0);

            double phaseIncrement = 1.0 / (static_cast<double>(input.m_ticksMultiplier) * multiplier);
            m_freqOut = phaseIncrement / m_samplesSinceLastClock;
            m_samplesSinceLastClock = 0;
        }
        else
        {
            m_samplesSinceLastClock++;
        }
    }
};
