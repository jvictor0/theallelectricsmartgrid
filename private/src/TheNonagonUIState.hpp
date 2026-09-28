#pragma once

#include "TheoryOfTime.hpp"
#include "LameJuis.hpp"
#include "IndexArp.hpp"
#include "ThreadId.hpp"
#include <atomic>
#include <deque>

struct TheNonagonUIState
{
    static constexpr size_t x_numVoices = NonagonIndexArp::x_numVoices;

    struct VoicePoint : public TheoryOfTimeBaseUIState::TimePoint
    {
        float m_choiceValue;
        HarmonicSheaf::SectionWithValue m_pitch;

        VoicePoint()
            : TheoryOfTimeBaseUIState::TimePoint()
            , m_choiceValue(0)
        {
        }

        VoicePoint(
            const TheoryOfTimeBaseUIState::TimePoint& timePoint,
            float choiceValue,
            const HarmonicSheaf::SectionWithValue& pitch)
            : TheoryOfTimeBaseUIState::TimePoint(timePoint)
            , m_choiceValue(choiceValue)
            , m_pitch(pitch)
        {
        }
    };

    struct Sequence
    {
        static constexpr size_t x_maxSize = 1024;
        static constexpr size_t x_maxExtend = 32;

        std::deque<VoicePoint> m_points;

        Sequence()
            : m_points()
        {
        }

        int64_t StartPosition() const
        {
            return m_points.front().m_globalTickPosition;
        }

        int64_t EndPosition() const
        {
            return m_points.back().m_globalTickPosition + 1;
        }

        void PushFront(const VoicePoint& point)
        {
            m_points.push_front(point);
        }

        void PushBack(const VoicePoint& point)
        {
            m_points.push_back(point);
        }

        void PopFront()
        {
            m_points.pop_front();
        }

        void PopBack()
        {
            m_points.pop_back();
        }

        VoicePoint GetPoint(int64_t globalTickPosition) const
        {
            return m_points[globalTickPosition - StartPosition()];
        }

        bool HasPoint(int64_t globalTickPosition) const
        {
            return globalTickPosition >= StartPosition() && globalTickPosition < EndPosition();
        }

        void Clear()
        {
            m_points.clear();
        }

        static std::pair<int64_t, int64_t> GetDesiredPositionRange(
            int64_t globalTickPosition,
            int64_t globalPeriodTicks)
        {
            int64_t globalCycleStart = globalTickPosition - PhaseUtils::FloorMod(globalTickPosition, globalPeriodTicks);
            int64_t globalCycleEnd = globalCycleStart + globalPeriodTicks;
            if ((globalCycleEnd - globalCycleStart) * 3 <= x_maxSize)
            {
                return std::make_pair(
                    globalCycleStart - globalPeriodTicks,
                    globalCycleEnd + globalPeriodTicks);
            }
            else if (globalCycleEnd - globalCycleStart <= x_maxSize)
            {
                int64_t buffer = (x_maxSize - (globalCycleEnd - globalCycleStart)) / 2;
                return std::make_pair(globalCycleStart - buffer, globalCycleEnd + buffer);
            }
            else
            {
                return std::make_pair(
                    globalTickPosition - x_maxSize / 2,
                    globalTickPosition + x_maxSize / 2);
            }
        }
    };

    TheoryOfTime::UIState m_theoryOfTimeUIState;
    LameJuisInternal::UIState m_lameJuisUIState;
    NonagonIndexArp::UIState m_indexArpUIState;

    Sequence m_sequences[x_numVoices];

    VoicePoint GetVoicePoint(int64_t globalTickPosition, size_t voiceIndex)
    {
        TheoryOfTimeBaseUIState::TimePoint timePoint = m_theoryOfTimeUIState.GetTimePoint(
            globalTickPosition,
            m_indexArpUIState.GetClockSelect(voiceIndex),
            m_indexArpUIState.GetResetSelect(voiceIndex));
        float choiceValue = m_indexArpUIState.GetChoiceValue(
            voiceIndex, timePoint.m_loopCyclePosition, timePoint.m_resetCycleCount);
        HarmonicSheaf::SectionWithValue pitch = m_lameJuisUIState.m_harmonicSheafState.Choose(
            voiceIndex,
            timePoint.m_timeSlice,
            choiceValue);
        return VoicePoint(timePoint, choiceValue, pitch);
    }

    bool Changed() const
    {
        if (m_theoryOfTimeUIState.Changed() || m_lameJuisUIState.m_harmonicSheafState.Changed())
        {
            return true;
        }

        return m_indexArpUIState.Changed();
    }

    void Snapshot()
    {
        m_theoryOfTimeUIState.Snapshot();
        m_lameJuisUIState.m_harmonicSheafState.Snapshot();
        m_indexArpUIState.Snapshot();
    }

    void PreProcess(int64_t globalTickPosition)
    {
        if (Changed())
        {
            Snapshot();
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_sequences[i].Clear();
                m_sequences[i].PushBack(GetVoicePoint(globalTickPosition, i));
            }
        }

        std::pair<int64_t, int64_t> desiredPositionRange = Sequence::GetDesiredPositionRange(
            globalTickPosition,
            m_theoryOfTimeUIState.GetGlobalPeriodTicks());
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (m_sequences[i].EndPosition() <= desiredPositionRange.first
                || desiredPositionRange.second <= m_sequences[i].StartPosition())
            {
                m_sequences[i].Clear();
                m_sequences[i].PushBack(GetVoicePoint(globalTickPosition, i));
            }
        }
    }

    void Process(int64_t globalTickPosition, size_t voiceIndex)
    {
        assert(GetCurrentThreadId() != ThreadId::Audio);
        std::pair<int64_t, int64_t> desiredPositionRange = Sequence::GetDesiredPositionRange(
            globalTickPosition,
            m_theoryOfTimeUIState.GetGlobalPeriodTicks());
        while (m_sequences[voiceIndex].StartPosition() < desiredPositionRange.first)
        {
            m_sequences[voiceIndex].PopFront();
        }

        while (desiredPositionRange.second < m_sequences[voiceIndex].EndPosition())
        {
            m_sequences[voiceIndex].PopBack();
        }

        for (int64_t i = 0; i < Sequence::x_maxExtend; ++i)
        {
            if (m_sequences[voiceIndex].StartPosition() <= desiredPositionRange.first)
            {
                break;
            }

            int64_t previousPosition = m_sequences[voiceIndex].StartPosition() - 1;
            m_sequences[voiceIndex].PushFront(GetVoicePoint(previousPosition, voiceIndex));
        }

        for (int64_t i = 0; i < Sequence::x_maxExtend; ++i)
        {
            if (m_sequences[voiceIndex].EndPosition() >= desiredPositionRange.second)
            {
                break;
            }

            int64_t nextPosition = m_sequences[voiceIndex].EndPosition();
            m_sequences[voiceIndex].PushBack(GetVoicePoint(nextPosition, voiceIndex));
        }
    }
};
