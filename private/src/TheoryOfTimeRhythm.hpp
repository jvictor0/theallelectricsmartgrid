#pragma once

#include <cstddef>
#include <cstdint>

#include "PhaseUtils.hpp"

struct TheoryOfTimeRhythm
{
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

    int MonodromyIndexToIndex(int64_t monodromyIndex) const
    {
        return static_cast<int>(PhaseUtils::FloorMod(monodromyIndex, m_size));
    }

    bool Gate(int64_t monodromyIndex) const
    {
        int index = MonodromyIndexToIndex(monodromyIndex);
        return m_gate[index];
    }

    void ToggleGate(int index)
    {
        m_gate[index] = !m_gate[index];
    }
};
