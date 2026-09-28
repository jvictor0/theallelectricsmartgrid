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
#include "SampleTop.hpp"
#include "TheoryOfTimeRhythm.hpp"

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
    SampleTop m_unmodulatedCycleCrossed;
    SampleTop m_modulatedCycleCrossed;
};

struct TheoryOfTimeBase
{
    static constexpr size_t x_numLoops = 6;
    static constexpr size_t x_globalLoop = x_numLoops - 1;
    static constexpr size_t x_microBlockSize = SampleTimer::x_controlFrameRate;
    static constexpr size_t x_microBlockBufferSize = x_microBlockSize + 1;

    struct Sample
    {
        double m_unmodulatedPhase = 0.0;
        double m_modulatedPhase = 0.0;
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

    std::array<Sample, x_microBlockBufferSize> m_samples;

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

    void RolloverMicroblockBuffer()
    {
        m_samples[0] = m_samples[x_microBlockSize];
    }

    const TimeLoop& GetLoop(size_t loopIndex, size_t sampleIndex) const
    {
        assert(loopIndex < x_numLoops && sampleIndex < x_microBlockBufferSize);
        return m_samples[sampleIndex].m_loops[loopIndex];
    }

    double GetPhase(size_t loopIndex, double samplePosition, PhaseDomain domain) const
    {
        assert(loopIndex < x_numLoops);
        assert(std::isfinite(samplePosition) && samplePosition >= 0.0 && samplePosition <= x_microBlockSize);
        size_t first = static_cast<size_t>(std::floor(samplePosition));
        size_t second = std::min(first + 1, x_microBlockSize);
        const Sample& before = m_samples[first];
        const Sample& after = m_samples[second];
        double start = domain == PhaseDomain::Unmodulated ? before.m_unmodulatedPhase : before.m_modulatedPhase;
        double end = domain == PhaseDomain::Unmodulated ? after.m_unmodulatedPhase : after.m_modulatedPhase;
        double globalPhase = start + (end - start) * (samplePosition - static_cast<double>(first));

        // Apply the interval's topology after interpolation so an edit cannot sweep
        // through the integer cycles between two differently mapped loop phases.
        //
        return globalPhase * static_cast<double>(before.m_loops[loopIndex].m_cycleRatio);
    }

    int64_t GetGlobalTickPosition(size_t sampleIndex, PhaseDomain domain) const
    {
        assert(sampleIndex < x_microBlockBufferSize);
        const Sample& sample = m_samples[sampleIndex];
        return domain == PhaseDomain::Unmodulated
            ? sample.m_unmodulatedGlobalTickPosition
            : sample.m_modulatedGlobalTickPosition;
    }

    int64_t GetPeriodTicks(size_t loopIndex, size_t sampleIndex) const
    {
        return GetLoop(loopIndex, sampleIndex).m_periodTicks;
    }

    int64_t GetCycleRatio(size_t loopIndex, size_t sampleIndex) const
    {
        return GetLoop(loopIndex, sampleIndex).m_cycleRatio;
    }

    SampleTop CrossedCycleBoundary(size_t loopIndex, size_t sampleIndex, PhaseDomain domain) const
    {
        const TimeLoop& loop = GetLoop(loopIndex, sampleIndex);
        return domain == PhaseDomain::Unmodulated ? loop.m_unmodulatedCycleCrossed : loop.m_modulatedCycleCrossed;
    }

    static int64_t GetLoopCyclePosition(const Sample& sample, size_t loopIndex, int resetLoopIndex)
    {
        assert(loopIndex < x_numLoops);
        assert(resetLoopIndex >= -1 && resetLoopIndex < static_cast<int>(x_numLoops));
        int64_t loopPeriodTicks = sample.m_loops[loopIndex].m_periodTicks;
        int64_t resetPeriodTicks = resetLoopIndex == -1 ? -1 : sample.m_loops[resetLoopIndex].m_periodTicks;
        int64_t globalTickPosition = sample.m_modulatedGlobalTickPosition;
        return GetLoopCyclePosition(loopPeriodTicks, resetPeriodTicks, globalTickPosition);
    }

    static int64_t GetLoopCyclePosition(
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

    int64_t GetLoopCyclePosition(size_t loopIndex, size_t sampleIndex, int resetLoopIndex) const
    {
        assert(sampleIndex < x_microBlockBufferSize);
        return GetLoopCyclePosition(m_samples[sampleIndex], loopIndex, resetLoopIndex);
    }

    int64_t GetLoopCyclePosition(size_t loopIndex, size_t sampleIndex) const
    {
        return GetLoopCyclePosition(loopIndex, sampleIndex, -1);
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

    int64_t GetResetCycleCount(size_t loopIndex, size_t sampleIndex, int resetLoopIndex) const
    {
        assert(resetLoopIndex >= -1 && resetLoopIndex < static_cast<int>(x_numLoops));
        return GetResetCycleCount(
            GetPeriodTicks(loopIndex, sampleIndex),
            resetLoopIndex == -1 ? -1 : GetPeriodTicks(resetLoopIndex, sampleIndex));
    }

    bool AnyChangeInMicroBlock() const
    {
        for (size_t j = 0; j < x_microBlockSize; ++j)
        {
            if (m_samples[j].m_anyChange)
            {
                return true;
            }
        }

        return false;
    }

    bool AnyTick(size_t loopIndex) const
    {
        for (size_t j = 0; j < x_microBlockSize; ++j)
        {
            if (GetLoop(loopIndex, j).m_modulatedCycleCrossed)
            {
                return true;
            }
        }

        return false;
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

    static void SetCrossings(Sample& sample, const Sample& previous, bool started)
    {
        int64_t globalPeriodTicks = sample.m_loops[x_globalLoop].m_periodTicks;
        int64_t previousUnmodulatedGlobalTickPosition =
            ComputeGlobalTickPosition(previous.m_unmodulatedPhase, globalPeriodTicks);
        int64_t previousModulatedGlobalTickPosition =
            ComputeGlobalTickPosition(previous.m_modulatedPhase, globalPeriodTicks);
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

            loop.m_modulatedCycleCrossed.InterpolatePhases(previous.m_modulatedPhase, sample.m_modulatedPhase, loop.m_cycleRatio, started);
            loop.m_unmodulatedCycleCrossed.InterpolatePhases(previous.m_unmodulatedPhase, sample.m_unmodulatedPhase, loop.m_cycleRatio, started);
        }
    }

    void Process(size_t sampleIndex, const Input& input)
    {
        assert(sampleIndex > 0 && sampleIndex < x_microBlockBufferSize);
        const Sample& previous = m_samples[sampleIndex - 1];
        Sample& sample = m_samples[sampleIndex];
        sample = previous;
        sample.m_running = input.m_running;

        if (!input.m_running)
        {
            bool changed = AcceptTopology(sample, input, false);
            sample.m_unmodulatedPhase = 0.0;
            sample.m_modulatedPhase = 0.0;
            SetPositions(sample);
            SetGates(sample, input);
            for (TimeLoop& loop : sample.m_loops)
            {
                loop.m_modulatedCycleCrossed = false;
                loop.m_unmodulatedCycleCrossed = false;
            }

            sample.m_anyChange = previous.m_running || changed;
            if (sampleIndex == x_microBlockSize)
            {
                bool anyChange = false;
                for (size_t j = 1; j <= x_microBlockSize; ++j)
                {
                    anyChange = anyChange || m_samples[j].m_anyChange;
                }

                m_samples[0] = sample;
                m_samples[0].m_anyChange = anyChange;
            }

            return;
        }

        bool started = !previous.m_running;
        if (started)
        {
            AcceptTopology(sample, input, false);
        }

        sample.m_unmodulatedPhase = input.m_unmodulatedPhase;
        sample.m_modulatedPhase = input.m_unmodulatedPhase + input.m_phaseOffset;
        SetPositions(sample);
        SetCrossings(sample, previous, started);

        if (!started && AcceptTopology(sample, input, true))
        {
            // Remap coordinates without turning a topology edit into elapsed travel.
            // Keep the crossing events that made this edit eligible.
            //
            SetPositions(sample);
            sample.m_anyChange = true;
        }

        SetGates(sample, input);
    }

    void PopulateUIState(TheoryOfTimeBaseUIState& uiState, const Input& input) const;
};
