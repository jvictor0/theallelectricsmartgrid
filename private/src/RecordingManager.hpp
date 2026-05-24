#pragma once

#include <cstddef>

#include "AudioBuffer.hpp"
#include "IOTaskThread.hpp"
#include "RecordingBuffer.hpp"
#include "SquiggleBoyVoiceConfig.hpp"
#include "TheNonagon.hpp"

struct RecordingManager
{
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_numTrios = 3;
    static constexpr size_t x_voicesPerTrio = 3;

    using VoiceRecordingBufferWriter = RecordingBufferWriter<x_numVoices>;

    RecordingBuffer* m_voiceRecordingBuffers[x_numVoices];
    VoiceRecordingBufferWriter* m_voiceBufferWriters[x_numVoices];
    SquiggleBoyVoiceConfig* m_voiceConfig[x_numVoices];
    IoTaskThread* m_ioTaskThread;
    int m_numRecording;

    RecordingManager()
        : m_voiceRecordingBuffers{}
        , m_voiceBufferWriters{}
        , m_voiceConfig{}
        , m_ioTaskThread(nullptr)
        , m_numRecording(0)
    {
    }

    bool AnyRecording() const
    {
        return m_numRecording > 0;
    }

    VoiceRecordingBufferWriter* GetRecordingBufferForVoice(int voiceID)
    {
        RecordingConfig& cfg = m_voiceConfig[static_cast<size_t>(voiceID)]->m_recordingConfig;

        switch (cfg.m_source)
        {
            case RecordingConfig::Source::None:
            {
                return nullptr;
            }

            case RecordingConfig::Source::Water:
            case RecordingConfig::Source::Earth:
            case RecordingConfig::Source::Fire:
            {
                size_t trioIndex = 0;
                cfg.GetTrioIndex(trioIndex);
                size_t voiceOffset = static_cast<size_t>(voiceID) % x_voicesPerTrio;
                size_t bufferIndex = trioIndex * x_voicesPerTrio + voiceOffset;
                return m_voiceBufferWriters[bufferIndex];
            }

            default:
            {
                return nullptr;
            }
        }
    }

    bool PersistRecordingForVoice(int voiceID)
    {
        SquiggleBoyVoiceConfig* voiceConfig = m_voiceConfig[static_cast<size_t>(voiceID)];

        AudioBufferBank* bank = voiceConfig->m_audioBufferBank;
        bool createDirectory = !bank || bank->m_directoryName.empty();
        const char* relativePath = "";

        if (!createDirectory)
        {
            relativePath = bank->m_directoryName.c_str();
        }

        return m_ioTaskThread->PushPersistRecording(
            relativePath,
            m_voiceRecordingBuffers[static_cast<size_t>(voiceID)],
            createDirectory,
            &voiceConfig->m_audioBufferBank,
            voiceID);
    }

    void StartRecording(int voiceID)
    {
        if (voiceID < 0 || static_cast<size_t>(voiceID) >= x_numVoices)
        {
            return;
        }

        VoiceRecordingBufferWriter* writer = GetRecordingBufferForVoice(voiceID);

        if (!writer)
        {
            return;
        }

        RecordingBuffer* buf = m_voiceRecordingBuffers[static_cast<size_t>(voiceID)];

        if (buf->m_state != RecordingBuffer::State::Idle)
        {
            return;
        }

        buf->StartRecording();

        if (buf->m_state == RecordingBuffer::State::Recording)
        {
            ++m_numRecording;
            writer->Register(buf);
        }
    }

    void StartRecording(TheNonagonInternal::Trio trio)
    {
        size_t trioIndex = static_cast<size_t>(trio);
        if (trioIndex >= x_numTrios)
        {
            return;
        }

        size_t baseVoiceID = trioIndex * x_voicesPerTrio;
        for (size_t i = 0; i < x_voicesPerTrio; ++i)
        {
            StartRecording(static_cast<int>(baseVoiceID + i));
        }
    }

    void StopRecordingVoice(int voiceID)
    {
        if (voiceID < 0 || static_cast<size_t>(voiceID) >= x_numVoices)
        {
            return;
        }

        RecordingBuffer* buf = m_voiceRecordingBuffers[static_cast<size_t>(voiceID)];
        bool wasActive =
            buf->m_state == RecordingBuffer::State::Recording
            || buf->m_state == RecordingBuffer::State::Error;

        buf->StopRecording();

        VoiceRecordingBufferWriter* writer = GetRecordingBufferForVoice(voiceID);

        if (writer)
        {
            writer->Deregister(buf);
        }

        if (!wasActive)
        {
            return;
        }

        --m_numRecording;

        if (buf->m_state == RecordingBuffer::State::Done)
        {
            if (!PersistRecordingForVoice(voiceID))
            {
                buf->Reset();
            }
        }
        else if (buf->m_state == RecordingBuffer::State::Error)
        {
            buf->Reset();
        }
    }

    void StopRecording(int voiceID)
    {
        StopRecordingVoice(voiceID);
        PostStopRecording();
    }

    void StopRecording(TheNonagonInternal::Trio trio)
    {
        size_t trioIndex = static_cast<size_t>(trio);
        if (trioIndex >= x_numTrios)
        {
            return;
        }

        size_t baseVoiceID = trioIndex * x_voicesPerTrio;
        for (size_t i = 0; i < x_voicesPerTrio; ++i)
        {
            StopRecordingVoice(static_cast<int>(baseVoiceID + i));
        }

        PostStopRecording();
    }

    void StopRecording()
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            StopRecordingVoice(static_cast<int>(i));
        }

        PostStopRecording();
    }

    void PostStopRecording()
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            SquiggleBoyVoiceConfig* voiceConfig = m_voiceConfig[i];

            AudioBufferBank* bank = voiceConfig->m_audioBufferBank;

            if (!bank || bank->m_directoryName.empty())
            {
                continue;
            }

            const char* rel = bank->m_directoryName.c_str();

            m_ioTaskThread->PushReloadDirectory(
                rel,
                &voiceConfig->m_audioBufferBank);
        }
    }
};
