#pragma once

#include <cstdio>
#include <cstdarg>
#include <atomic>
#include <JuceHeader.h>
#include "CircularQueue.hpp"

struct LogMessage
{
    static constexpr size_t x_maxMessageLength = 256;
    char m_message[x_maxMessageLength];
    size_t m_length;

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
    }
};

struct AsyncLogQueue
{
    CircularQueue<LogMessage, 1024> m_queue;
    std::atomic<size_t> m_missed;

    template<typename... Args>
    void Log(const char* format, Args... args)
    {
        LogMessage* message = m_queue.NextToPush();
        if (message == nullptr)
        {
            m_missed.fetch_add(1);
            return;
        }

        message->Fill(format, args...);
        m_queue.CompletePush();
    }

    void DoLog()
    {
        while (true)
        {
            LogMessage* message = m_queue.PopPtr();
            if (message == nullptr)
            {
                break;
            }

            juce::Logger::writeToLog(message->m_message);
        }

        size_t missed = m_missed.exchange(0);
        if (missed > 0)
        {
            juce::Logger::writeToLog(juce::String::formatted("Missed %zu messages", missed));
        }
    }

    static AsyncLogQueue s_instance;
};
  
inline AsyncLogQueue AsyncLogQueue::s_instance;

#define INFO(...) AsyncLogQueue::s_instance.Log(__VA_ARGS__)
//#define INFO(...) juce::Logger::writeToLog(juce::String::formatted(__VA_ARGS__))