#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>

#ifndef EMBEDDED_BUILD
#include <JuceHeader.h>
#endif

#include "CircularQueue.hpp"
#include "SampleTimer.hpp"
#include "ThreadId.hpp"

struct LogMessage
{
    static constexpr size_t x_maxMessageLength = 256;
    char m_message[x_maxMessageLength];
    size_t m_length;
    size_t m_sample;
    ThreadId m_threadId;

    LogMessage()
        : m_message{}
        , m_length(0)
        , m_sample(0)
        , m_threadId(ThreadId::Unknown)
    {
    }

    void Clear()
    {
        m_length = 0;
        m_message[0] = '\0';
    }

    template<typename... Args>
    void Fill(ThreadId threadId, const char* format, Args... args)
    {
        Clear();
        m_length = snprintf(m_message, x_maxMessageLength, format, args...);
        if (m_length >= x_maxMessageLength)
        {
            m_length = x_maxMessageLength - 1;
            m_message[x_maxMessageLength - 1] = '\0';
        }

        m_sample = SampleTimer::GetSample();
        m_threadId = threadId;
    }

    void Fill(ThreadId threadId, const char* message)
    {
        Clear();
        m_length = snprintf(m_message, x_maxMessageLength, "%s", message);
        if (m_length >= x_maxMessageLength)
        {
            m_length = x_maxMessageLength - 1;
            m_message[x_maxMessageLength - 1] = '\0';
        }

        m_sample = SampleTimer::GetSample();
        m_threadId = threadId;
    }
};

struct AsyncLogQueue
{
    static constexpr size_t x_queueSize = 16384;

    std::array<CircularQueue<LogMessage, x_queueSize>, x_threadIdCount> m_queues;
    std::array<std::atomic<size_t>, x_threadIdCount> m_missed;
    size_t m_nextDrainIndex;

#ifdef EMBEDDED_BUILD
    CircularQueue<LogMessage, 1024> m_testOutputMessages;
    CircularQueue<ThreadId, 1024> m_testOutputThreadIds;
#endif

    AsyncLogQueue()
        : m_queues()
        , m_missed()
        , m_nextDrainIndex(0)
    {
        for (size_t i = 0; i < x_threadIdCount; ++i)
        {
            m_missed[i].store(0);
        }
    }

    template<typename... Args>
    void Log(const char* format, Args... args)
    {
        ThreadId threadId = GetCurrentThreadId();
        size_t queueIndex = ThreadIdToIndex(threadId);
        LogMessage* message = m_queues[queueIndex].NextToPush();
        if (message == nullptr)
        {
            m_missed[queueIndex].fetch_add(1);
            return;
        }

        message->Fill(threadId, format, args...);
        m_queues[queueIndex].CompletePush();
    }

    void DoLog()
    {
        size_t emptyQueueCount = 0;

        while (emptyQueueCount < x_threadIdCount)
        {
            size_t queueIndex = m_nextDrainIndex;
            m_nextDrainIndex = (m_nextDrainIndex + 1) % x_threadIdCount;
            LogMessage* message = m_queues[queueIndex].PeekPtr();
            if (message == nullptr)
            {
                ++emptyQueueCount;
                continue;
            }

            WriteLogMessage(static_cast<ThreadId>(queueIndex), message);
            m_queues[queueIndex].Pop();
            emptyQueueCount = 0;
        }

        for (size_t i = 0; i < x_threadIdCount; ++i)
        {
            size_t missed = m_missed[i].exchange(0);
            if (missed > 0)
            {
                WriteMissedMessage(static_cast<ThreadId>(i), missed);
            }
        }
    }

    void WriteLogMessage(ThreadId threadId, LogMessage* message)
    {
#ifndef EMBEDDED_BUILD
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#if defined(_WIN32)
        localtime_s(&localTm, &t);
#else
        localtime_r(&t, &localTm);
#endif
        juce::String timeStamp = juce::String::formatted("%02d:%02d:%02d %lu %s ",
            localTm.tm_hour,
            localTm.tm_min,
            localTm.tm_sec,
            message->m_sample,
            ThreadIdToString(threadId));
        juce::Logger::writeToLog(timeStamp + juce::String(message->m_message));
#else
        m_testOutputThreadIds.Push(threadId);
        m_testOutputMessages.Push(*message);
#endif
    }

    void WriteMissedMessage(ThreadId threadId, size_t missed)
    {
#ifndef EMBEDDED_BUILD
        juce::Logger::writeToLog(juce::String::formatted(
            "Missed %zu messages on %s",
            missed,
            ThreadIdToString(threadId)));
#else
        LogMessage message;
        message.Fill(
            ThreadId::Logger,
            "Missed %zu messages on %s",
            missed,
            ThreadIdToString(threadId));
        m_testOutputThreadIds.Push(ThreadId::Logger);
        m_testOutputMessages.Push(message);
#endif
    }

#ifdef EMBEDDED_BUILD
    void ResetForTesting()
    {
        LogMessage message;
        ThreadId threadId;

        for (size_t i = 0; i < x_threadIdCount; ++i)
        {
            while (m_queues[i].Pop(message))
            {
            }

            m_missed[i].store(0);
        }

        while (m_testOutputMessages.Pop(message))
        {
        }

        while (m_testOutputThreadIds.Pop(threadId))
        {
        }

        m_nextDrainIndex = 0;
        SetCurrentThreadId(ThreadId::Unknown);
    }

    size_t QueueSizeForTesting(ThreadId threadId) const
    {
        return m_queues[ThreadIdToIndex(threadId)].Size();
    }

    size_t MissedCountForTesting(ThreadId threadId) const
    {
        return m_missed[ThreadIdToIndex(threadId)].load();
    }

    bool PopWrittenForTesting(ThreadId& threadId, LogMessage& message)
    {
        if (!m_testOutputThreadIds.Pop(threadId))
        {
            return false;
        }

        return m_testOutputMessages.Pop(message);
    }
#endif

    static AsyncLogQueue s_instance;
};

inline AsyncLogQueue AsyncLogQueue::s_instance;

#define INFO(...) AsyncLogQueue::s_instance.Log(__VA_ARGS__)
//#define INFO(...) juce::Logger::writeToLog(juce::String::formatted(__VA_ARGS__))
