#pragma once

#include "TheoryOfTimeBase.hpp"
#include "HarmonicSheaf.hpp"
#include <atomic>

struct TheoryOfTimeBaseUIState
{
    struct TimePoint
    {
        int64_t m_globalTickPosition;
        HarmonicSheaf::BitVector m_timeSlice;
        int64_t m_loopCyclePosition;
        int64_t m_resetCycleCount;

        TimePoint()
        {
            m_globalTickPosition = 0;
            m_timeSlice = HarmonicSheaf::BitVector(0);
            m_loopCyclePosition = 0;
            m_resetCycleCount = 0;
        }

        TimePoint(
            int64_t globalTickPosition,
            const HarmonicSheaf::BitVector& timeSlice,
            int64_t loopCyclePosition,
            int64_t resetCycleCount)
        {
            m_globalTickPosition = globalTickPosition;
            m_timeSlice = timeSlice;
            m_loopCyclePosition = loopCyclePosition;
            m_resetCycleCount = resetCycleCount;
        }
    };

    std::atomic<int64_t> m_periodTicks[TheoryOfTimeBase::x_numLoops];
    TheoryOfTimeRhythm::UIState m_rhythm[TheoryOfTimeBase::x_numLoops];

    int64_t m_snapshotPeriodTicks[TheoryOfTimeBase::x_numLoops];

    TheoryOfTimeBaseUIState()
        : m_periodTicks{}
        , m_rhythm{}
        , m_snapshotPeriodTicks{}
    {
        for (size_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            m_periodTicks[i].store(0);
            m_snapshotPeriodTicks[i] = -1;
        }
    }

    bool Changed() const
    {
        for (int64_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            if (m_periodTicks[i].load() != m_snapshotPeriodTicks[i] || m_rhythm[i].Changed())
            {
                return true;
            }
        }

        return false;
    }

    void Snapshot()
    {
        for (int64_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            m_snapshotPeriodTicks[i] = m_periodTicks[i].load();
            m_rhythm[i].Snapshot();
        }
    }

    TimePoint GetTimePoint(
        int64_t globalTickPosition,
        int64_t loopIndex,
        int64_t resetLoopIndex) const
    {
        int64_t loopCyclePosition = 0;
        int64_t resetCycleCount = 0;
        if (loopIndex >= 0)
        {
            int64_t loopPeriodTicks = m_snapshotPeriodTicks[loopIndex];
            int64_t resetPeriodTicks = resetLoopIndex == -1
                ? -1
                : m_snapshotPeriodTicks[resetLoopIndex];
            resetCycleCount = TheoryOfTimeBase::GetResetCycleCount(loopPeriodTicks, resetPeriodTicks);
            loopCyclePosition = TheoryOfTimeBase::GetLoopCyclePosition(
                loopPeriodTicks,
                resetPeriodTicks,
                globalTickPosition);
        }

        HarmonicSheaf::BitVector timeSlice(0);
        for (int64_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            const TheoryOfTimeRhythm& rhythm = m_rhythm[i].m_snapshot;
            int64_t rhythmResetPeriodTicks = rhythm.m_resetLoopIndex == -1
                ? -1
                : m_snapshotPeriodTicks[rhythm.m_resetLoopIndex];
            int64_t rhythmLoopCyclePosition = TheoryOfTimeBase::GetLoopCyclePosition(
                m_snapshotPeriodTicks[i],
                rhythmResetPeriodTicks,
                globalTickPosition);
            if (rhythm.GateAt(rhythmLoopCyclePosition))
            {
                timeSlice.Set(i, true);
            }
        }

        return TimePoint(globalTickPosition, timeSlice, loopCyclePosition, resetCycleCount);
    }

    int64_t GetGlobalPeriodTicks() const
    {
        return m_snapshotPeriodTicks[TheoryOfTimeBase::x_globalLoop];
    }
};

inline void TheoryOfTimeBase::PopulateUIState(TheoryOfTimeBaseUIState& uiState, const TheoryOfTimeBase::Input& input) const
{
    for (int64_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
    {
        uiState.m_periodTicks[i].store(GetPeriodTicks(i, 0));
        input.m_rhythm[i].PopulateUIState(uiState.m_rhythm[i]);
    }
}
