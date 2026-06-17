#include "doctest.h"

#include "AsyncLogger.hpp"
#include "ThreadId.hpp"

#include <cstring>
#include <thread>

DOCTEST_TEST_CASE("thread id scope clears callback identity")
{
    SetCurrentThreadId(ThreadId::Message);

    {
        ScopedThreadId scopedThreadId(ThreadId::Audio);
        DOCTEST_CHECK(GetCurrentThreadId() == ThreadId::Audio);
    }

    DOCTEST_CHECK(GetCurrentThreadId() == ThreadId::Unknown);
}

DOCTEST_TEST_CASE("async logger routes different thread ids to separate queues")
{
    AsyncLogQueue::s_instance.ResetForTesting();

    std::thread audioThread([]()
    {
        SetCurrentThreadId(ThreadId::Audio);
        INFO("audio message");
    });

    std::thread midiThread([]()
    {
        SetCurrentThreadId(ThreadId::MidiSender);
        INFO("midi message");
    });

    audioThread.join();
    midiThread.join();

    DOCTEST_CHECK(AsyncLogQueue::s_instance.QueueSizeForTesting(ThreadId::Audio) == 1);
    DOCTEST_CHECK(AsyncLogQueue::s_instance.QueueSizeForTesting(ThreadId::MidiSender) == 1);
    DOCTEST_CHECK(AsyncLogQueue::s_instance.QueueSizeForTesting(ThreadId::Unknown) == 0);
}

DOCTEST_TEST_CASE("async logger drains queues round robin")
{
    AsyncLogQueue::s_instance.ResetForTesting();

    {
        ScopedThreadId scopedThreadId(ThreadId::Audio);
        INFO("audio 1");
        INFO("audio 2");
    }

    {
        ScopedThreadId scopedThreadId(ThreadId::MidiSender);
        INFO("midi 1");
    }

    AsyncLogQueue::s_instance.DoLog();

    ThreadId threadId;
    LogMessage message;

    DOCTEST_REQUIRE(AsyncLogQueue::s_instance.PopWrittenForTesting(threadId, message));
    DOCTEST_CHECK(threadId == ThreadId::Audio);
    DOCTEST_CHECK(std::strcmp(message.m_message, "audio 1") == 0);

    DOCTEST_REQUIRE(AsyncLogQueue::s_instance.PopWrittenForTesting(threadId, message));
    DOCTEST_CHECK(threadId == ThreadId::MidiSender);
    DOCTEST_CHECK(std::strcmp(message.m_message, "midi 1") == 0);

    DOCTEST_REQUIRE(AsyncLogQueue::s_instance.PopWrittenForTesting(threadId, message));
    DOCTEST_CHECK(threadId == ThreadId::Audio);
    DOCTEST_CHECK(std::strcmp(message.m_message, "audio 2") == 0);

    DOCTEST_CHECK(!AsyncLogQueue::s_instance.PopWrittenForTesting(threadId, message));
}

DOCTEST_TEST_CASE("async logger records missed messages per selected queue")
{
    AsyncLogQueue::s_instance.ResetForTesting();
    SetCurrentThreadId(ThreadId::Audio);

    for (size_t i = 0; i < AsyncLogQueue::x_queueSize; ++i)
    {
        INFO("fill");
    }

    INFO("missed");

    DOCTEST_CHECK(AsyncLogQueue::s_instance.QueueSizeForTesting(ThreadId::Audio) == AsyncLogQueue::x_queueSize);
    DOCTEST_CHECK(AsyncLogQueue::s_instance.MissedCountForTesting(ThreadId::Audio) == 1);
    DOCTEST_CHECK(AsyncLogQueue::s_instance.MissedCountForTesting(ThreadId::MidiSender) == 0);

    AsyncLogQueue::s_instance.ResetForTesting();
}
