#pragma once

#include <atomic>

struct ScopeWriter
{
    static constexpr size_t x_bufferSize = 32 * 1024 * 1024;
    size_t m_voices;
    size_t m_scopes;

    size_t m_index;
    std::atomic<size_t> m_publishedIndex;
    float m_buffer[x_bufferSize];

    static constexpr size_t x_maxScopes = 16;
    static constexpr size_t x_maxVoices = 16;
    static constexpr size_t x_numStartIndices = 256;
    size_t m_startIndices[x_maxScopes][x_maxVoices][x_numStartIndices];
    size_t m_endIndices[x_maxScopes][x_maxVoices][x_numStartIndices];

    std::atomic<size_t> m_startIndexIndex[x_maxScopes][x_maxVoices];
    size_t m_advanceStartIndices[x_maxScopes][x_maxVoices];

    ScopeWriter(size_t voices, size_t scopes)
        : m_voices(voices)
        , m_scopes(scopes)
        , m_index(0)
        , m_publishedIndex(0)
    {
        for (size_t i = 0; i < x_maxScopes; ++i)
        {
            for (size_t j = 0; j < x_maxVoices; ++j)
            {
                m_startIndexIndex[i][j] = 0;
                for (size_t k = 0; k < x_numStartIndices; ++k)
                {
                    m_startIndices[i][j][k] = 0;
                    m_endIndices[i][j][k] = 0;
                }
            }
        }
    }

    size_t GetPhysicalIndex(size_t scope, size_t voice, size_t index)
    {
        return (index * m_voices * m_scopes + scope * m_voices + voice) % x_bufferSize;
    }

    float Read(size_t scope, size_t voice, double index)
    {
        size_t index1 = GetPhysicalIndex(scope, voice, static_cast<size_t>(index));
        size_t index2 = GetPhysicalIndex(scope, voice, static_cast<size_t>(index) + 1);
        float value1 = m_buffer[index1];
        float value2 = m_buffer[index2];
        double wayThrough = index - std::floor(index);
        return value1 * (1 - wayThrough) + value2 * wayThrough;
    }

    void Write(size_t scope, size_t voice, float value)
    {
        size_t index = GetPhysicalIndex(scope, voice, m_index);
        m_buffer[index] = value;
    }

    void AdvanceIndex()
    {
        ++m_index;
    }

    void Publish()
    {
        m_publishedIndex.store(m_index);
        for (size_t i = 0; i < m_scopes; ++i)
        {
            for (size_t j = 0; j < m_voices; ++j)
            {
                if (m_advanceStartIndices[i][j] > 0)
                {
                    m_startIndexIndex[i][j].store(m_startIndexIndex[i][j].load() + m_advanceStartIndices[i][j]);
                }

                m_advanceStartIndices[i][j] = 0;
            }
        }
    }

    void RecordStart(size_t scope, size_t voice)
    {
        size_t curStartIndex = m_startIndexIndex[scope][voice].load() + m_advanceStartIndices[scope][voice];
        curStartIndex %= x_numStartIndices;
        m_startIndices[scope][voice][curStartIndex] = m_index;
        m_advanceStartIndices[scope][voice] += 1;
    }

    void RecordEnd(size_t scope, size_t voice)
    {
        size_t curEndIndex = m_startIndexIndex[scope][voice].load() + m_advanceStartIndices[scope][voice] - 1;
        curEndIndex %= x_numStartIndices;
        m_endIndices[scope][voice][curEndIndex] = m_index;
    }
};

struct ScopeWriterHolder
{
    ScopeWriter* m_scopeWriter;
    size_t m_voiceIx;
    size_t m_scopeIx;

    ScopeWriterHolder()
        : m_scopeWriter(nullptr)
        , m_voiceIx(0)
        , m_scopeIx(0)
    {
    }

    ScopeWriterHolder(ScopeWriter* scopeWriter, size_t voiceIx, size_t scopeIx)
        : m_scopeWriter(scopeWriter)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
    {
    }

    void Write(float value)
    {
        if (m_scopeWriter)
        {
            m_scopeWriter->Write(m_scopeIx, m_voiceIx, value);
        }
    }

    void RecordStart()
    {
        if (m_scopeWriter)
        {
            m_scopeWriter->RecordStart(m_scopeIx, m_voiceIx);
        }
    }

    void RecordEnd()
    {
        if (m_scopeWriter)
        {
            m_scopeWriter->RecordEnd(m_scopeIx, m_voiceIx);
        }
    }
};

struct ScopeReader
{
    ScopeWriter* m_scopeWriter;
    size_t m_voiceIx;
    size_t m_scopeIx;
    size_t m_numXSamples;
    
    size_t m_startIndex;
    size_t m_transferXSample;
    size_t m_transferIndex;
    size_t m_postTransferIndex;

    ScopeReader(
        ScopeWriter* scopeWriter, 
        size_t voiceIx, 
        size_t scopeIx,
        size_t numXSamples,
        size_t numCycles)
        : m_scopeWriter(scopeWriter)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
        , m_numXSamples(numXSamples)
    {
        size_t lastStartIndexIndex = m_scopeWriter->m_startIndexIndex[m_scopeIx][m_voiceIx].load();
        m_startIndex = m_scopeWriter->m_startIndices[m_scopeIx][m_voiceIx][(lastStartIndexIndex - 1) % ScopeWriter::x_numStartIndices];
        size_t prevStartIndex = m_scopeWriter->m_startIndices[m_scopeIx][m_voiceIx][(lastStartIndexIndex - 2) % ScopeWriter::x_numStartIndices];
        m_transferIndex = m_scopeWriter->m_publishedIndex.load() - 1;
        size_t endIndex = m_scopeWriter->m_endIndices[m_scopeIx][m_voiceIx][(lastStartIndexIndex - 1) % ScopeWriter::x_numStartIndices];
        if (m_startIndex < endIndex)
        {
            m_transferIndex = endIndex - 1;
        }

        size_t prevNumSamples = m_startIndex - prevStartIndex;
        size_t prevEndIndex = m_scopeWriter->m_endIndices[m_scopeIx][m_voiceIx][(lastStartIndexIndex - 2) % ScopeWriter::x_numStartIndices];
        if (prevStartIndex < prevEndIndex)
        {
            prevNumSamples = prevEndIndex - prevStartIndex;
        }

        if (prevNumSamples < m_transferIndex - m_startIndex)
        {
            // No transfer.
            //
            m_transferXSample = m_numXSamples;
        }
        else
        {
            // Assume number of pre transfer samples is same as previous cycle length.
            //
            double wayThrough = static_cast<double>(m_transferIndex - m_startIndex) / static_cast<double>(prevNumSamples);
            m_transferXSample = wayThrough * m_numXSamples;
            m_postTransferIndex = prevStartIndex + (m_transferIndex - m_startIndex);
        }
    }

    float Get(size_t sample)
    {
        if (sample < m_transferXSample)
        {
            double wayThrough = static_cast<double>(sample) / static_cast<double>(m_transferXSample);
            double index = m_startIndex + wayThrough * (m_transferIndex - m_startIndex);
            float value = m_scopeWriter->Read(m_scopeIx, m_voiceIx, index);
            return value;
        }
        else
        {
            double wayThrough = static_cast<double>(sample - m_transferXSample) / static_cast<double>(m_numXSamples - m_transferXSample);
            double index = m_postTransferIndex + wayThrough * (m_startIndex - m_postTransferIndex);
            float value = m_scopeWriter->Read(m_scopeIx, m_voiceIx, index);
            return value;
        }
    }
};

struct ScopeReaderFactory
{
    ScopeWriter* m_scopeWriter;
    size_t m_voiceIx;
    size_t m_scopeIx;
    size_t m_numXSamples;
    size_t m_numCycles;

    ScopeReaderFactory(
        ScopeWriter* scopeWriter, 
        size_t voiceIx, 
        size_t scopeIx, 
        size_t numXSamples, 
        size_t numCycles)
        : m_scopeWriter(scopeWriter)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
        , m_numXSamples(numXSamples)
        , m_numCycles(numCycles)
    {
    }

    ScopeReader Create()
    {
        return ScopeReader(m_scopeWriter, m_voiceIx, m_scopeIx, m_numXSamples, m_numCycles);
    }
};

template<typename T>
struct PeriodicEventWriter
{
    static constexpr size_t x_numEvents = 16384;
    T m_events[x_numEvents];
    std::atomic<ssize_t> m_index;
    std::atomic<ssize_t> m_lastStartIndex;

    PeriodicEventWriter()
        : m_index(0)
        , m_lastStartIndex(0)
    {
    }

    void RecordStartIndex()
    {
        m_lastStartIndex.store(m_index.load());
    }

    ssize_t RecordEvent(T& value)
    {
        ssize_t index = m_index.load();
        m_events[index % x_numEvents] = value;
        m_index.store(m_index.load() + 1);
        return index;
    }

    T& Get(ssize_t index)
    {
        return m_events[index % x_numEvents];
    }
};

struct NonagonNoteWriter
{
    static constexpr size_t x_numVoices = 9;
    struct EventData
    {
        uint8_t m_voiceIx;
        float m_voltPerOct;
        float m_startPosition;
        float m_endPosition;
        float m_timbre[3];

        EventData()
            : m_voiceIx(0)
            , m_voltPerOct(0)
            , m_startPosition(0)
            , m_endPosition(-1)
            , m_timbre{0, 0, 0}
        {
        }
    };

    static constexpr size_t x_numEvents = PeriodicEventWriter<EventData>::x_numEvents;

    PeriodicEventWriter<EventData> m_events;

    ssize_t m_lastNoteIndex[x_numVoices];
    std::atomic<float> m_minPitch;
    std::atomic<float> m_maxPitch;
    std::atomic<float> m_lastPeriodMinPitch;
    std::atomic<float> m_lastPeriodMaxPitch;
    std::atomic<float> m_curPosition;

    NonagonNoteWriter()
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_lastNoteIndex[i] = -1;
        }
    }

    void RecordNote(EventData& eventData)
    {
        bool isFirstEvent = m_events.m_lastStartIndex.load() == m_events.m_index.load();
        if (m_lastNoteIndex[eventData.m_voiceIx] != -1)
        {
            RecordNoteEnd(eventData.m_voiceIx, eventData.m_startPosition);
        }

        m_lastNoteIndex[eventData.m_voiceIx] = m_events.RecordEvent(eventData);

        if (isFirstEvent)
        {
            m_minPitch.store(eventData.m_voltPerOct);
            m_maxPitch.store(eventData.m_voltPerOct);
        }
        else
        {
            m_minPitch.store(std::min(m_minPitch.load(), eventData.m_voltPerOct));
            m_maxPitch.store(std::max(m_maxPitch.load(), eventData.m_voltPerOct));
        }
    }

    void RecordNoteEnd(size_t voiceIx, float startPosition)
    {
        ssize_t currentIndex = m_events.m_index.load();
        if (m_lastNoteIndex[voiceIx] != -1 && currentIndex - m_lastNoteIndex[voiceIx] < x_numEvents)
        {
            assert(m_events.m_events[m_lastNoteIndex[voiceIx] % x_numEvents].m_voiceIx == voiceIx);
            m_events.m_events[m_lastNoteIndex[voiceIx] % x_numEvents].m_endPosition = startPosition;
        }

        m_lastNoteIndex[voiceIx] = -1;
    }

    void RecordStartIndex()
    {
        m_lastPeriodMinPitch.store(m_minPitch.load());
        m_lastPeriodMaxPitch.store(m_maxPitch.load());

        m_events.RecordStartIndex();

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            if (m_lastNoteIndex[i] != -1 && m_events.m_index.load() - m_lastNoteIndex[i] < x_numEvents)
            {
                EventData eventData = m_events.m_events[m_lastNoteIndex[i] % x_numEvents];
                RecordNoteEnd(i, 1.0);
                eventData.m_startPosition = 0;
                RecordNote(eventData);
            }
        }
    }

    void SetCurPosition(float position)
    {
        m_curPosition.store(position);
    }

    float GetCurPosition()
    {
        return m_curPosition.load();
    }

    EventData& Get(ssize_t index)
    {
        return m_events.Get(index);
    }

    ssize_t GetIndex()
    {
        return m_events.m_index.load();
    }

    ssize_t GetLastStartIndex()
    {
        return m_events.m_lastStartIndex.load();
    }
    
    float GetMinPitch()
    {
        return std::min(m_minPitch.load(), m_lastPeriodMinPitch.load());
    }

    float GetMaxPitch()
    {
        return std::max(m_maxPitch.load(), m_lastPeriodMaxPitch.load());
    }
};