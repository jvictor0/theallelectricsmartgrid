#include "Bench.hpp"
#include "AudioCallbackMetrics.hpp"

thread_local bool g_countAllocations = false;
thread_local size_t g_allocations = 0;

void* operator new(size_t bytes)
{
    g_allocations += g_countAllocations;
    if (void* pointer = std::malloc(bytes == 0 ? 1 : bytes))
    {
        return pointer;
    }

    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}

void* operator new[](size_t bytes)
{
    return ::operator new(bytes);
}

void operator delete[](void* pointer) noexcept
{
    ::operator delete(pointer);
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "Usage: perf_audio_diagnostics CALLBACKS");
        const int count = Integer(argv[1]);
        Require(count > 0, "Callback count must be positive");
        PrepareCPU();
        Require(AudioCallbackClock::Read().m_cpuValid, "Thread CPU clock unavailable");
        for (const std::string mode : {"wall_only", "metrics", "incident_storm", "reporter_stalled"})
        {
            const bool wallOnly = mode == "wall_only";
            const bool stalled = mode == "reporter_stalled";
            const bool incident = stalled || mode == "incident_storm";
            auto metrics = std::make_unique<AudioCallbackMetrics>();
            metrics->Prepare();
            AudioCallbackMetrics::Batch batch;
            AudioCallbackDiagnostics arrivals;
            uint64_t checksum = 0;
            if (stalled)
            {
                for (size_t i = 0; i < AudioCallbackMetrics::x_batches; ++i)
                {
                    metrics->m_batches.Push(batch);
                }

                // An already overdue pending batch exercises the full-queue
                // publication attempt on every callback, even in short runs.
                //
                metrics->m_pending.m_statistics.Add({});
            }

            const auto wall = std::chrono::steady_clock::now();
            const double cpu = CPUSeconds();
            g_allocations = 0;
            g_countAllocations = true;
            for (int i = 0; i < count; ++i)
            {
                if (wallOnly)
                {
                    const auto start = AudioCallbackClock::WallUs();
                    const auto observation = arrivals.Observe(start, 1024, 48000);
                    checksum += AudioCallbackClock::WallUs() - start + observation.m_gapUs;
                }
                else
                {
                    const auto start = metrics->Begin(1024, 48000, static_cast<uint64_t>(i) * 4096);
                    auto end = AudioCallbackClock::Read();
                    if (incident)
                    {
                        end.m_wallUs += 30000;
                    }

                    metrics->EndAt(start, end);
                    if (i % 48 == 47)
                    {
                        metrics->Publish();
                        if (!stalled && metrics->m_batches.Pop(batch))
                        {
                            checksum += batch.m_statistics.m_count;
                        }
                    }
                }
            }

            g_countAllocations = false;
            const double elapsedCpu = CPUSeconds() - cpu;
            const double elapsedWall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall).count();
            Require(g_allocations == 0, "Callback metrics allocated memory");
            std::cout << "{\"mode\":\"" << mode << "\",\"callbacks\":" << count
                << ",\"cpu_ns_per_callback\":" << elapsedCpu * 1e9 / count
                << ",\"wall_ns_per_callback\":" << elapsedWall * 1e9 / count
                << ",\"allocations\":" << g_allocations << ",\"checksum\":" << checksum
                << ",\"metrics_bytes\":" << sizeof(AudioCallbackMetrics) << "}\n";
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
