#include "doctest.h"
#include "AudioCallbackMetrics.hpp"
#include <thread>

namespace
{
    void Complete(AudioCallbackMetrics& metrics, uint64_t startUs, uint64_t wallUs, uint64_t cpuUs, uint64_t sample)
    {
        const auto start = metrics.BeginAt({startUs, 1000000, true}, 1024, 48000.0, sample);
        metrics.EndAt(start, {startUs + wallUs, 1000000 + cpuUs * 1000, true});
    }
}

DOCTEST_TEST_CASE("audio metrics preserve paired timings and positions for different worst callbacks")
{
    AudioCallbackMetrics metrics;
    Complete(metrics, 1000000, 8000, 7000, 0);
    Complete(metrics, 1021333, 30000, 7000, 4096);
    Complete(metrics, 1071333, 22000, 21000, 8192);
    metrics.Publish();
    AudioCallbackMetrics::Batch batch;
    DOCTEST_REQUIRE(metrics.m_batches.Pop(batch));
    const auto& stats = batch.m_statistics;
    DOCTEST_CHECK(stats.m_count == 3);
    DOCTEST_CHECK(stats.m_cpuCount == 3);
    DOCTEST_CHECK(stats.m_wallUs == 60000);
    DOCTEST_CHECK(stats.m_cpuUs == 35000);
    DOCTEST_CHECK(stats.m_worstWall.m_startUs == 1021333);
    DOCTEST_CHECK(stats.m_worstWall.m_engineSample == 4096);
    DOCTEST_CHECK(stats.m_worstWall.m_cpuUs == 7000);
    DOCTEST_CHECK(stats.m_worstCpu.m_wallUs == 22000);
    DOCTEST_CHECK(stats.m_worstCpu.m_engineSample == 8192);
    DOCTEST_CHECK(stats.m_worstArrival.ArrivalExcessUs() == 28667);
    DOCTEST_CHECK(batch.m_incidentCount == 2);
    DOCTEST_CHECK(metrics.m_overruns.load() == 2);
    DOCTEST_CHECK(metrics.m_longGaps.load() == 1);
}

DOCTEST_TEST_CASE("audio metrics retain small arrival delays below the old long gap threshold")
{
    AudioCallbackMetrics metrics;
    Complete(metrics, 1000000, 16000, 15000, 0);
    Complete(metrics, 1026333, 16000, 15000, 4096);
    metrics.Publish();
    AudioCallbackMetrics::Batch batch;
    DOCTEST_REQUIRE(metrics.m_batches.Pop(batch));
    DOCTEST_CHECK(batch.m_statistics.m_worstArrival.ArrivalExcessUs() == 5000);
    DOCTEST_CHECK(batch.m_statistics.m_worstArrival.m_cpuUs == 15000);
    DOCTEST_CHECK(batch.m_incidentCount == 0);
    DOCTEST_CHECK(metrics.m_longGaps.load() == 0);
}

DOCTEST_TEST_CASE("audio metrics report unavailable CPU clocks without inventing zero CPU measurements")
{
    AudioCallbackMetrics metrics;
    auto start = metrics.BeginAt({1000000, 5000, false}, 1024, 48000, 0);
    metrics.EndAt(start, {1010000, 500000, true});
    start = metrics.BeginAt({1021333, 5000, true}, 1024, 48000, 4096);
    metrics.EndAt(start, {1031333, 0, false});
    start = metrics.BeginAt({1042666, 5000, true}, 1024, 48000, 8192);
    metrics.EndAt(start, {1052666, 4000, true});
    metrics.Publish();
    AudioCallbackMetrics::Batch batch;
    DOCTEST_REQUIRE(metrics.m_batches.Pop(batch));
    DOCTEST_CHECK(batch.m_statistics.m_count == 3);
    DOCTEST_CHECK(batch.m_statistics.m_cpuCount == 0);
    DOCTEST_CHECK_FALSE(batch.m_statistics.m_worstCpu.m_cpuValid);
    DOCTEST_CHECK(batch.m_statistics.m_wallUs == 30000);
    DOCTEST_CHECK(batch.m_statistics.m_cpuUs == 0);
}

DOCTEST_TEST_CASE("audio metrics keep aggregate evidence when the reporter stalls and bound incident storage")
{
    AudioCallbackMetrics metrics;
    const size_t count = AudioCallbackMetrics::x_batches + 100;
    for (size_t i = 0; i < count; ++i)
    {
        Complete(metrics, 1000000 + i * 60000, 30000 + i, 10000, i * 4096);
        metrics.Publish();
    }

    DOCTEST_CHECK(metrics.m_batches.Size() == AudioCallbackMetrics::x_batches);
    DOCTEST_CHECK(metrics.m_pending.m_statistics.m_count == 100);
    DOCTEST_CHECK(metrics.m_pending.m_incidentCount == AudioCallbackMetrics::x_incidentsPerBatch);
    DOCTEST_CHECK(metrics.m_pending.m_omittedIncidents == 100 - AudioCallbackMetrics::x_incidentsPerBatch);
    DOCTEST_CHECK(metrics.m_overruns.load() == count);
    AudioCallbackMetrics::Statistics combined;
    AudioCallbackMetrics::Batch batch;
    while (metrics.m_batches.Pop(batch))
    {
        combined.Merge(batch.m_statistics);
    }

    metrics.Publish();
    DOCTEST_REQUIRE(metrics.m_batches.Pop(batch));
    combined.Merge(batch.m_statistics);
    DOCTEST_CHECK(combined.m_count == count);
    DOCTEST_CHECK(combined.m_worstWall.m_wallUs == 30000 + count - 1);
    DOCTEST_CHECK(combined.m_worstWall.m_engineSample == (count - 1) * 4096);
    DOCTEST_CHECK(metrics.m_pending.m_statistics.m_count == 0);
}

DOCTEST_TEST_CASE("audio metrics device restart preserves queued records and resets arrival history")
{
    AudioCallbackMetrics metrics;
    Complete(metrics, 1000000, 16000, 15000, 0);
    metrics.Prepare();
    Complete(metrics, 9000000, 17000, 16000, 4096);
    metrics.Publish();
    AudioCallbackMetrics::Batch first, second;
    DOCTEST_REQUIRE(metrics.m_batches.Pop(first));
    DOCTEST_REQUIRE(metrics.m_batches.Pop(second));
    DOCTEST_CHECK(first.m_statistics.m_worstWall.m_callback == 1);
    DOCTEST_CHECK(second.m_statistics.m_worstWall.m_callback == 2);
    DOCTEST_CHECK(second.m_statistics.m_worstWall.m_gapUs == 0);
    DOCTEST_CHECK(metrics.m_longGaps.load() == 0);
}

DOCTEST_TEST_CASE("audio metrics transfer coherent batches across producer and consumer threads")
{
    AudioCallbackMetrics metrics;
    std::atomic<bool> done{false};
    AudioCallbackMetrics::Statistics combined;
    std::thread consumer([&]()
    {
        AudioCallbackMetrics::Batch batch;
        while (!done.load() || !metrics.m_batches.IsEmpty())
        {
            if (metrics.m_batches.Pop(batch))
            {
                combined.Merge(batch.m_statistics);
            }
        }
    });
    for (size_t i = 0; i < 10000; ++i)
    {
        Complete(metrics, 1000000 + i * 21333, 10000 + i, 5000 + i, i * 4096);
    }

    done = true;
    consumer.join();
    metrics.Publish();
    AudioCallbackMetrics::Batch tail;
    while (metrics.m_batches.Pop(tail))
    {
        combined.Merge(tail.m_statistics);
    }

    DOCTEST_CHECK(combined.m_count == 10000);
    DOCTEST_CHECK(combined.m_worstWall.m_wallUs == 19999);
    DOCTEST_CHECK(combined.m_worstWall.m_cpuUs == 14999);
    DOCTEST_CHECK(combined.m_worstWall.m_engineSample == 9999 * 4096);
}
