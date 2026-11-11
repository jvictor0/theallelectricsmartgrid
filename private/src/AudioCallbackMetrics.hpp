#pragma once

#include "AudioCallbackDiagnostics.hpp"
#include "CircularQueue.hpp"
#include <array>
#include <chrono>
#include <ctime>
#include <type_traits>

struct AudioCallbackClock
{
    struct Stamp
    {
        uint64_t m_wallUs = 0;
        uint64_t m_cpuNs = 0;
        bool m_cpuValid = false;
    };

    static uint64_t WallUs()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    static Stamp Read()
    {
        Stamp result;
        result.m_wallUs = WallUs();
        timespec cpu{};
        result.m_cpuValid = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu) == 0;
        if (result.m_cpuValid)
        {
            result.m_cpuNs = static_cast<uint64_t>(cpu.tv_sec) * 1000000000 + static_cast<uint64_t>(cpu.tv_nsec);
        }

        return result;
    }
};

struct AudioCallbackMetrics
{
    static constexpr size_t x_batches = 64;
    static constexpr size_t x_incidentsPerBatch = 16;
    static constexpr uint64_t x_batchUs = 1000000;

    struct Record
    {
        uint64_t m_callback = 0;
        uint64_t m_startUs = 0;
        uint64_t m_engineSample = 0;
        uint64_t m_wallUs = 0;
        uint64_t m_cpuUs = 0;
        uint64_t m_gapUs = 0;
        uint64_t m_previousBudgetUs = 0;
        uint64_t m_budgetUs = 0;
        int m_frames = 0;
        double m_sampleRate = 0;
        bool m_cpuValid = false;

        uint64_t ArrivalExcessUs() const
        {
            return m_previousBudgetUs > 0 && m_gapUs > m_previousBudgetUs ? m_gapUs - m_previousBudgetUs : 0;
        }

        bool Overrun() const
        {
            return m_budgetUs > 0 && m_wallUs > m_budgetUs;
        }

        bool LongGap() const
        {
            return m_previousBudgetUs > 0 && m_gapUs > 2 * m_previousBudgetUs;
        }
    };

    struct Statistics
    {
        uint64_t m_count = 0;
        uint64_t m_cpuCount = 0;
        uint64_t m_wallUs = 0;
        uint64_t m_cpuUs = 0;
        uint64_t m_budgetUs = 0;
        uint64_t m_firstStartUs = 0;
        uint64_t m_lastStartUs = 0;
        Record m_worstWall;
        Record m_worstCpu;
        Record m_worstArrival;

        void Add(const Record& record)
        {
            if (m_count == 0)
            {
                m_firstStartUs = record.m_startUs;
            }

            ++m_count;
            m_cpuCount += record.m_cpuValid;
            m_wallUs += record.m_wallUs;
            m_cpuUs += record.m_cpuUs;
            m_budgetUs += record.m_budgetUs;
            m_lastStartUs = record.m_startUs;
            KeepWorst(record, record, record);
        }

        void KeepWorst(const Record& wall, const Record& cpu, const Record& arrival)
        {
            if (m_worstWall.m_callback == 0 || wall.m_wallUs > m_worstWall.m_wallUs)
            {
                m_worstWall = wall;
            }

            if (cpu.m_cpuValid && (!m_worstCpu.m_cpuValid || cpu.m_cpuUs > m_worstCpu.m_cpuUs))
            {
                m_worstCpu = cpu;
            }

            if (m_worstArrival.m_callback == 0 || arrival.ArrivalExcessUs() > m_worstArrival.ArrivalExcessUs())
            {
                m_worstArrival = arrival;
            }
        }

        void Merge(const Statistics& other)
        {
            if (other.m_count == 0)
            {
                return;
            }

            if (m_count == 0)
            {
                m_firstStartUs = other.m_firstStartUs;
            }

            m_count += other.m_count;
            m_cpuCount += other.m_cpuCount;
            m_wallUs += other.m_wallUs;
            m_cpuUs += other.m_cpuUs;
            m_budgetUs += other.m_budgetUs;
            m_lastStartUs = other.m_lastStartUs;
            KeepWorst(other.m_worstWall, other.m_worstCpu, other.m_worstArrival);
        }
    };

    struct Batch
    {
        Statistics m_statistics;
        std::array<Record, x_incidentsPerBatch> m_incidents{};
        size_t m_incidentCount = 0;
        uint64_t m_omittedIncidents = 0;
    };

    struct Start
    {
        AudioCallbackClock::Stamp m_clock;
        Record m_record;
    };

    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<size_t>::is_always_lock_free);
    static_assert(std::is_trivially_copyable_v<Batch>);

    // The callback is the only producer; the existing message-thread reporter
    // is the only consumer. Full queues retain the pending aggregate without
    // waiting. Individual incidents are capped; omitted incidents are counted.
    //
    AudioCallbackDiagnostics m_arrivals;
    Batch m_pending;
    CircularQueue<Batch, x_batches> m_batches;
    std::atomic<uint64_t> m_callbacks{0};
    std::atomic<uint64_t> m_overruns{0};
    std::atomic<uint64_t> m_longGaps{0};
    std::atomic<uint64_t> m_lastLongGapUs{0};

    Start BeginAt(AudioCallbackClock::Stamp clock, int frames, double sampleRate, uint64_t engineSample)
    {
        Start start;
        start.m_clock = clock;
        const auto arrival = m_arrivals.Observe(clock.m_wallUs, frames, sampleRate);
        start.m_record.m_startUs = clock.m_wallUs;
        start.m_record.m_engineSample = engineSample;
        start.m_record.m_gapUs = arrival.m_gapUs;
        start.m_record.m_previousBudgetUs = arrival.m_previousBudgetUs;
        start.m_record.m_budgetUs = m_arrivals.m_previousBudgetUs;
        start.m_record.m_frames = frames;
        start.m_record.m_sampleRate = sampleRate;
        return start;
    }

    Start Begin(int frames, double sampleRate, uint64_t engineSample)
    {
        return BeginAt(AudioCallbackClock::Read(), frames, sampleRate, engineSample);
    }

    void EndAt(Start start, AudioCallbackClock::Stamp end)
    {
        auto& record = start.m_record;
        record.m_callback = m_callbacks.fetch_add(1, std::memory_order_relaxed) + 1;
        record.m_wallUs = end.m_wallUs - start.m_clock.m_wallUs;
        record.m_cpuValid = start.m_clock.m_cpuValid && end.m_cpuValid && end.m_cpuNs >= start.m_clock.m_cpuNs;
        if (record.m_cpuValid)
        {
            record.m_cpuUs = (end.m_cpuNs - start.m_clock.m_cpuNs) / 1000;
        }

        if (record.Overrun())
        {
            m_overruns.fetch_add(1, std::memory_order_relaxed);
        }

        if (record.LongGap())
        {
            m_longGaps.fetch_add(1, std::memory_order_relaxed);
            m_lastLongGapUs.store(record.m_gapUs, std::memory_order_relaxed);
        }

        m_pending.m_statistics.Add(record);
        if (record.Overrun() || record.LongGap())
        {
            if (m_pending.m_incidentCount < x_incidentsPerBatch)
            {
                m_pending.m_incidents[m_pending.m_incidentCount++] = record;
            }
            else
            {
                ++m_pending.m_omittedIncidents;
            }
        }

        if (end.m_wallUs - m_pending.m_statistics.m_firstStartUs >= x_batchUs)
        {
            Publish();
        }
    }

    void End(Start start)
    {
        EndAt(start, AudioCallbackClock::Read());
    }

    void Publish()
    {
        if (m_pending.m_statistics.m_count > 0 && m_batches.Push(m_pending))
        {
            m_pending = {};
        }
    }

    // Called only with audio quiescent, preserving already queued diagnostics.
    // Warm clock initialization before the first callback of a device session.
    //
    void Prepare()
    {
        static_cast<void>(AudioCallbackClock::Read());
        Publish();
        m_arrivals.Reset();
    }
};
