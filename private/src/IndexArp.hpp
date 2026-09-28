#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include "PhaseUtils.hpp"

struct IndexArp
{
    static constexpr size_t x_rhythmLength = 8;
    
    int64_t m_clockPosition;
    int64_t m_noteIndex;
    int64_t m_motivePosition;
    int64_t m_rhythmSlotIndex;
    float m_choiceValue;
    bool m_triggered;

    IndexArp()
        : m_clockPosition(-1)
        , m_noteIndex(0)
        , m_motivePosition(0)
        , m_rhythmSlotIndex(-1)
        , m_choiceValue(0.0f)
        , m_triggered(false)
    {
    }
    
    struct Input
    {
        bool m_clock;
        bool m_read;
        bool m_noClock;

        int64_t m_clockPosition;

        float m_offset;
        float m_interval;
        float m_min;
        float m_max;
        bool m_invert;
        bool m_retro;
        bool m_cycle;
        float m_pageInterval;
        bool m_rhythm[x_rhythmLength];
        int m_rhythmLength;

        Input()
            : m_clock(false)
            , m_read(false)
            , m_noClock(false)
            , m_clockPosition(0)
            , m_offset(0)
            , m_interval(0)
            , m_min(0)
            , m_max(0)
            , m_invert(false)
            , m_retro(false)
            , m_cycle(false)
            , m_pageInterval(0)
            , m_rhythm{}
            , m_rhythmLength(x_rhythmLength)
        {
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                m_rhythm[i] = true;
            }
        }      

        float GetChoiceValue(int64_t noteIndex, int64_t motivePosition) const
        {
            noteIndex = GetPhysicalNoteIndex(noteIndex);
            double result = static_cast<double>(m_offset) + noteIndex * static_cast<double>(m_interval)
                + static_cast<double>(motivePosition) * m_pageInterval;
            if (m_cycle)
            {
                result = result - 2 * std::floor(result / 2);
                if (result > 1)
                {
                    result = 2 - result;
                }
            }
            else
            {
                result = result - std::floor(result);
            }

            if (m_invert)
            {
                result = 1 - result;
            }

            result = m_min + result * (m_max - m_min);
            return static_cast<float>(result);
        }                 

        int64_t GetPhysicalNoteIndex(int64_t noteIndex) const
        {
            if (m_retro)
            {
                int64_t numEnabledStoredSlots = GetNumEnabledStoredSlots();
                return numEnabledStoredSlots - noteIndex;
            }
            else
            {
                return noteIndex;
            }
        }

        int64_t GetNumEnabledStoredSlots() const
        {
            int64_t result = 0;
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                if (m_rhythm[i])
                {
                    ++result;
                }
            }
            return result;
        }
    };

    struct Coordinates
    {
        int64_t m_rhythmSlotIndex;
        int64_t m_motivePosition;
        int64_t m_noteIndex;
    };

    void Process(Input& input)
    {
        m_triggered = false;

        if (input.m_noClock)
        {
            Reset();
        }

        if (input.m_clock)
        {
            ProcessClock(input);
        }

        if (input.m_read || m_triggered)
        {
            m_choiceValue = input.GetChoiceValue(m_noteIndex, m_motivePosition);
        }
    }

    void ProcessClock(Input& input)
    {
        m_clockPosition = input.m_clockPosition;
        Coordinates coordinates = GetCoordinates(input, m_clockPosition);
        m_rhythmSlotIndex = coordinates.m_rhythmSlotIndex;

        if (input.m_rhythm[m_rhythmSlotIndex])
        {
            m_noteIndex = coordinates.m_noteIndex;
            m_motivePosition = coordinates.m_motivePosition;
            m_triggered = true;
        }
    }

    static Coordinates GetCoordinates(const Input& input, int64_t clockPosition)
    {
        Coordinates result;
        result.m_rhythmSlotIndex = static_cast<int64_t>(PhaseUtils::FloorMod(clockPosition, input.m_rhythmLength));
        result.m_motivePosition = PhaseUtils::FloorDiv(clockPosition, input.m_rhythmLength);

        result.m_noteIndex = -1;
        for (int64_t i = 0; i <= result.m_rhythmSlotIndex; ++i)
        {
            if (input.m_rhythm[i])
            {
                ++result.m_noteIndex;
            }
        }

        return result;
    }

    static Coordinates GetForwardCoordinates(const Input& input, int64_t clockPosition, int64_t resetCycleCount)
    {
        Coordinates current = GetCoordinates(input, clockPosition);
        // A reset can join the trailing and leading rests of two partial motives.
        //
        int64_t lookback = resetCycleCount > 0
            ? std::min<int64_t>(2 * input.m_rhythmLength, resetCycleCount)
            : input.m_rhythmLength;
        for (int64_t distance = 0; distance < lookback; ++distance)
        {
            Coordinates candidate;
            if (resetCycleCount > 0 && clockPosition < distance)
            {
                candidate = GetCoordinates(input, resetCycleCount - (distance - clockPosition));
            }
            else
            {
                // Subtract within the rhythm so signed clock positions cannot underflow.
                //
                candidate = GetCoordinates(input, current.m_rhythmSlotIndex - distance);
                candidate.m_motivePosition += current.m_motivePosition;
            }

            if (input.m_rhythm[candidate.m_rhythmSlotIndex])
            {
                return candidate;
            }
        }

        return Coordinates{current.m_rhythmSlotIndex, 0, 0};
    }

    void Reset()
    {
        m_noteIndex = 0;
        m_motivePosition = 0;
        m_rhythmSlotIndex = 0;
    }

    struct UIState
    {
        std::atomic<float> m_offset;
        std::atomic<float> m_interval;
        std::atomic<float> m_min;
        std::atomic<float> m_max;
        std::atomic<bool> m_invert;
        std::atomic<bool> m_retro;
        std::atomic<bool> m_cycle;
        std::atomic<float> m_pageInterval;
        std::atomic<bool> m_rhythm[x_rhythmLength];
        std::atomic<int> m_rhythmLength;

        Input m_snapshot;

        UIState()
            : m_offset(0)
            , m_interval(0)
            , m_min(0)
            , m_max(0)
            , m_invert(false)
            , m_retro(false)
            , m_cycle(false)
            , m_pageInterval(0)
            , m_rhythm{}
            , m_rhythmLength(x_rhythmLength)
        {
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                m_rhythm[i] = true;
            }
        }

        void Snapshot()
        {
            m_snapshot.m_offset = m_offset.load();
            m_snapshot.m_interval = m_interval.load();
            m_snapshot.m_min = m_min.load();
            m_snapshot.m_max = m_max.load();
            m_snapshot.m_invert = m_invert.load();
            m_snapshot.m_retro = m_retro.load();
            m_snapshot.m_cycle = m_cycle.load();
            m_snapshot.m_pageInterval = m_pageInterval.load();
            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                m_snapshot.m_rhythm[i] = m_rhythm[i].load();
            }

            m_snapshot.m_rhythmLength = m_rhythmLength.load();
        }

        bool Changed() const
        {
            if (m_offset.load() != m_snapshot.m_offset
                || m_interval.load() != m_snapshot.m_interval
                || m_min.load() != m_snapshot.m_min
                || m_max.load() != m_snapshot.m_max
                || m_invert.load() != m_snapshot.m_invert
                || m_retro.load() != m_snapshot.m_retro
                || m_cycle.load() != m_snapshot.m_cycle
                || m_pageInterval.load() != m_snapshot.m_pageInterval
                || m_rhythmLength.load() != m_snapshot.m_rhythmLength)
            {
                return true;
            }

            for (size_t i = 0; i < x_rhythmLength; ++i)
            {
                if (m_rhythm[i].load() != m_snapshot.m_rhythm[i])
                {
                    return true;
                }
            }

            return false;
        }

        float GetChoiceValue(int64_t clockPosition) const
        {
            return GetChoiceValue(clockPosition, 0);
        }

        float GetChoiceValue(int64_t clockPosition, int64_t resetCycleCount) const
        {
            Coordinates coordinates = GetForwardCoordinates(m_snapshot, clockPosition, resetCycleCount);
            return m_snapshot.GetChoiceValue(coordinates.m_noteIndex, coordinates.m_motivePosition);
        }

        float GetResetChoiceValue() const
        {
            return m_snapshot.GetChoiceValue(0, 0);
        }
    };

    void PopulateUIState(UIState* uiState, const Input& input)
    {
        uiState->m_offset.store(input.m_offset);
        uiState->m_interval.store(input.m_interval);
        uiState->m_min.store(input.m_min);
        uiState->m_max.store(input.m_max);
        uiState->m_invert.store(input.m_invert);
        uiState->m_retro.store(input.m_retro);
        uiState->m_cycle.store(input.m_cycle);
        uiState->m_pageInterval.store(input.m_pageInterval);
        for (size_t i = 0; i < x_rhythmLength; ++i)
        {
            uiState->m_rhythm[i].store(input.m_rhythm[i]);
        }

        uiState->m_rhythmLength.store(input.m_rhythmLength);
    }
};

struct NonagonIndexArp
{
    static constexpr size_t x_numClocks = 7;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;
    static constexpr size_t x_numVoices = 9;

    IndexArp m_arp[x_numVoices];
    
    struct Input
    {
        IndexArp::Input m_input[x_numVoices];
        bool m_clocks[x_numClocks];
        int m_clockSelect[x_numTrios];
        int m_resetSelect[x_numTrios];

        float m_zoneHeight[x_numVoices];
        float m_zoneOverlap[x_numVoices];
        float m_offset[x_numVoices];
        float m_interval[x_numVoices];
        float m_pageInterval[x_numVoices];
        
        bool m_invert[x_numVoices];
        bool m_retro[x_numVoices];
        bool m_cycle[x_numVoices];        

        int64_t m_clockPosition[x_numTrios];

        void SetTrioInputs()
        {
            for (size_t i = 0; i < x_numTrios; ++i)
            {                
                for (size_t j = 0; j < x_voicesPerTrio; ++j)
                {
                    if (j == 0)
                    {
                        m_input[i * x_voicesPerTrio + j].m_min = 0;
                    }
                    else 
                    {
                        m_input[i * x_voicesPerTrio + j].m_min = m_input[i * x_voicesPerTrio + j - 1].m_max - m_zoneHeight[i * x_voicesPerTrio + j] * m_zoneOverlap[i * x_voicesPerTrio + j];
                    }

                    m_input[i * x_voicesPerTrio + j].m_max = m_input[i * x_voicesPerTrio + j].m_min + m_zoneHeight[i * x_voicesPerTrio + j];

                    m_input[i * x_voicesPerTrio + j].m_clockPosition = m_clockPosition[i];
                }
            }
                    
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_input[i].m_offset = m_offset[i];
                m_input[i].m_interval = m_interval[i];
                m_input[i].m_pageInterval = m_pageInterval[i];
                m_input[i].m_invert = m_invert[i];
                m_input[i].m_retro = m_retro[i];
                m_input[i].m_cycle = m_cycle[i];
            }
        }

        Input()
            : m_input{}
            , m_clocks{}
            , m_clockSelect{}
            , m_resetSelect{}
            , m_zoneHeight{}
            , m_zoneOverlap{}
            , m_offset{}
            , m_interval{}
            , m_pageInterval{}
            , m_invert{}
            , m_retro{}
            , m_cycle{}
            , m_clockPosition{}
        {
            for (size_t i = 0; i < x_numClocks; ++i)
            {
                m_clocks[i] = false;
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_clockSelect[i] = 0;
                m_resetSelect[i] = -1;
                m_clockPosition[i] = 0;
            }
        }

        void SetClocks()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                size_t j = i / x_voicesPerTrio;
                if (m_clockSelect[j] >= 0)
                {
                    m_input[i].m_clock = m_clocks[m_clockSelect[j]];
                    m_input[i].m_noClock = false;
                }
                else
                {
                    m_input[i].m_noClock = m_input[i].m_read;
                    m_input[i].m_clock = false;
                }
            }
        }
    };

    struct UIState
    {
        IndexArp::UIState m_arpUIState[x_numVoices];
        std::atomic<int> m_clockSelect[x_numTrios];
        std::atomic<int> m_resetSelect[x_numTrios];

        int m_snapshotClockSelect[x_numTrios];
        int m_snapshotResetSelect[x_numTrios];

        UIState()
            : m_arpUIState{}
            , m_clockSelect{}
            , m_resetSelect{}
            , m_snapshotClockSelect{}
            , m_snapshotResetSelect{}
        {
            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_clockSelect[i].store(0);
                m_resetSelect[i].store(-1);
                m_snapshotClockSelect[i] = -2;
                m_snapshotResetSelect[i] = -2;
            }
        }

        bool Changed() const
        {
            for (size_t i = 0; i < x_numTrios; ++i)
            {
                if (m_clockSelect[i].load() != m_snapshotClockSelect[i]
                    || m_resetSelect[i].load() != m_snapshotResetSelect[i])
                {
                    return true;
                }
            }

            for (size_t i = 0; i < x_numVoices; ++i)
            {
                if (m_arpUIState[i].Changed())
                {
                    return true;
                }
            }

            return false;
        }

        void Snapshot()
        {
            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_snapshotClockSelect[i] = m_clockSelect[i].load();
                m_snapshotResetSelect[i] = m_resetSelect[i].load();
            }

            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_arpUIState[i].Snapshot();
            }
        }

        int GetClockSelect(size_t voiceIndex) const
        {
            return m_snapshotClockSelect[voiceIndex / x_voicesPerTrio];
        }

        int GetResetSelect(size_t voiceIndex) const
        {
            return m_snapshotResetSelect[voiceIndex / x_voicesPerTrio];
        }

        float GetChoiceValue(size_t voiceIndex, int64_t clockPosition, int64_t resetCycleCount) const
        {
            if (GetClockSelect(voiceIndex) < 0)
            {
                return m_arpUIState[voiceIndex].GetResetChoiceValue();
            }

            return m_arpUIState[voiceIndex].GetChoiceValue(clockPosition, resetCycleCount);
        }
    };

    void Process(Input& input)
    {
        input.SetClocks();
        input.SetTrioInputs();
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_arp[i].Process(input.m_input[i]);
        }
    }

    void PopulateUIState(UIState* uiState, const Input& input)
    {
        for (size_t i = 0; i < x_numTrios; ++i)
        {
            uiState->m_clockSelect[i].store(input.m_clockSelect[i]);
            uiState->m_resetSelect[i].store(input.m_resetSelect[i]);
        }

        Input inputCopy = input;
        inputCopy.SetTrioInputs();

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_arp[i].PopulateUIState(&uiState->m_arpUIState[i], inputCopy.m_input[i]);
        }
    }
};
