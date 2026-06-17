#pragma once

#include <cstddef>

enum class ThreadId : size_t
{
    Unknown = 0,
    Message,
    Audio,
    MidiInput,
    MidiSender,
    MidiBusInput,
    MidiBusSender,
    AsyncIo,
    FileWriter,
    SampleLoader,
    Logger,
    Count
};

static constexpr size_t x_threadIdCount = static_cast<size_t>(ThreadId::Count);

inline thread_local ThreadId thread_threadId = ThreadId::Unknown;

inline size_t ThreadIdToIndex(ThreadId threadId)
{
    size_t index = static_cast<size_t>(threadId);
    if (index >= x_threadIdCount)
    {
        return static_cast<size_t>(ThreadId::Unknown);
    }

    return index;
}

inline const char* ThreadIdToString(ThreadId threadId)
{
    switch (threadId)
    {
        case ThreadId::Unknown: return "Unknown";
        case ThreadId::Message: return "Message";
        case ThreadId::Audio: return "Audio";
        case ThreadId::MidiInput: return "MidiInput";
        case ThreadId::MidiSender: return "MidiSender";
        case ThreadId::MidiBusInput: return "MidiBusInput";
        case ThreadId::MidiBusSender: return "MidiBusSender";
        case ThreadId::AsyncIo: return "AsyncIo";
        case ThreadId::FileWriter: return "FileWriter";
        case ThreadId::SampleLoader: return "SampleLoader";
        case ThreadId::Logger: return "Logger";
        case ThreadId::Count: return "Count";
    }

    return "Unknown";
}

inline ThreadId GetCurrentThreadId()
{
    return thread_threadId;
}

inline void SetCurrentThreadId(ThreadId threadId)
{
    thread_threadId = threadId;
}

struct ScopedThreadId
{
    ScopedThreadId(ThreadId threadId)
    {
        SetCurrentThreadId(threadId);
    }

    ~ScopedThreadId()
    {
        SetCurrentThreadId(ThreadId::Unknown);
    }
};
