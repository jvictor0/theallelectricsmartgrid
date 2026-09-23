#pragma once

#include "State.hpp"
#include "SampleTimer.hpp"
#include "StreamingRecorder.hpp"

struct ParamEventLogger
{
    StreamingRecorder* m_recorder;
    bool m_suspended = false;

    ParamEventLogger(StreamingRecorder* recorder)
        : m_recorder(recorder)
    {
    }

    void RecordPatch(PatchArena& arena, bool restoreFaders, bool snapshot = false)
    {
        if (m_recorder->IsRecording())
        {
            const size_t sample = SampleTimer::GetSample() - m_recorder->m_recordingStartSample;
            m_recorder->RecordParamEvent(ParamEvent::MkPatch(arena, restoreFaders, snapshot, sample));
        }
    }

    void RecordStateChange(State* state, int scene)
    {
        if (!m_suspended && m_recorder->GetState() == StreamingRecorder::State::Recording)
        {
            size_t sample = SampleTimer::GetSample() - m_recorder->m_recordingStartSample;
            m_recorder->RecordParamEvent(ParamEvent::MkStateChange(state, scene, sample));
        }
    }

    void RecordFaderChange(size_t faderIndex, float value)
    {
        if (!m_suspended && m_recorder->GetState() == StreamingRecorder::State::Recording)
        {
            size_t sample = SampleTimer::GetSample() - m_recorder->m_recordingStartSample;
            m_recorder->RecordParamEvent(ParamEvent::MkGestureSet(static_cast<int>(faderIndex), value, sample));
        }
    }

    void RecordBlendChange(float value)
    {
        if (!m_suspended && m_recorder->GetState() == StreamingRecorder::State::Recording)
        {
            size_t sample = SampleTimer::GetSample() - m_recorder->m_recordingStartSample;
            m_recorder->RecordParamEvent(ParamEvent::MkBlendSet(value, sample));
        }
    }

    void RecordEncoderSet(SmartGrid::StateEncoderCell* stateEncoderCell, int scene, int track)
    {
        if (!m_suspended && m_recorder->GetState() == StreamingRecorder::State::Recording)
        {
            size_t sample = SampleTimer::GetSample() - m_recorder->m_recordingStartSample;
            m_recorder->RecordParamEvent(ParamEvent::MkEncoderSet(stateEncoderCell, scene, track, sample));
        }
    }

    void RecordEncoderActivate(SmartGrid::BankedEncoderCell* cell, int scene, int track)
    {
        if (!m_suspended && m_recorder->GetState() == StreamingRecorder::State::Recording)
        {
            size_t sample = SampleTimer::GetSample() - m_recorder->m_recordingStartSample;
            m_recorder->RecordParamEvent(ParamEvent::MkEncoderActivate(cell, scene, track, sample));
        }
    }
};

inline void RecordStateChange(ParamEventLogger* paramEventLogger, State* state, int scene)
{
    paramEventLogger->RecordStateChange(state, scene);
}
