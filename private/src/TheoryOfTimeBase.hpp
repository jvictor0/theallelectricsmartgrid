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

enum class PhaseDomain
{
    Unmodulated,
    Modulated
};

struct TimeLoop
{
    struct Input
    {
        int m_parentIndex = 0;
        int m_parentMult = 2;
    };

    Input m_input;
    int64_t m_cycleRatio = 1;
    int64_t m_periodTicks = 2;
    bool m_gate = false;
    bool m_gateStepChanged = false;
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
        int64_t m_unmodulatedPosition = 0;
        int64_t m_modulatedPosition = 0;
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

    static int64_t Position(double phase, int64_t periodTicks)
    {
        double position = std::floor(phase * static_cast<double>(periodTicks));
        constexpr double x_minPosition = static_cast<double>(std::numeric_limits<int64_t>::min());
        assert(std::isfinite(position) && position >= x_minPosition && position < -x_minPosition);
        std::ignore = x_minPosition;
        return static_cast<int64_t>(position);
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

    int64_t GetPosition(size_t sampleIndex, PhaseDomain domain) const
    {
        assert(sampleIndex < x_microBlockBufferSize);
        const Sample& sample = m_samples[sampleIndex];
        return domain == PhaseDomain::Unmodulated ? sample.m_unmodulatedPosition : sample.m_modulatedPosition;
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

    int64_t GetGateStepIndex(size_t loopIndex, size_t sampleIndex, int resetLoopIndex = -1) const
    {
        assert(resetLoopIndex >= -1 && resetLoopIndex < static_cast<int>(x_numLoops));
        int64_t stepTicks = GetPeriodTicks(loopIndex, sampleIndex) / 2;
        int64_t index = PhaseUtils::FloorDiv(GetPosition(sampleIndex, PhaseDomain::Modulated), stepTicks);
        int ancestor = static_cast<int>(loopIndex);
        while (ancestor < static_cast<int>(x_numLoops))
        {
            if (ancestor == resetLoopIndex)
            {
                return PhaseUtils::FloorMod(index, GetPeriodTicks(ancestor, sampleIndex) / stepTicks);
            }

            ancestor = GetLoop(ancestor, sampleIndex).m_input.m_parentIndex;
        }

        return index;
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

    bool AnyGateStepChanged(size_t loopIndex) const
    {
        for (size_t j = 0; j < x_microBlockSize; ++j)
        {
            if (GetLoop(loopIndex, j).m_gateStepChanged)
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

        int64_t globalPeriod = 2 * commonRatio;
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

    static void SetPositionsAndGates(Sample& sample)
    {
        int64_t globalPeriod = sample.m_loops[x_globalLoop].m_periodTicks;
        sample.m_unmodulatedPosition = Position(sample.m_unmodulatedPhase, globalPeriod);
        sample.m_modulatedPosition = Position(sample.m_modulatedPhase, globalPeriod);
        for (TimeLoop& loop : sample.m_loops)
        {
            loop.m_gate = sample.m_running && PhaseUtils::FloorMod(sample.m_modulatedPosition, loop.m_periodTicks) < loop.m_periodTicks / 2;
        }
    }

    static void SetCrossings(Sample& sample, const Sample& previous, bool started)
    {
        int64_t globalPeriod = sample.m_loops[x_globalLoop].m_periodTicks;
        int64_t previousUnmodulated = Position(previous.m_unmodulatedPhase, globalPeriod);
        int64_t previousModulated = Position(previous.m_modulatedPhase, globalPeriod);
        sample.m_anyChange = started || sample.m_modulatedPosition != previousModulated;
        for (TimeLoop& loop : sample.m_loops)
        {
            loop.m_gateStepChanged = started ||
                PhaseUtils::FloorDiv(sample.m_modulatedPosition, loop.m_periodTicks / 2) != PhaseUtils::FloorDiv(previousModulated, loop.m_periodTicks / 2);
            loop.m_modulatedCycleCrossed = started ||
                PhaseUtils::FloorDiv(sample.m_modulatedPosition, loop.m_periodTicks) != PhaseUtils::FloorDiv(previousModulated, loop.m_periodTicks);
            loop.m_unmodulatedCycleCrossed = started ||
                PhaseUtils::FloorDiv(sample.m_unmodulatedPosition, loop.m_periodTicks) != PhaseUtils::FloorDiv(previousUnmodulated, loop.m_periodTicks);

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
            SetPositionsAndGates(sample);
            for (TimeLoop& loop : sample.m_loops)
            {
                loop.m_gateStepChanged = false;
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
        SetPositionsAndGates(sample);
        SetCrossings(sample, previous, started);

        if (!started && AcceptTopology(sample, input, true))
        {
            // Remap coordinates without turning a topology edit into elapsed travel.
            // Keep the crossing events that made this edit eligible.
            //
            SetPositionsAndGates(sample);
            sample.m_anyChange = true;
        }
    }
};
