// Standalone recorder measurement; see docs/streaming-recording-format.md.
//
#include "StreamingRecorder.hpp"
#include "SmartGridBuildInfo.hpp"
#include <iostream>
#include <sys/resource.h>

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
    if (argc != 4)
    {
        std::cerr << "usage: benchmark_recording OUTPUT_DIRECTORY silence|audio|noise SECONDS\n";
        return 2;
    }

    const std::string mode = argv[2];
    const size_t seconds = std::stoul(argv[3]);
    if ((mode != "silence" && mode != "audio" && mode != "noise") || seconds == 0)
    {
        return 2;
    }

    RecordingFormat::Session session;
    session.m_gitCommitSha = SmartGridBuildInfo::x_gitCommitSha;
    auto add = [&](RecordingFormat::TrackType type, const std::string& role)
    {
        const auto id = static_cast<uint32_t>(session.m_tracks.size());
        session.m_tracks.push_back({id, "track-" + std::to_string(id), type, role,
            role.find("master_") == 0 ? "post_mastering_pre_master_volume" : "post_fader"});
    };

    for (size_t i = 0; i < 17; ++i)
    {
        add(RecordingFormat::TrackType::PannedMono, "input");
    }

    for (size_t i = 0; i < 9; ++i)
    {
        add(RecordingFormat::TrackType::Mono, "mono_input");
    }

    for (size_t i = 0; i < 3; ++i)
    {
        add(RecordingFormat::TrackType::Quad, "return");
    }

    add(RecordingFormat::TrackType::Quad, "master_quad");
    add(RecordingFormat::TrackType::Stereo, "master_stereo");
    StreamingRecorder recorder;
    std::filesystem::create_directories(argv[1]);
    if (!recorder.Prepare(session, argv[1]))
    {
        return 1;
    }

    g_countAllocations = true;
    const bool started = recorder.Start();
    g_countAllocations = false;
    if (!started)
    {
        return 1;
    }

    while (!recorder.BeginFrame())
    {
        if (recorder.GetError() != StreamingRecorder::Error::None)
        {
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const size_t streams = session.StreamCount();
    std::vector<float> page(streams * StreamingRecorder::x_pageFrames);
    uint32_t random = 0x12345678;
    uint64_t captureNanos = 0;
    uint64_t maximumPageNanos = 0;
    const auto start = std::chrono::steady_clock::now();
    const size_t total = seconds * session.m_sampleRate;
    for (size_t base = 0; base < total; base += StreamingRecorder::x_pageFrames)
    {
        const size_t count = std::min(StreamingRecorder::x_pageFrames, total - base);
        for (size_t frame = 0; frame < count; ++frame)
        {
            for (size_t stream = 0; stream < streams; ++stream)
            {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                const bool position = recorder.m_positions[stream];
                const float sine = std::sin(static_cast<double>(base + frame) * (position ? 0.00001 : 0.01) + stream);
                const float noise = static_cast<float>(random) / 4294967295.0f;
                page[frame * streams + stream] = mode == "silence" ? 0.0f
                    : mode == "noise" ? (position ? noise : noise * 2 - 1)
                    : position ? sine * 0.5f + 0.5f : sine * 0.3f;
            }
        }

        const auto captureStart = std::chrono::steady_clock::now();
        g_countAllocations = true;
        for (size_t frame = 0; frame < count; ++frame)
        {
            recorder.BeginFrame();
            for (size_t track = 0; track < session.m_tracks.size(); ++track)
            {
                recorder.Submit(track, page.data() + frame * streams + recorder.m_trackOffsets[track],
                    RecordingFormat::StreamCount(session.m_tracks[track].m_type));
            }

            recorder.CommitFrame();
        }

        g_countAllocations = false;
        const auto nanos = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - captureStart).count());
        captureNanos += nanos;
        maximumPageNanos = std::max(maximumPageNanos, nanos);
        if (recorder.GetError() != StreamingRecorder::Error::None)
        {
            break;
        }

        std::this_thread::sleep_until(start + std::chrono::microseconds((base + count) * 1000000 / session.m_sampleRate));
    }

    g_countAllocations = true;
    recorder.Stop();
    g_countAllocations = false;
    recorder.Shutdown();
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    std::cout << "mode=" << mode << " frames=" << recorder.m_writtenFrames
        << " bytes=" << recorder.m_writtenBytes << " ratio="
        << static_cast<double>(recorder.m_writtenBytes) / (total * streams * 3)
        << " worker_max_us=" << recorder.m_maxBlockMicros
        << " ring_high_water=" << recorder.m_queueHighWater
        << " capture_ns_per_frame=" << captureNanos / total
        << " capture_page_max_us=" << maximumPageNanos / 1000
        << " capture_allocations=" << g_allocations
        << " maxrss=" << usage.ru_maxrss
        << " error=" << static_cast<int>(recorder.GetError()) << '\n';
    return recorder.GetError() == StreamingRecorder::Error::None && recorder.m_writtenFrames == total
        && g_allocations == 0 ? 0 : 1;
}
