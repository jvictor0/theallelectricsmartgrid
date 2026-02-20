#pragma once

#include <cstdio>
#include <cstdarg>
#include <atomic>
#ifndef EMBEDDED_BUILD
#include <JuceHeader.h>
#endif
#include "CircularQueue.hpp"
#include "SampleTimer.hpp"

struct LogMessage
{
    static constexpr size_t x_maxMessageLength = 256;
    char m_message[x_maxMessageLength];
    size_t m_length;
    size_t m_sample;

    LogMessage()
        : m_length(0)
    {
    }

    void Clear()
    {
        m_length = 0;
        m_message[0] = '\0';
    }

    template<typename... Args>
    void Fill(const char* format, Args... args)
    {
        Clear();
        m_length = snprintf(m_message, x_maxMessageLength, format, args...);
        if (m_length >= x_maxMessageLength)
        {
            m_length = x_maxMessageLength - 1;
            m_message[x_maxMessageLength - 1] = '\0';
        }

        m_sample = SampleTimer::GetSample();
    }

    void Fill(const char* message)
    {
        Clear();
        m_length = snprintf(m_message, x_maxMessageLength, "%s", message);
        if (m_length >= x_maxMessageLength)
        {
            m_length = x_maxMessageLength - 1;
            m_message[x_maxMessageLength - 1] = '\0';
        }

        m_sample = SampleTimer::GetSample();
    }
};

struct AsyncLogQueue
{
#ifndef EMBEDDED_BUILD
    CircularQueue<LogMessage, 16384> m_queue;
    std::atomic<size_t> m_missed;
#endif

    template<typename... Args>
    void Log(const char* format, Args... args)
    {
#ifndef EMBEDDED_BUILD
        LogMessage* message = m_queue.NextToPush();
        if (message == nullptr)
        {
            m_missed.fetch_add(1);
            return;
        }

        message->Fill(format, args...);
        m_queue.CompletePush();
#endif
    }

    void DoLog()
    {
#ifndef EMBEDDED_BUILD
        while (true)
        {
            LogMessage* message = m_queue.PeekPtr();
            if (message == nullptr)
            {
                break;
            }

            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm localTm;
#if defined(_WIN32)
            localtime_s(&localTm, &t);
#else
            localtime_r(&t, &localTm);
#endif
            juce::String timeStamp = juce::String::formatted("%02d:%02d:%02d %lu ",
                localTm.tm_hour,
                localTm.tm_min,
                localTm.tm_sec,
                message->m_sample);
            juce::Logger::writeToLog(timeStamp + juce::String(message->m_message));
            m_queue.Pop();
        }

        size_t missed = m_missed.exchange(0);
        if (missed > 0)
        {
            juce::Logger::writeToLog(juce::String::formatted("Missed %zu messages", missed));
        }
#endif
    }

    static AsyncLogQueue s_instance;
};
  
inline AsyncLogQueue AsyncLogQueue::s_instance;

#define INFO(...) AsyncLogQueue::s_instance.Log(__VA_ARGS__)
//#define INFO(...) juce::Logger::writeToLog(juce::String::formatted(__VA_ARGS__))