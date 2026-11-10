#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "TheoryOfTimeRhythm.hpp"
#include "PhaseClock.hpp"

enum class PhaseDomain
{
    Unmodulated,
    Modulated
};

struct TheoryOfTimeBaseUIState;

struct TimeLoop
{
    struct Input
    {
        int m_parentIndex = 0;
        int m_parentMult = 2;
    };

    Input m_input;
    int64_t m_cycleRatio = 1;
    int64_t m_periodTicks = 1;

    bool m_gate = false;
    bool m_unmodulatedCycleCrossed = false;
    bool m_modulatedCycleCrossed = false;
};

struct TheoryOfTimeBase
{
    static constexpr size_t x_numLoops = 6;
    static constexpr size_t x_globalLoop = x_numLoops - 1;

    struct Sample
    {
        double m_unmodulatedPhase = 0.0;
        double m_modulatedPhase = 0.0;
        double m_modulatedPhaseIncrement = 0.0;
        int64_t m_unmodulatedGlobalTickPosition = 0;
        int64_t m_modulatedGlobalTickPosition = 0;
        std::array<TimeLoop, x_numLoops> m_loops;
        bool m_running = false;
        bool m_anyChange = false;

        Sample()
        {
            for (size_t i = 0; i < x_numLoops; ++i)
            {
                m_loops[i].m_input.m_parentIndex = static_cast<int>(i + 1);
                m_loops[i].m_input.m_parentMult = 1;
            }
        }
    };

    struct Input
    {
        double m_unmodulatedPhase = 0.0;
        double m_phaseOffset = 0.0;
        std::array<TimeLoop::Input, x_numLoops> m_input;
        std::array<TheoryOfTimeRhythm, x_numLoops> m_rhythm;
        bool m_running = false;

        Input()
        {
            for (size_t i = 0; i < x_numLoops; ++i)
            {
                m_input[i].m_parentIndex = static_cast<int>(i + 1);
            }
        }
    };

    Sample m_sample;
    PhaseClock m_phaseClock;
    double m_globalPeriodSamples = 1.0;

    static int64_t ComputeGlobalTickPosition(double phase, int64_t globalPeriodTicks)
    {
        double globalTickPosition = std::floor(phase * static_cast<double>(globalPeriodTicks));
        constexpr double x_minPosition = static_cast<double>(std::numeric_limits<int64_t>::min());
        assert(
            std::isfinite(globalTickPosition)
            && globalTickPosition >= x_minPosition
            && globalTickPosition < -x_minPosition);
        std::ignore = x_minPosition;
        return static_cast<int64_t>(globalTickPosition);
    }

    const TimeLoop& GetLoop(size_t loopIndex) const
    {
        assert(loopIndex < x_numLoops);
        return m_sample.m_loops[loopIndex];
    }

    double GetPhase(size_t loopIndex, PhaseDomain domain) const
    {
        assert(loopIndex < x_numLoops);
        double globalPhase = domain == PhaseDomain::Unmodulated ? m_sample.m_unmodulatedPhase : m_sample.m_modulatedPhase;
        return globalPhase * static_cast<double>(m_sample.m_loops[loopIndex].m_cycleRatio);
    }

    // Basic global cycles per engine sample, including while transport is stopped.
    //
    double GetUnmodulatedGlobalPhaseIncrement() const
    {
        assert(m_globalPeriodSamples > 0.0);
        return 1.0 / m_globalPeriodSamples;
    }

    double GetModulatedGlobalPhaseIncrement() const
    {
        return m_sample.m_modulatedPhaseIncrement;
    }

    int64_t GetGlobalTickPosition(PhaseDomain domain) const
    {
        return domain == PhaseDomain::Unmodulated
            ? m_sample.m_unmodulatedGlobalTickPosition
            : m_sample.m_modulatedGlobalTickPosition;
    }

    int64_t GetPeriodTicks(size_t loopIndex) const
    {
        return GetLoop(loopIndex).m_periodTicks;
    }

    int64_t GetCycleRatio(size_t loopIndex) const
    {
        return GetLoop(loopIndex).m_cycleRatio;
    }

    bool CrossedCycleBoundary(size_t loopIndex, PhaseDomain domain) const
    {
        const TimeLoop& loop = GetLoop(loopIndex);
        return domain == PhaseDomain::Unmodulated ? loop.m_unmodulatedCycleCrossed : loop.m_modulatedCycleCrossed;
    }

    static int64_t GetLoopCyclePosition(const Sample& sample, size_t loopIndex, int resetLoopIndex)
    {
        assert(loopIndex < x_numLoops);
        assert(resetLoopIndex >= -1 && resetLoopIndex < static_cast<int>(x_numLoops));
        int64_t loopPeriodTicks = sample.m_loops[loopIndex].m_periodTicks;
        int64_t resetPeriodTicks = resetLoopIndex == -1 ? -1 : sample.m_loops[resetLoopIndex].m_periodTicks;
        int64_t globalTickPosition = sample.m_modulatedGlobalTickPosition;
        return GetLoopCyclePositionFromTicks(loopPeriodTicks, resetPeriodTicks, globalTickPosition);
    }

    static int64_t GetLoopCyclePositionFromTicks(
        int64_t loopPeriodTicks,
        int64_t resetPeriodTicks,
        int64_t globalTickPosition)
    {
        int64_t loopCyclePosition = PhaseUtils::FloorDiv(globalTickPosition, loopPeriodTicks);
        int64_t resetCycleCount = GetResetCycleCount(loopPeriodTicks, resetPeriodTicks);
        if (resetCycleCount > 0)
        {
            return PhaseUtils::FloorMod(loopCyclePosition, resetCycleCount);
        }
        else
        {
            return loopCyclePosition;
        }
    }

    int64_t GetLoopCyclePosition(size_t loopIndex, int resetLoopIndex) const
    {
        return GetLoopCyclePosition(m_sample, loopIndex, resetLoopIndex);
    }

    int64_t GetLoopCyclePosition(size_t loopIndex) const
    {
        return GetLoopCyclePosition(loopIndex, -1);
    }

    static int64_t GetResetCycleCount(int64_t loopPeriodTicks, int64_t resetPeriodTicks)
    {
        assert(loopPeriodTicks > 0);
        if (resetPeriodTicks > 0 && resetPeriodTicks % loopPeriodTicks == 0)
        {
            return resetPeriodTicks / loopPeriodTicks;
        }

        return 0;
    }

    int64_t GetResetCycleCount(size_t loopIndex, int resetLoopIndex) const
    {
        assert(resetLoopIndex >= -1 && resetLoopIndex < static_cast<int>(x_numLoops));
        return GetResetCycleCount(
            GetPeriodTicks(loopIndex),
            resetLoopIndex == -1 ? -1 : GetPeriodTicks(resetLoopIndex));
    }

    bool AnyChange() const
    {
        return m_sample.m_anyChange;
    }

    bool AnyTick(size_t loopIndex) const
    {
        return GetLoop(loopIndex).m_modulatedCycleCrossed;
    }

    static void SetLoopPeriods(Sample& sample)
    {
        int64_t commonRatio = 1;
        sample.m_loops[x_globalLoop].m_cycleRatio = 1;
        for (int i = static_cast<int>(x_globalLoop) - 1; i >= 0; --i)
        {
            TimeLoop& loop = sample.m_loops[i];
            int parent = loop.m_input.m_parentIndex;
            assert(parent > i && parent < static_cast<int>(x_numLoops));
            loop.m_cycleRatio = sample.m_loops[parent].m_cycleRatio * loop.m_input.m_parentMult;
            commonRatio = std::lcm(commonRatio, loop.m_cycleRatio);
        }

        int64_t globalPeriod = commonRatio;
        for (TimeLoop& loop : sample.m_loops)
        {
            loop.m_periodTicks = globalPeriod / loop.m_cycleRatio;
        }
    }

    static bool AcceptTopology(Sample& sample, const Input& input, bool requireBoundaries)
    {
        std::array<bool, x_globalLoop> accept{};
        bool changed = false;
        for (size_t i = 0; i < x_globalLoop; ++i)
        {
            const TimeLoop::Input& current = sample.m_loops[i].m_input;
            const TimeLoop::Input& requested = input.m_input[i];
            assert(requested.m_parentIndex > static_cast<int>(i));
            assert(requested.m_parentIndex < static_cast<int>(x_numLoops));
            assert(requested.m_parentMult > 0);
            bool different = current.m_parentIndex != requested.m_parentIndex || current.m_parentMult != requested.m_parentMult;
            accept[i] = different && (!requireBoundaries ||
                (sample.m_loops[current.m_parentIndex].m_modulatedCycleCrossed &&
                 sample.m_loops[requested.m_parentIndex].m_modulatedCycleCrossed));
            changed = changed || accept[i];
        }

        // All eligibility checks use the old topology's boundary events.
        // Accept the requested parent and multiplier together.
        //
        for (size_t i = 0; i < x_globalLoop; ++i)
        {
            if (accept[i])
            {
                sample.m_loops[i].m_input = input.m_input[i];
            }
        }

        if (changed)
        {
            SetLoopPeriods(sample);
        }

        return changed;
    }

    static void SetPositions(Sample& sample)
    {
        int64_t globalPeriodTicks = sample.m_loops[x_globalLoop].m_periodTicks;
        sample.m_unmodulatedGlobalTickPosition =
            ComputeGlobalTickPosition(sample.m_unmodulatedPhase, globalPeriodTicks);
        sample.m_modulatedGlobalTickPosition =
            ComputeGlobalTickPosition(sample.m_modulatedPhase, globalPeriodTicks);
    }

    static void SetGates(Sample& sample, const Input& input)
    {
        if (sample.m_running)
        {
            for (size_t i = 0; i < x_numLoops; ++i)
            {
                if (sample.m_loops[i].m_modulatedCycleCrossed)
                {
                    int64_t loopCyclePosition = GetLoopCyclePosition(sample, i, input.m_rhythm[i].m_resetLoopIndex);
                    sample.m_loops[i].m_gate = input.m_rhythm[i].GateAt(loopCyclePosition);
                }
            }
        }
        else
        {
            for (size_t i = 0; i < x_numLoops; ++i)
            {
                sample.m_loops[i].m_gate = false;
            }
        }
    }

    static void SetCrossings(Sample& sample, double previousUnmodulatedPhase, double previousModulatedPhase, bool started)
    {
        int64_t globalPeriodTicks = sample.m_loops[x_globalLoop].m_periodTicks;
        int64_t previousUnmodulatedGlobalTickPosition =
            ComputeGlobalTickPosition(previousUnmodulatedPhase, globalPeriodTicks);
        int64_t previousModulatedGlobalTickPosition =
            ComputeGlobalTickPosition(previousModulatedPhase, globalPeriodTicks);
        sample.m_anyChange =
            started || sample.m_modulatedGlobalTickPosition != previousModulatedGlobalTickPosition;
        for (TimeLoop& loop : sample.m_loops)
        {
            loop.m_modulatedCycleCrossed = started ||
                PhaseUtils::FloorDiv(sample.m_modulatedGlobalTickPosition, loop.m_periodTicks)
                    != PhaseUtils::FloorDiv(previousModulatedGlobalTickPosition, loop.m_periodTicks);
            loop.m_unmodulatedCycleCrossed = started ||
                PhaseUtils::FloorDiv(sample.m_unmodulatedGlobalTickPosition, loop.m_periodTicks)
                    != PhaseUtils::FloorDiv(previousUnmodulatedGlobalTickPosition, loop.m_periodTicks);
        }
    }

    // Advances m_sample by one sample in place. Crossings compare against the
    // previous phases, saved before they are overwritten.
    //
    void Process(const Input& input)
    {
        Sample& sample = m_sample;
        bool wasRunning = sample.m_running;
        double previousUnmodulatedPhase = sample.m_unmodulatedPhase;
        double previousModulatedPhase = sample.m_modulatedPhase;
        sample.m_running = input.m_running;

        if (!input.m_running)
        {
            bool changed = AcceptTopology(sample, input, false);
            sample.m_unmodulatedPhase = 0.0;
            sample.m_modulatedPhase = 0.0;
            sample.m_modulatedPhaseIncrement = 0.0;
            SetPositions(sample);
            SetGates(sample, input);
            for (TimeLoop& loop : sample.m_loops)
            {
                loop.m_modulatedCycleCrossed = false;
                loop.m_unmodulatedCycleCrossed = false;
            }

            sample.m_anyChange = wasRunning || changed;
            m_phaseClock.Update(0.0, false, GetUnmodulatedGlobalPhaseIncrement());
            return;
        }

        bool started = !wasRunning;
        if (started)
        {
            AcceptTopology(sample, input, false);
        }

        sample.m_unmodulatedPhase = input.m_unmodulatedPhase;
        sample.m_modulatedPhase = input.m_unmodulatedPhase + input.m_phaseOffset;
        sample.m_modulatedPhaseIncrement = started
            ? GetUnmodulatedGlobalPhaseIncrement()
            : sample.m_modulatedPhase - previousModulatedPhase;
        SetPositions(sample);
        SetCrossings(sample, previousUnmodulatedPhase, previousModulatedPhase, started);

        if (!started && AcceptTopology(sample, input, true))
        {
            // Remap coordinates without turning a topology edit into elapsed travel.
            // Keep the crossing events that made this edit eligible.
            //
            SetPositions(sample);
            sample.m_anyChange = true;
        }

        SetGates(sample, input);
        m_phaseClock.Update(sample.m_modulatedPhase, true, GetUnmodulatedGlobalPhaseIncrement());
    }

    void PopulateUIState(TheoryOfTimeBaseUIState& uiState, const Input& input) const;
};
