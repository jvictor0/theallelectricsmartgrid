#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

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
    std::string m_logDirectory;
    std::string m_logFilePath;
    std::ofstream m_logFile;

    AsyncLogQueue()
        : m_queues()
        , m_missed()
        , m_nextDrainIndex(0)
        , m_logDirectory()
        , m_logFilePath()
        , m_logFile()
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
        bool didSomething = false;

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

            if (WriteLogMessage(static_cast<ThreadId>(queueIndex), message))
            {
                didSomething = true;
            }

            m_queues[queueIndex].Pop();
            emptyQueueCount = 0;
        }

        for (size_t i = 0; i < x_threadIdCount; ++i)
        {
            size_t missed = m_missed[i].exchange(0);
            if (missed > 0)
            {
                if (WriteMissedMessage(static_cast<ThreadId>(i), missed))
                {
                    didSomething = true;
                }
            }
        }

        if (didSomething)
        {
            fflush(stdout);
        }
    }

    bool WriteLogMessage(ThreadId threadId, LogMessage* message)
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#if defined(_WIN32)
        localtime_s(&localTm, &t);
#else
        localtime_r(&t, &localTm);
#endif

        char line[LogMessage::x_maxMessageLength + 96];
        snprintf(
            line,
            sizeof(line),
            "%02d:%02d:%02d %lu %s %s",
            localTm.tm_hour,
            localTm.tm_min,
            localTm.tm_sec,
            message->m_sample,
            ThreadIdToString(threadId),
            message->m_message);
        return WriteLine(line);
    }

    bool WriteMissedMessage(ThreadId threadId, size_t missed)
    {
        char line[LogMessage::x_maxMessageLength];
        snprintf(
            line,
            sizeof(line),
            "Missed %zu messages on %s",
            missed,
            ThreadIdToString(threadId));
        return WriteLine(line);
    }

    void ConfigureLogDirectory(const char* logDirectory)
    {
        if (m_logFile.is_open())
        {
            m_logFile.close();
        }

        m_logDirectory = logDirectory == nullptr ? "" : logDirectory;
        m_logFilePath.clear();

        if (m_logDirectory.empty())
        {
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(m_logDirectory, ec);
        OpenSessionLogFile();
    }

    void OpenSessionLogFile()
    {
        if (m_logDirectory.empty())
        {
            return;
        }

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#if defined(_WIN32)
        localtime_s(&localTm, &t);
#else
        localtime_r(&t, &localTm);
#endif

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;

        char filename[64];
        snprintf(
            filename,
            sizeof(filename),
            "%04d-%02d-%02dT%02d-%02d-%02d-%03lld.log",
            localTm.tm_year + 1900,
            localTm.tm_mon + 1,
            localTm.tm_mday,
            localTm.tm_hour,
            localTm.tm_min,
            localTm.tm_sec,
            static_cast<long long>(millis));

        std::filesystem::path logPath = std::filesystem::path(m_logDirectory) / filename;
        m_logFilePath = logPath.string();
        m_logFile.open(m_logFilePath, std::ios::out | std::ios::app);
    }

    bool WriteLine(const char* line)
    {
        fputs(line, stdout);
        fputc('\n', stdout);

        if (!m_logFile.is_open())
        {
            OpenSessionLogFile();
        }

        if (!m_logFile.is_open())
        {
            return true;
        }

        m_logFile << line << '\n';
        m_logFile.flush();
        return true;
    }

    void ResetForTesting()
    {
        LogMessage message;

        for (size_t i = 0; i < x_threadIdCount; ++i)
        {
            while (m_queues[i].Pop(message))
            {
            }

            m_missed[i].store(0);
        }

        if (m_logFile.is_open())
        {
            m_logFile.close();
        }

        m_logDirectory.clear();
        m_logFilePath.clear();

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

    void SetLogDirectoryForTesting(const char* logDirectory)
    {
        ConfigureLogDirectory(logDirectory);
    }

    const std::string& LogFilePathForTesting() const
    {
        return m_logFilePath;
    }

    static AsyncLogQueue s_instance;
};

inline AsyncLogQueue AsyncLogQueue::s_instance;

#define INFO(...) AsyncLogQueue::s_instance.Log(__VA_ARGS__)
