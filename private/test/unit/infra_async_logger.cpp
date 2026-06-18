#include "doctest.h"

#include "AsyncLogger.hpp"
#include "ThreadId.hpp"
#include "support/TempDir.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
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
    synthrig::TempDir tempDir;
    DOCTEST_REQUIRE(tempDir.Valid());
    AsyncLogQueue::s_instance.SetLogDirectoryForTesting(tempDir.String().c_str());

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

    std::filesystem::path logPath(AsyncLogQueue::s_instance.LogFilePathForTesting());
    DOCTEST_REQUIRE(std::filesystem::exists(logPath));

    std::ifstream in(logPath);
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string log = buffer.str();

    size_t audio1 = log.find("audio 1");
    size_t midi1 = log.find("midi 1");
    size_t audio2 = log.find("audio 2");
    DOCTEST_REQUIRE(audio1 != std::string::npos);
    DOCTEST_REQUIRE(midi1 != std::string::npos);
    DOCTEST_REQUIRE(audio2 != std::string::npos);
    DOCTEST_CHECK(audio1 < midi1);
    DOCTEST_CHECK(midi1 < audio2);
}

DOCTEST_TEST_CASE("async logger records missed messages per selected queue")
{
    AsyncLogQueue::s_instance.ResetForTesting();
    synthrig::TempDir tempDir;
    DOCTEST_REQUIRE(tempDir.Valid());
    AsyncLogQueue::s_instance.SetLogDirectoryForTesting(tempDir.String().c_str());
    SetCurrentThreadId(ThreadId::Audio);

    for (size_t i = 0; i < AsyncLogQueue::x_queueSize; ++i)
    {
        INFO("fill");
    }

    INFO("missed");

    DOCTEST_CHECK(AsyncLogQueue::s_instance.QueueSizeForTesting(ThreadId::Audio) == AsyncLogQueue::x_queueSize);
    DOCTEST_CHECK(AsyncLogQueue::s_instance.MissedCountForTesting(ThreadId::Audio) == 1);
    DOCTEST_CHECK(AsyncLogQueue::s_instance.MissedCountForTesting(ThreadId::MidiSender) == 0);

    AsyncLogQueue::s_instance.DoLog();

    std::ifstream in(AsyncLogQueue::s_instance.LogFilePathForTesting());
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string log = buffer.str();

    DOCTEST_CHECK(log.find("Missed 1 messages on Audio") != std::string::npos);

    AsyncLogQueue::s_instance.ResetForTesting();
}

DOCTEST_TEST_CASE("async logger creates one timestamped session file in configured log directory")
{
    AsyncLogQueue::s_instance.ResetForTesting();
    synthrig::TempDir tempDir;
    DOCTEST_REQUIRE(tempDir.Valid());

    AsyncLogQueue::s_instance.SetLogDirectoryForTesting(tempDir.String().c_str());
    std::filesystem::path firstPath(AsyncLogQueue::s_instance.LogFilePathForTesting());

    {
        ScopedThreadId scopedThreadId(ThreadId::Audio);
        INFO("first session message");
    }

    AsyncLogQueue::s_instance.DoLog();

    {
        ScopedThreadId scopedThreadId(ThreadId::MidiSender);
        INFO("same session message");
    }

    AsyncLogQueue::s_instance.DoLog();

    std::filesystem::path secondPath(AsyncLogQueue::s_instance.LogFilePathForTesting());
    DOCTEST_CHECK(firstPath == secondPath);
    DOCTEST_CHECK(firstPath.parent_path() == tempDir.Path());
    DOCTEST_CHECK(firstPath.extension() == ".log");
    DOCTEST_REQUIRE(std::filesystem::exists(firstPath));

    size_t logCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(tempDir.Path()))
    {
        if (entry.path().extension() == ".log")
        {
            ++logCount;
        }
    }

    DOCTEST_CHECK(logCount == 1);

    std::ifstream in(firstPath);
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string log = buffer.str();

    DOCTEST_CHECK(log.find("first session message") != std::string::npos);
    DOCTEST_CHECK(log.find("same session message") != std::string::npos);

    AsyncLogQueue::s_instance.ResetForTesting();
}
