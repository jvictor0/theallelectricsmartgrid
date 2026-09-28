#pragma once

#include "SmartGrid.hpp"
#include "TheoryOfTime.hpp"

struct TheoryOfTimeRhythmCell : public SmartGrid::Cell
{
    TheoryOfTime* m_theoryOfTime;
    State* m_gateState;
    State* m_sizeState;
    State* m_resetState;
    int m_loopIndex;
    int m_rhythmSlotIndex;
    bool* m_shift;

    TheoryOfTimeRhythmCell(
        TheoryOfTime* theoryOfTime,
        State* gateState,
        State* sizeState,
        State* resetState,
        int loopIndex,
        int rhythmSlotIndex,
        bool* shift)
        : m_theoryOfTime(theoryOfTime)
        , m_gateState(gateState)
        , m_sizeState(sizeState)
        , m_resetState(resetState)
        , m_loopIndex(loopIndex)
        , m_rhythmSlotIndex(rhythmSlotIndex)
        , m_shift(shift)
    {
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (m_sizeState->Get<int>() <= m_rhythmSlotIndex)
        {
            return SmartGrid::Color::Off;
        }
        else
        {
            int resetLoopIndex = m_resetState->Get<int>();
            int64_t currentRhythmSlotIndex = PhaseUtils::FloorMod(
                m_theoryOfTime->GetLoopCyclePosition(m_loopIndex, 0, resetLoopIndex),
                m_sizeState->Get<int>());
            bool currentGate = m_gateState->Get<bool>();
            if (currentRhythmSlotIndex == m_rhythmSlotIndex)
            {
                return currentGate ? SmartGrid::Color::Purple : SmartGrid::Color::Pink;
            }
            else
            {
                return currentGate ? SmartGrid::Color::Purple.Dim() : SmartGrid::Color::Grey;
            }
        }
    }

    virtual void OnPress(uint8_t) override
    {
        if (*m_shift)
        {
            m_sizeState->Set(m_rhythmSlotIndex + 1);
        }
        else
        {
            m_gateState->Set(!m_gateState->Get<bool>());
        }
    }
};

struct TheoryOfTimeRhythmResetCell : public SmartGrid::Cell
{
    TheoryOfTime* m_theoryOfTime;
    State* m_resetState;
    int m_loopIndex;
    int m_candidateLoopIndex;

    TheoryOfTimeRhythmResetCell(
        TheoryOfTime* theoryOfTime,
        State* resetState,
        int loopIndex,
        int candidateLoopIndex)
        : m_theoryOfTime(theoryOfTime)
        , m_resetState(resetState)
        , m_loopIndex(loopIndex)
        , m_candidateLoopIndex(candidateLoopIndex)
    {
    }

    bool IsEnabled()
    {
        return m_loopIndex != m_candidateLoopIndex
            && m_theoryOfTime->GetResetCycleCount(m_loopIndex, 0, m_candidateLoopIndex) > 0;
    }

    virtual void OnPress(uint8_t) override
    {
        if (IsEnabled())
        {
            if (m_resetState->Get<int>() == m_candidateLoopIndex)
            {
                m_resetState->Set(-1);
            }
            else
            {
                m_resetState->Set(m_candidateLoopIndex);
            }
        }
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (!IsEnabled())
        {
            return SmartGrid::Color::Off;
        }
        else if (m_resetState->Get<int>() == m_candidateLoopIndex)
        {
            return SmartGrid::Color::Blue;
        }
        else
        {
            return SmartGrid::Color::Blue.Dim();
        }
    }
};
