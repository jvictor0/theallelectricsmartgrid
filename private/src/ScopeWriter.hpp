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