#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "PhaseUtils.hpp"

struct TheoryOfTimeRhythm
{
    struct UIState;

    static constexpr size_t x_maxSize = 16;
    int m_resetLoopIndex;
    int m_size;
    bool m_gate[x_maxSize];

    TheoryOfTimeRhythm()
    {
        m_resetLoopIndex = -1;
        m_size = 2;
        m_gate[0] = true;
        for (size_t i = 1; i < x_maxSize; ++i)
        {
            m_gate[i] = false;
        }
    }

    int GetRhythmSlotIndex(int64_t loopCyclePosition) const
    {
        return static_cast<int>(PhaseUtils::FloorMod(loopCyclePosition, m_size));
    }

    bool GateAt(int64_t loopCyclePosition) const
    {
        int rhythmSlotIndex = GetRhythmSlotIndex(loopCyclePosition);
        return m_gate[rhythmSlotIndex];
    }

    void ToggleGate(int index)
    {
        m_gate[index] = !m_gate[index];
    }

    void PopulateUIState(UIState& uiState) const;
};

struct TheoryOfTimeRhythm::UIState
{
    std::atomic<int64_t> m_resetLoopIndex;
    std::atomic<int64_t> m_size;
    std::atomic<bool> m_gate[x_maxSize];

    TheoryOfTimeRhythm m_snapshot;

    UIState()
    {
        m_resetLoopIndex = -1;
        m_size = 2;
        for (size_t i = 0; i < x_maxSize; ++i)
        {
            m_gate[i] = false;
        }
    }

    void Snapshot()
    {
        m_snapshot.m_resetLoopIndex = m_resetLoopIndex.load();
        m_snapshot.m_size = m_size.load();
        for (size_t i = 0; i < x_maxSize; ++i)
        {
            m_snapshot.m_gate[i] = m_gate[i].load();
        }
    }

    bool Changed() const
    {
        if (m_resetLoopIndex.load() != m_snapshot.m_resetLoopIndex
            || m_size.load() != m_snapshot.m_size)
        {
            return true;
        }

        for (size_t i = 0; i < x_maxSize; ++i)
        {
            if (m_gate[i].load() != m_snapshot.m_gate[i])
            {
                return true;
            }
        }

        return false;
    }

    bool GateAt(int64_t loopCyclePosition) const
    {
        return m_snapshot.GateAt(loopCyclePosition);
    }
};

inline void TheoryOfTimeRhythm::PopulateUIState(TheoryOfTimeRhythm::UIState& uiState) const
{
    uiState.m_resetLoopIndex.store(m_resetLoopIndex);
    uiState.m_size.store(m_size);
    for (size_t i = 0; i < x_maxSize; ++i)
    {
        uiState.m_gate[i].store(m_gate[i]);
    }
}
