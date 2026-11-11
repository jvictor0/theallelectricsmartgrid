#include "doctest.h"
#include "AppObserver.hpp"
#include <limits>
#include <string>

DOCTEST_TEST_CASE("audio metrics format only on the reporter thread and bound a backlog of incident logs")
{
    auto& logger = AsyncLogQueue::s_instance;
    logger.ResetForTesting();
    AppObserver observer;
    SetCurrentThreadId(ThreadId::Audio);
    for (size_t i = 0; i < 100; ++i)
    {
        const uint64_t startUs = 1000000 + i * 60000;
        const auto start = observer.m_audioMetrics.BeginAt({startUs, 0, true}, 1024, 48000, i * 4096);
        observer.m_audioMetrics.EndAt(start, {startUs + 30000, 10000000, true});
    }

    observer.FinishAudio();
    DOCTEST_CHECK(logger.QueueSizeForTesting(ThreadId::Audio) == 0);
    SetCurrentThreadId(ThreadId::Message);
    observer.CollectMetrics();
    DOCTEST_CHECK(observer.m_metricsWindow.m_count == 100);
    DOCTEST_CHECK(logger.QueueSizeForTesting(ThreadId::Message) == 2 * AudioCallbackMetrics::x_incidentsPerBatch);
    DOCTEST_CHECK(observer.m_omittedIncidents == 100 - AudioCallbackMetrics::x_incidentsPerBatch);
    DOCTEST_CHECK(observer.m_audioMetrics.m_overruns.load() == 100);
    observer.LogMetrics();
    DOCTEST_CHECK(observer.m_metricsWindow.m_count == 0);
    DOCTEST_CHECK(observer.m_omittedIncidents == 0);
    DOCTEST_CHECK(logger.QueueSizeForTesting(ThreadId::Audio) == 0);
    logger.ResetForTesting();
}

DOCTEST_TEST_CASE("audio metric log records retain event timestamps and all fields without truncation")
{
    auto& logger = AsyncLogQueue::s_instance;
    logger.ResetForTesting();
    SetCurrentThreadId(ThreadId::Message);
    AudioCallbackMetrics::Record record;
    record.m_callback = std::numeric_limits<uint64_t>::max();
    record.m_startUs = record.m_callback;
    record.m_engineSample = record.m_callback;
    record.m_wallUs = record.m_callback;
    record.m_cpuUs = record.m_callback;
    record.m_gapUs = record.m_callback;
    record.m_previousBudgetUs = 21333;
    record.m_budgetUs = 21333;
    record.m_cpuValid = true;
    record.m_frames = 1024;
    record.m_sampleRate = 48000;
    AppObserver::LogCallback("worst_arrival", record);
    LogMessage message;
    auto& queue = logger.m_queues[ThreadIdToIndex(ThreadId::Message)];
    DOCTEST_REQUIRE(queue.Pop(message));
    DOCTEST_CHECK(message.m_length < LogMessage::x_maxMessageLength - 1);
    DOCTEST_CHECK(std::string(message.m_message).find("mono_us=18446744073709551615") != std::string::npos);
    DOCTEST_CHECK(std::string(message.m_message).find("gap_us=18446744073709551615") != std::string::npos);
    DOCTEST_REQUIRE(queue.Pop(message));
    DOCTEST_CHECK(message.m_length < LogMessage::x_maxMessageLength - 1);
    DOCTEST_CHECK(std::string(message.m_message).find("prev_budget_us=21333") != std::string::npos);
    logger.ResetForTesting();
}
