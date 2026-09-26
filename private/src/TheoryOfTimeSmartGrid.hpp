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
    int m_rhythmIndex;
    bool* m_shift;

    TheoryOfTimeRhythmCell(
        TheoryOfTime* theoryOfTime,
        State* gateState,
        State* sizeState,
        State* resetState,
        int loopIndex,
        int rhythmIndex,
        bool* shift)
        : m_theoryOfTime(theoryOfTime)
        , m_gateState(gateState)
        , m_sizeState(sizeState)
        , m_resetState(resetState)
        , m_loopIndex(loopIndex)
        , m_rhythmIndex(rhythmIndex)
        , m_shift(shift)
    {
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (m_sizeState->Get<int>() <= m_rhythmIndex)
        {
            return SmartGrid::Color::Off;
        }
        else
        {
            int resetLoopIndex = m_resetState->Get<int>();
            int64_t curIndex = PhaseUtils::FloorMod(m_theoryOfTime->GetGateStepIndex(m_loopIndex, 0, resetLoopIndex), m_sizeState->Get<int>());
            bool curGate = m_gateState->Get<bool>();
            if (curIndex == m_rhythmIndex)
            {
                return curGate ? SmartGrid::Color::Purple : SmartGrid::Color::Pink;
            }
            else
            {
                return curGate ? SmartGrid::Color::Purple.Dim() : SmartGrid::Color::Grey;
            }
        }
    }

    virtual void OnPress(uint8_t) override
    {
        if (*m_shift)
        {
            m_sizeState->Set(m_rhythmIndex + 1);
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
    int m_rhythmIndex;

    TheoryOfTimeRhythmResetCell(
        TheoryOfTime* theoryOfTime,
        State* resetState,
        int loopIndex,
        int rhythmIndex)
        : m_theoryOfTime(theoryOfTime)
        , m_resetState(resetState)
        , m_loopIndex(loopIndex)
        , m_rhythmIndex(rhythmIndex)
    {
    }

    bool IsEnabled()
    {
        return m_loopIndex != m_rhythmIndex && m_theoryOfTime->IsAncestorOf(m_loopIndex, 0, m_rhythmIndex);
    }

    virtual void OnPress(uint8_t) override
    {
        if (IsEnabled())
        {
            if (m_resetState->Get<int>() == m_rhythmIndex)
            {
                m_resetState->Set(-1);
            }
            else
            {
                m_resetState->Set(m_rhythmIndex);
            }
        }
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (!IsEnabled())
        {
            return SmartGrid::Color::Off;
        }
        else if (m_resetState->Get<int>() == m_rhythmIndex)
        {
            return SmartGrid::Color::Blue;
        }
        else
        {
            return SmartGrid::Color::Blue.Dim();
        }
    }
};
