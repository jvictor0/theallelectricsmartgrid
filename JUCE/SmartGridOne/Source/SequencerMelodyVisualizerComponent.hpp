#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

struct SequencerMelodyVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr size_t x_voicesPerTrio = TheNonagonInternal::x_voicesPerTrio;
    static constexpr int64_t x_maxSteps = 256;
    static constexpr int64_t x_overBudget = x_maxSteps + 1;
    static_assert(x_maxSteps <= TheNonagonUIState::Sequence::x_maxSize,
        "The visible window must fit in the sequence cache.");

    struct PitchSlice
    {
        std::array<float, HarmonicSheaf::x_numBasePoints> m_pitches{};
        size_t m_numPitches = 0;

        float InterpolatePercentile(float choice) const
        {
            assert(m_numPitches > 0);
            float octave = std::floor(choice);
            float rank = (choice - octave) * static_cast<float>(m_numPitches);
            size_t first = std::min(static_cast<size_t>(rank), m_numPitches - 1);
            size_t next = std::min(first + 1, m_numPitches - 1);
            float fraction = rank - static_cast<float>(first);
            // Match the chooser's i/N rank positions, holding the last rank until the next octave.
            //
            return m_pitches[first] + fraction * (m_pitches[next] - m_pitches[first]) + octave;
        }

        std::pair<float, float> GetPercentileRange(float minimum, float maximum) const
        {
            float first = InterpolatePercentile(minimum);
            float last = InterpolatePercentile(maximum);
            float low = std::min(first, last);
            float high = std::max(first, last);
            if (std::floor(minimum) < std::floor(maximum))
            {
                // Integer boundaries can jump downward when the sheaf spans more than an octave.
                //
                low = std::min(low, m_pitches[0] + std::floor(minimum) + 1.0f);
                high = std::max(high, m_pitches[m_numPitches - 1] + std::floor(maximum) - 1.0f);
            }

            return {low, high};
        }
    };

    struct Frame
    {
        size_t m_trioIndex = 0;
        std::array<size_t, x_voicesPerTrio> m_voices{};
        size_t m_numVoices = 0;
        size_t m_referenceVoice = 0;
        int64_t m_position = 0;
        double m_positionFraction = 0.0;
        int64_t m_start = 0;
        int64_t m_end = 1;
        HarmonicSheaf::Lens m_lens;
        float m_minPitch = 0.0f;
        float m_maxPitch = 1.0f;
        float m_minChoice = 0.0f;
        float m_maxChoice = 1.0f;
        bool m_choiceIsModOne = false;
        bool m_choiceIsPercentile = false;
        std::array<PitchSlice, HarmonicSheaf::x_numBasePoints> m_pitchSlices{};
    };

    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    int* m_voiceOffset;
    // The caller publishes the absolute modulated global phase in cycles.
    //
    const std::atomic<double>* m_globalPhase;

    SequencerMelodyVisualizerComponent(
        TheNonagonSquiggleBoyInternal::UIState* uiState,
        int* voiceOffset,
        const std::atomic<double>* globalPhase)
        : m_uiState(uiState)
        , m_voiceOffset(voiceOffset)
        , m_globalPhase(globalPhase)
    {
    }

    static int64_t CappedLcm(int64_t first, int64_t second)
    {
        assert(first > 0 && second > 0);
        if (first > x_maxSteps || second > x_maxSteps)
        {
            return x_overBudget;
        }

        int64_t factor = first / std::gcd(first, second);
        return factor > x_maxSteps / second ? x_overBudget : factor * second;
    }

    static int64_t GetLoopPeriod(const TheoryOfTimeBaseUIState& time, size_t loopIndex)
    {
        int64_t period = time.m_snapshotPeriodTicks[loopIndex];
        const auto& rhythm = time.m_rhythm[loopIndex].m_snapshot;
        int64_t resetPeriod = rhythm.m_resetLoopIndex < 0
            ? -1 : time.m_snapshotPeriodTicks[rhythm.m_resetLoopIndex];
        int64_t resetCycles = TheoryOfTimeBase::GetResetCycleCount(period, resetPeriod);
        int64_t size = rhythm.m_size;
        assert(size > 0 && size <= static_cast<int64_t>(TheoryOfTimeRhythm::x_maxSize));
        int64_t cycles = resetCycles > 0 ? resetCycles : size;
        bool constant = true;
        for (int64_t slot = 1; slot < std::min(cycles, size); ++slot)
        {
            constant = constant && rhythm.m_gate[slot] == rhythm.m_gate[0];
        }

        if (constant)
        {
            return 1;
        }

        for (int64_t candidate = 1; candidate <= std::min(cycles, x_maxSteps / period); ++candidate)
        {
            if (cycles % candidate != 0)
            {
                continue;
            }

            // Compare the reset prefix with a repeated candidate block. These two
            // sequences repeat together after lcm(size, candidate), even for huge resets.
            //
            int64_t checks = std::min(cycles, std::lcm(size, candidate));
            bool repeats = true;
            for (int64_t slot = candidate; slot < checks; ++slot)
            {
                if (rhythm.m_gate[slot % size] != rhythm.m_gate[(slot % candidate) % size])
                {
                    repeats = false;
                    break;
                }
            }

            if (repeats)
            {
                return period * candidate;
            }
        }

        return x_overBudget;
    }

    int64_t GetArpPeriod(const Frame& frame) const
    {
        const auto& state = m_uiState->m_nonagonUIState;
        const auto& arps = state.m_indexArpUIState;
        const auto& time = state.m_theoryOfTimeUIState;
        bool zeroPageIntervals = true;
        for (size_t index = 0; index < frame.m_numVoices; ++index)
        {
            const auto& arp = arps.m_arpUIState[frame.m_voices[index]].m_snapshot;
            zeroPageIntervals = zeroPageIntervals && arp.m_pageInterval == 0.0f;
        }

        int64_t result = 1;
        for (size_t index = 0; index < frame.m_numVoices; ++index)
        {
            size_t voice = frame.m_voices[index];
            int clock = arps.GetClockSelect(voice);
            if (clock < 0)
            {
                continue;
            }

            int64_t clockPeriod = time.m_snapshotPeriodTicks[clock];
            int reset = arps.GetResetSelect(voice);
            int64_t resetPeriod = reset < 0 ? -1 : time.m_snapshotPeriodTicks[reset];
            int64_t resetCycles = TheoryOfTimeBase::GetResetCycleCount(clockPeriod, resetPeriod);
            int64_t length = arps.m_arpUIState[voice].m_snapshot.m_rhythmLength;
            assert(length > 0 && length <= static_cast<int64_t>(IndexArp::x_rhythmLength));
            int64_t period;
            if (!zeroPageIntervals)
            {
                period = resetCycles > 0 ? resetPeriod : x_overBudget;
            }
            else if (resetCycles > 0 && resetCycles % length != 0)
            {
                period = resetPeriod;
            }
            else
            {
                period = clockPeriod > x_maxSteps / length ? x_overBudget : clockPeriod * length;
            }

            result = CappedLcm(result, period);
        }

        return result;
    }

    int64_t GetViewPeriod(const Frame& frame) const
    {
        const auto& time = m_uiState->m_nonagonUIState.m_theoryOfTimeUIState;
        std::array<int64_t, TheoryOfTimeBase::x_numLoops> periods;
        periods.fill(1);
        int64_t readPeriod = 1;
        for (size_t loop = 0; loop < periods.size(); ++loop)
        {
            if ((frame.m_lens.m_bits & (1 << loop)) != 0)
            {
                periods[loop] = GetLoopPeriod(time, loop);
                readPeriod = CappedLcm(readPeriod, periods[loop]);
            }
        }

        int64_t fullPeriod = CappedLcm(readPeriod, GetArpPeriod(frame));
        if (fullPeriod <= x_maxSteps)
        {
            return fullPeriod;
        }

        // First omit the arp, then omit rhythm lengths from local to global loops.
        // The sequence cache continues to evaluate the actual settings at every tick.
        //
        for (size_t loop = 0; readPeriod > x_maxSteps && loop < periods.size(); ++loop)
        {
            if (periods[loop] == 1)
            {
                continue;
            }

            periods[loop] = std::min(time.m_snapshotPeriodTicks[loop], x_overBudget);
            readPeriod = 1;
            for (int64_t period : periods)
            {
                readPeriod = CappedLcm(readPeriod, period);
            }
        }

        return readPeriod;
    }

    static std::pair<int64_t, int64_t> GetWindow(int64_t position, int64_t viewPeriodTicks)
    {
        assert(viewPeriodTicks > 0);
        if (viewPeriodTicks <= x_maxSteps)
        {
            int64_t start = position - PhaseUtils::FloorMod(position, viewPeriodTicks);
            return {start, start + viewPeriodTicks};
        }

        return {position - x_maxSteps / 2, position + x_maxSteps / 2};
    }

    void PrepareSequence(const Frame& frame, size_t voiceIndex, int64_t cachePeriod)
    {
        auto& state = m_uiState->m_nonagonUIState;
        const auto& sequence = state.m_sequences[voiceIndex];
        for (int64_t extended = 0; extended < x_maxSteps; extended += TheNonagonUIState::Sequence::x_maxExtend)
        {
            state.Process(frame.m_position, voiceIndex, cachePeriod);
            if (sequence.StartPosition() <= frame.m_start && sequence.EndPosition() >= frame.m_end)
            {
                return;
            }
        }

        assert(sequence.StartPosition() <= frame.m_start && sequence.EndPosition() >= frame.m_end);
    }

    bool PrepareFrame(Frame& frame)
    {
        frame = {};
        if (!m_uiState || !m_voiceOffset || !m_globalPhase)
        {
            return false;
        }

        auto& state = m_uiState->m_nonagonUIState;
        for (const auto& period : state.m_theoryOfTimeUIState.m_periodTicks)
        {
            if (period.load() <= 0)
            {
                return false;
            }
        }

        frame.m_trioIndex = m_uiState->m_squiggleBoyUIState.m_activeTrack.load();
        if (frame.m_trioIndex >= TheNonagonInternal::x_numTrios)
        {
            return false;
        }

        int voiceOffset = *m_voiceOffset;
        if (voiceOffset < -1 || voiceOffset >= static_cast<int>(x_voicesPerTrio))
        {
            return false;
        }

        if (state.Changed())
        {
            state.Snapshot();
        }

        double phase = m_globalPhase->load();
        int64_t globalPeriodTicks = state.m_theoryOfTimeUIState.GetGlobalPeriodTicks();
        frame.m_position = TheoryOfTimeBase::ComputeGlobalTickPosition(phase, globalPeriodTicks);
        frame.m_positionFraction = phase * static_cast<double>(globalPeriodTicks) - static_cast<double>(frame.m_position);
        size_t firstVoice = frame.m_trioIndex * x_voicesPerTrio;
        frame.m_referenceVoice = firstVoice + (voiceOffset < 0 ? 0 : static_cast<size_t>(voiceOffset));
        for (size_t channelIndex = 0; channelIndex < x_voicesPerTrio; ++channelIndex)
        {
            size_t voiceIndex = firstVoice + channelIndex;
            bool selected = voiceOffset == static_cast<int>(channelIndex);
            bool visibleInTrio = voiceOffset == -1 && !state.m_muted[voiceIndex].load();
            if (selected || visibleInTrio)
            {
                frame.m_voices[frame.m_numVoices++] = voiceIndex;
            }
        }

        auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
        auto& chooser = harmonic.m_voiceChooserState[frame.m_referenceVoice];
        frame.m_lens = chooser.m_snapshotLens;
        auto strategy = chooser.m_snapshotStrategy;
        frame.m_choiceIsModOne = strategy == HarmonicSheaf::SectionChoiceStrategy::ClosestModOne;
        frame.m_choiceIsPercentile = strategy == HarmonicSheaf::SectionChoiceStrategy::Percentile;
        int64_t viewPeriod = GetViewPeriod(frame);
        auto window = GetWindow(frame.m_position, viewPeriod);
        frame.m_start = window.first;
        frame.m_end = window.second;
        // Scrolling windows need a centered cache instead of a partial aligned cycle.
        //
        int64_t cachePeriod = viewPeriod <= x_maxSteps
            ? viewPeriod : TheNonagonUIState::Sequence::x_maxSize + 1;
        state.PreProcess(frame.m_position, cachePeriod);
        for (size_t index = 0; index < frame.m_numVoices; ++index)
        {
            PrepareSequence(frame, frame.m_voices[index], cachePeriod);
        }

        if (std::find(frame.m_voices.begin(), frame.m_voices.begin() + frame.m_numVoices,
                frame.m_referenceVoice) == frame.m_voices.begin() + frame.m_numVoices)
        {
            PrepareSequence(frame, frame.m_referenceVoice, cachePeriod);
        }

        SetRanges(frame);
        return true;
    }

    void PreparePitchSlices(Frame& frame)
    {
        auto& state = m_uiState->m_nonagonUIState;
        auto& harmonic = state.m_lameJuisUIState.m_harmonicSheafState;
        HarmonicSheaf::SectionChooser chooser;
        chooser.m_evaluator = harmonic.m_evaluatorState.m_snapshot;
        for (size_t voice = 0; voice <= frame.m_numVoices; ++voice)
        {
            size_t voiceIndex = voice == frame.m_numVoices ? frame.m_referenceVoice : frame.m_voices[voice];
            const auto& sequence = state.m_sequences[voiceIndex];
            int64_t start = std::max(frame.m_start, sequence.StartPosition());
            int64_t end = std::min(frame.m_end, sequence.EndPosition());
            for (int64_t position = start; position < end; ++position)
            {
                auto timeSlice = sequence.GetPoint(position).m_timeSlice;
                auto representative = frame.m_lens.Canonicalize(timeSlice);
                auto& pitches = frame.m_pitchSlices[representative.m_bits];
                if (pitches.m_numPitches != 0)
                {
                    continue;
                }

                pitches.m_numPitches = size_t{1} << (TheoryOfTimeBase::x_numLoops - frame.m_lens.CountSetBits());
                for (size_t rank = 0; rank < pitches.m_numPitches; ++rank)
                {
                    // Query the shared chooser at its exact rank boundaries, retaining duplicate sections.
                    //
                    float percentile = static_cast<float>(rank) / static_cast<float>(pitches.m_numPitches);
                    pitches.m_pitches[rank] = chooser.ChoosePercentile(
                        harmonic.m_sectionState.m_snapshot, frame.m_lens, timeSlice, percentile).m_value;
                }
            }
        }
    }

    void SetRanges(Frame& frame)
    {
        PreparePitchSlices(frame);
        if (frame.m_numVoices == 0)
        {
            return;
        }

        auto& state = m_uiState->m_nonagonUIState;
        frame.m_minChoice = std::numeric_limits<float>::max();
        frame.m_maxChoice = std::numeric_limits<float>::lowest();
        frame.m_minPitch = std::numeric_limits<float>::max();
        frame.m_maxPitch = std::numeric_limits<float>::lowest();
        for (size_t index = 0; index < frame.m_numVoices; ++index)
        {
            size_t voiceIndex = frame.m_voices[index];
            const auto& arp = state.m_indexArpUIState.m_arpUIState[voiceIndex].m_snapshot;
            float minimum = std::min(arp.m_min, arp.m_max);
            float maximum = std::max(arp.m_min, arp.m_max);
            frame.m_minChoice = std::min(frame.m_minChoice, minimum);
            frame.m_maxChoice = std::max(frame.m_maxChoice, maximum);
            if (frame.m_choiceIsPercentile)
            {
                for (const auto& pitches : frame.m_pitchSlices)
                {
                    if (pitches.m_numPitches > 0)
                    {
                        auto range = pitches.GetPercentileRange(minimum, maximum);
                        frame.m_minPitch = std::min(frame.m_minPitch, range.first);
                        frame.m_maxPitch = std::max(frame.m_maxPitch, range.second);
                    }
                }
            }
        }

        if (!frame.m_choiceIsPercentile)
        {
            frame.m_minPitch = frame.m_minChoice;
            frame.m_maxPitch = frame.m_maxChoice;
        }
    }

    static float TimeX(int64_t position, const Frame& frame, juce::Rectangle<float> plot)
    {
        return plot.getX() + plot.getWidth() * static_cast<float>(position - frame.m_start)
            / static_cast<float>(frame.m_end - frame.m_start);
    }

    static float PitchY(float pitch, const Frame& frame, juce::Rectangle<float> plot)
    {
        if (frame.m_minPitch == frame.m_maxPitch)
        {
            if (pitch == frame.m_minPitch)
            {
                return plot.getCentreY();
            }

            return pitch < frame.m_minPitch ? plot.getBottom() + plot.getHeight() : plot.getY() - plot.getHeight();
        }

        return plot.getBottom() - plot.getHeight() * (pitch - frame.m_minPitch)
            / (frame.m_maxPitch - frame.m_minPitch);
    }

    static float ChoiceY(float choice, const PitchSlice& pitches, const Frame& frame, juce::Rectangle<float> plot)
    {
        float pitch = frame.m_choiceIsPercentile ? pitches.InterpolatePercentile(choice) : choice;
        return PitchY(pitch, frame, plot);
    }

    static std::vector<float> GetPossiblePitches(const PitchSlice& pitches, const Frame& frame)
    {
        std::vector<float> result;
        for (size_t index = 0; index < pitches.m_numPitches; ++index)
        {
            float pitch = pitches.m_pitches[index];
            if (frame.m_choiceIsModOne || frame.m_choiceIsPercentile)
            {
                if (frame.m_choiceIsModOne)
                {
                    pitch -= std::floor(pitch);
                }

                int firstOctave = static_cast<int>(std::ceil(frame.m_minPitch - pitch));
                int lastOctave = static_cast<int>(std::floor(frame.m_maxPitch - pitch));
                if (frame.m_choiceIsPercentile)
                {
                    firstOctave = std::max(0, firstOctave);
                    lastOctave = std::min(lastOctave, static_cast<int>(std::floor(frame.m_maxChoice)));
                }

                for (int octave = firstOctave; octave <= lastOctave; ++octave)
                {
                    result.push_back(pitch + static_cast<float>(octave));
                }
            }
            else if (pitch >= frame.m_minPitch && pitch <= frame.m_maxPitch)
            {
                result.push_back(pitch);
            }
        }

        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    void DrawTimeSliceSeparators(juce::Graphics& g, const Frame& frame, juce::Rectangle<float> plot)
    {
        const auto& sequence = m_uiState->m_nonagonUIState.m_sequences[frame.m_referenceVoice];
        int64_t start = std::max(frame.m_start, sequence.StartPosition());
        int64_t end = std::min(frame.m_end, sequence.EndPosition());
        int64_t previous = start;
        for (int64_t position = start; position < end;)
        {
            auto timeSlice = frame.m_lens.Canonicalize(sequence.GetPoint(position).m_timeSlice);
            int64_t next = position + 1;
            while (next < end && frame.m_lens.Canonicalize(sequence.GetPoint(next).m_timeSlice) == timeSlice)
            {
                ++next;
            }

            if (position > start)
            {
                float x = TimeX(position, frame, plot);
                float spacing = std::min(x - TimeX(previous, frame, plot), TimeX(next, frame, plot) - x);
                float alpha = 0.07f * std::clamp((spacing - 3.0f) / 9.0f, 0.0f, 1.0f);
                if (alpha > 0.0f)
                {
                    g.setColour(juce::Colours::white.withAlpha(alpha));
                    g.drawLine(x, plot.getY(), x, plot.getBottom(), 0.75f);
                }
            }

            previous = position;
            position = next;
        }
    }

    void DrawSheaf(juce::Graphics& g, const Frame& frame, juce::Rectangle<float> plot)
    {
        DrawTimeSliceSeparators(g, frame, plot);
        auto& state = m_uiState->m_nonagonUIState;
        auto& sequence = state.m_sequences[frame.m_referenceVoice];
        int64_t end = std::min(frame.m_end, sequence.EndPosition());
        int64_t start = std::max(frame.m_start, sequence.StartPosition());
        auto trio = static_cast<TheNonagonSmartGrid::Trio>(frame.m_trioIndex);
        g.setColour(J(TheNonagonSmartGrid::TrioColor(trio)).withAlpha(0.3f));
        while (start < end)
        {
            auto timeSlice = sequence.GetPoint(start).m_timeSlice;
            auto representative = frame.m_lens.Canonicalize(timeSlice);
            int64_t next = start + 1;
            while (next < end && frame.m_lens.Canonicalize(sequence.GetPoint(next).m_timeSlice) == representative)
            {
                ++next;
            }

            const auto& pitches = frame.m_pitchSlices[representative.m_bits];
            for (float pitch : GetPossiblePitches(pitches, frame))
            {
                float y = PitchY(pitch, frame, plot);
                g.drawLine(TimeX(start, frame, plot), y, TimeX(next, frame, plot), y, 1.0f);
            }

            start = next;
        }
    }

    std::pair<int64_t, int64_t> GetMotive(const TheNonagonUIState::VoicePoint& point, size_t voiceIndex) const
    {
        const auto& state = m_uiState->m_nonagonUIState;
        const auto& arps = state.m_indexArpUIState;
        if (arps.GetClockSelect(voiceIndex) < 0)
        {
            return {0, 0};
        }

        const auto& arp = arps.m_arpUIState[voiceIndex].m_snapshot;
        auto held = IndexArp::GetForwardCoordinates(arp, point.m_loopCyclePosition, point.m_resetCycleCount);
        int64_t resetCycle = 0;
        if (point.m_resetCycleCount > 0)
        {
            int resetLoop = arps.GetResetSelect(voiceIndex);
            int64_t resetPeriod = state.m_theoryOfTimeUIState.m_snapshotPeriodTicks[resetLoop];
            resetCycle = PhaseUtils::FloorDiv(point.m_globalTickPosition, resetPeriod);
            auto current = IndexArp::GetCoordinates(arp, point.m_loopCyclePosition);
            if (std::make_pair(held.m_motivePosition, held.m_rhythmSlotIndex)
                > std::make_pair(current.m_motivePosition, current.m_rhythmSlotIndex))
            {
                // A leading rest may still display the previous reset cycle's final enabled note.
                //
                --resetCycle;
            }
        }

        return {resetCycle, held.m_motivePosition};
    }

    std::vector<juce::Path> BuildChoicePaths(const Frame& frame, juce::Rectangle<float> plot, size_t voiceIndex) const
    {
        const auto& sequence = m_uiState->m_nonagonUIState.m_sequences[voiceIndex];
        int64_t start = std::max(frame.m_start, sequence.StartPosition());
        int64_t end = std::min(frame.m_end, sequence.EndPosition());
        auto GetY = [&](const TheNonagonUIState::VoicePoint& point)
        {
            const auto& pitches = frame.m_pitchSlices[frame.m_lens.Canonicalize(point.m_timeSlice).m_bits];
            return ChoiceY(point.m_choiceValue, pitches, frame, plot);
        };

        std::vector<juce::Path> paths;
        bool startSubPath = true;
        for (int64_t position = start; position < end; ++position)
        {
            auto point = sequence.GetPoint(position);
            float x = TimeX(position, frame, plot);
            float nextX = TimeX(position + 1, frame, plot);
            float y = GetY(point);
            if (startSubPath)
            {
                paths.emplace_back();
                paths.back().startNewSubPath(x, y);
            }

            auto& path = paths.back();
            bool hasNext = position + 1 < end;
            startSubPath = true;
            if (hasNext)
            {
                auto nextPoint = sequence.GetPoint(position + 1);
                if (GetMotive(point, voiceIndex) == GetMotive(nextPoint, voiceIndex))
                {
                    float nextY = GetY(nextPoint);
                    float midpoint = (x + nextX) * 0.5f;
                    path.lineTo(midpoint, y);
                    int segments = std::clamp(static_cast<int>(std::ceil(nextX - midpoint)), 2, 24);
                    for (int segment = 1; segment <= segments; ++segment)
                    {
                        float fraction = static_cast<float>(segment) / static_cast<float>(segments);
                        float blend = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * fraction);
                        path.lineTo(midpoint + (nextX - midpoint) * fraction, y + (nextY - y) * blend);
                    }

                    startSubPath = false;
                }
            }

            if (startSubPath)
            {
                float gap = hasNext ? std::min(2.0f, (nextX - x) * 0.15f) : 0.0f;
                path.lineTo(nextX - gap, y);
            }
        }

        return paths;
    }

    void DrawVoice(juce::Graphics& g, const Frame& frame, juce::Rectangle<float> plot, size_t voiceIndex)
    {
        auto& state = m_uiState->m_nonagonUIState;
        const auto& sequence = state.m_sequences[voiceIndex];
        auto colour = J(TheNonagonSmartGrid::VoiceColor(voiceIndex));
        int64_t start = std::max(frame.m_start, sequence.StartPosition());
        int64_t end = std::min(frame.m_end, sequence.EndPosition());
        g.setColour(colour);
        for (int64_t position = start; position < end;)
        {
            auto point = sequence.GetPoint(position);
            int64_t next = position + 1;
            while (next < end && sequence.GetPoint(next).m_pitch.m_value == point.m_pitch.m_value)
            {
                ++next;
            }

            float y = PitchY(point.m_pitch.m_value, frame, plot);
            g.drawLine(TimeX(position, frame, plot), y, TimeX(next, frame, plot), y, 2.5f);
            position = next;
        }

        constexpr float x_dashes[] = {4.0f, 3.0f};
        g.setColour(colour.withAlpha(0.7f));
        // Dash each motive separately because JUCE can reconnect open subpaths while dashing.
        //
        for (const auto& path : BuildChoicePaths(frame, plot, voiceIndex))
        {
            juce::Path dashed;
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, path, x_dashes, 2);
            g.fillPath(dashed);
        }
    }

    void DrawReadRows(juce::Graphics& g, const Frame& frame, juce::Rectangle<float> rows, float rowHeight)
    {
        const auto& sequence = m_uiState->m_nonagonUIState.m_sequences[frame.m_referenceVoice];
        int64_t start = std::max(frame.m_start, sequence.StartPosition());
        int64_t end = std::min(frame.m_end, sequence.EndPosition());
        size_t rowIndex = 0;
        for (size_t loopIndex = 0; loopIndex < TheoryOfTimeBase::x_numLoops; ++loopIndex)
        {
            if ((frame.m_lens.m_bits & (1 << loopIndex)) == 0)
            {
                continue;
            }

            float y = rows.getY() + static_cast<float>(rowIndex++) * rowHeight;
            for (int64_t position = start; position < end;)
            {
                bool gate = sequence.GetPoint(position).m_timeSlice.Get(loopIndex);
                int64_t next = position + 1;
                while (next < end && sequence.GetPoint(next).m_timeSlice.Get(loopIndex) == gate)
                {
                    ++next;
                }

                float x = TimeX(position, frame, rows);
                float width = TimeX(next, frame, rows) - x;
                auto rectangle = juce::Rectangle<float>(x, y + 1.5f, width, rowHeight - 3.0f);
                float radius = std::min(3.0f, width * 0.25f);
                g.setColour(gate ? juce::Colour(0xff707070) : juce::Colours::black);
                g.fillRoundedRectangle(rectangle, radius);
                g.setColour(juce::Colours::grey.withAlpha(0.35f));
                g.drawRoundedRectangle(rectangle, radius, 0.6f);
                position = next;
            }
        }
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        juce::Graphics::ScopedSaveState savedState(g);
        g.reduceClipRegion(boundsRect);
        g.setColour(juce::Colours::black);
        g.fillRect(boundsRect);
        Frame frame;
        if (boundsRect.getWidth() < 140 || boundsRect.getHeight() < 100 || !PrepareFrame(frame))
        {
            return;
        }

        auto bounds = boundsRect.toFloat().reduced(8.0f);
        auto lens = frame.m_lens;
        float rowHeight = std::clamp(bounds.getHeight() * 0.045f, 10.0f, 20.0f);
        float rowsHeight = static_cast<float>(lens.CountSetBits()) * rowHeight;
        float rowGap = rowsHeight > 0.0f ? 8.0f : 0.0f;
        auto plot = bounds.withTrimmedBottom(rowsHeight + rowGap);
        if (plot.getHeight() < 20.0f)
        {
            return;
        }

        auto rows = juce::Rectangle<float>(plot.getX(), plot.getBottom() + rowGap, plot.getWidth(), rowsHeight);
        int firstDivision = static_cast<int>(std::ceil(frame.m_minPitch * 4.0f));
        int lastDivision = static_cast<int>(std::floor(frame.m_maxPitch * 4.0f));
        for (int division = firstDivision; division <= lastDivision; ++division)
        {
            float pitch = static_cast<float>(division) * 0.25f;
            float y = PitchY(pitch, frame, plot);
            bool octave = division % 4 == 0;
            bool tritone = division % 2 == 0;
            float alpha = octave ? 0.40f : tritone ? 0.25f : 0.15f;
            float thickness = octave ? 1.25f : 1.0f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(plot.getX(), y, plot.getRight(), y, thickness);
        }

        {
            juce::Graphics::ScopedSaveState plotState(g);
            g.reduceClipRegion(plot.toNearestInt());
            DrawSheaf(g, frame, plot);
            for (size_t index = 0; index < frame.m_numVoices; ++index)
            {
                DrawVoice(g, frame, plot, frame.m_voices[index]);
            }
        }

        DrawReadRows(g, frame, rows, rowHeight);
        float playheadX = TimeX(frame.m_position, frame, plot)
            + plot.getWidth() * static_cast<float>(frame.m_positionFraction)
                / static_cast<float>(frame.m_end - frame.m_start);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawLine(playheadX, plot.getY(), playheadX, rows.getBottom(), 1.0f);
        g.fillEllipse(playheadX - 2.0f, plot.getY() - 2.0f, 4.0f, 4.0f);
    }

    void OnClick(const juce::MouseEvent&) override
    {
        if (m_voiceOffset)
        {
            *m_voiceOffset = (*m_voiceOffset + 2) % static_cast<int>(x_voicesPerTrio + 1) - 1;
        }
    }
};
