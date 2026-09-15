#include "doctest.h"
#include "StreamingRecorder.hpp"
#include "AsyncLogger.hpp"

#include <limits>

namespace
{
    struct MemorySink : StreamingRecorder::Sink
    {
        std::vector<uint8_t> m_bytes;
        std::atomic<bool> m_stall{false};
        std::atomic<bool> m_entered{false};
        bool m_failOpen = false;
        bool m_failWrite = false;
        bool m_failClose = false;
        bool m_throwClose = false;

        bool Open(const std::string&) override
        {
            m_bytes.clear();
            return !m_failOpen;
        }

        bool Write(const uint8_t* bytes, size_t size) override
        {
            if (size >= 4 && std::memcmp(bytes, "BLK1", 4) == 0)
            {
                m_entered = true;
                while (m_stall)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                if (m_failWrite)
                {
                    return false;
                }
            }

            m_bytes.insert(m_bytes.end(), bytes, bytes + size);
            return true;
        }

        bool Close(const std::vector<uint8_t>& completion) override
        {
            if (m_throwClose)
            {
                throw std::runtime_error("sink close failed");
            }

            if (m_failClose)
            {
                return false;
            }

            m_bytes.insert(m_bytes.end(), completion.begin(), completion.end());
            return true;
        }
    };

    template <typename Predicate>
    bool Await(Predicate ready)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!ready() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return ready();
    }

    RecordingFormat::Session Session()
    {
        RecordingFormat::Session session;
        session.m_gitCommitSha = std::string(40, 'a');
        session.m_blockFrames = 8;
        session.m_tracks = {{0, "voice", RecordingFormat::TrackType::PannedMono, "input", "post_fader"}};
        return session;
    }

    uint64_t ReadLE(const std::vector<uint8_t>& bytes, size_t offset, size_t size)
    {
        uint64_t value = 0;
        for (size_t i = 0; i < size; ++i)
        {
            value |= static_cast<uint64_t>(bytes.at(offset + i)) << (i * 8);
        }

        return value;
    }

    void Ready(StreamingRecorder& recorder)
    {
        DOCTEST_REQUIRE(recorder.Start());
        DOCTEST_REQUIRE(Await([&]() { return recorder.BeginFrame(); }));
    }
}

DOCTEST_TEST_CASE("streaming recorder: partial pages, skipped submissions, stop and restart")
{
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    Ready(recorder);
    DOCTEST_CHECK_FALSE(recorder.Start());
    const float frame[] = {0.5f, 0.25f, 0.75f};
    recorder.Submit(0, frame, 3);
    recorder.CommitFrame();
    for (size_t i = 0; i < 8; ++i)
    {
        DOCTEST_REQUIRE(recorder.BeginFrame());
        recorder.CommitFrame();
    }

    recorder.Stop();
    DOCTEST_REQUIRE(Await([&]() { return recorder.GetState() == StreamingRecorder::State::Idle; }));
    const auto& bytes = memory->m_bytes;
    size_t offset = 12 + ReadLE(bytes, 8, 4);
    DOCTEST_CHECK(ReadLE(bytes, offset + 16, 4) == 8);
    DOCTEST_CHECK(ReadLE(bytes, offset + 20, 2) == 1);
    offset += ReadLE(bytes, offset + 4, 4);
    DOCTEST_CHECK(ReadLE(bytes, offset + 16, 4) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 20, 2) == 0);
    offset += ReadLE(bytes, offset + 4, 4);
    DOCTEST_CHECK(std::memcmp(bytes.data() + offset, "END1", 4) == 0);
    DOCTEST_CHECK(ReadLE(bytes, offset + 4, 8) == 9);
    Ready(recorder);
    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_CHECK(ReadLE(memory->m_bytes, memory->m_bytes.size() - 12, 8) == 0);
}

DOCTEST_TEST_CASE("streaming recorder: invalid frame drains accepted prefix without completion")
{
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    Ready(recorder);
    recorder.CommitFrame();
    DOCTEST_REQUIRE(recorder.BeginFrame());
    const float frame[] = {std::numeric_limits<float>::infinity(), 0.5f, 0.5f};
    recorder.Submit(0, frame, 3);
    recorder.CommitFrame();
    recorder.Shutdown();
    DOCTEST_CHECK(recorder.GetError() == StreamingRecorder::Error::InvalidSample);
    const size_t offset = 12 + ReadLE(memory->m_bytes, 8, 4);
    DOCTEST_CHECK(ReadLE(memory->m_bytes, offset + 16, 4) == 1);
    DOCTEST_CHECK(memory->m_bytes.size() == offset + ReadLE(memory->m_bytes, offset + 4, 4));
}

DOCTEST_TEST_CASE("streaming recorder: stalled sink overrun keeps stop independent of queue capacity")
{
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    memory->m_stall = true;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    Ready(recorder);
    for (size_t i = 0; i < StreamingRecorder::x_pageFrames; ++i)
    {
        recorder.BeginFrame();
        recorder.CommitFrame();
    }

    const bool entered = Await([&]() { return memory->m_entered.load(); });
    for (size_t i = 0; i < StreamingRecorder::x_pageFrames * StreamingRecorder::x_queuePages; ++i)
    {
        recorder.BeginFrame();
        recorder.CommitFrame();
    }

    recorder.Stop();
    const auto error = recorder.GetError();
    memory->m_stall = false;
    recorder.Shutdown();
    DOCTEST_CHECK(entered);
    DOCTEST_CHECK(error == StreamingRecorder::Error::Overrun);
    DOCTEST_CHECK(recorder.m_writtenFrames == StreamingRecorder::x_pageFrames * StreamingRecorder::x_queuePages);
    DOCTEST_CHECK(recorder.m_queueHighWater == StreamingRecorder::x_queuePages);
}

DOCTEST_TEST_CASE("streaming recorder: open, write and close failures remain observable")
{
    for (int failure = 0; failure < 4; ++failure)
    {
        StreamingRecorder recorder;
        auto sink = std::make_unique<MemorySink>();
        auto* memory = sink.get();
        memory->m_failOpen = failure == 0;
        memory->m_failWrite = failure == 1;
        memory->m_failClose = failure == 2;
        memory->m_throwClose = failure == 3;
        DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
        DOCTEST_REQUIRE(recorder.Start());
        DOCTEST_REQUIRE(Await([&]() { return recorder.BeginFrame() || recorder.GetError() != StreamingRecorder::Error::None; }));
        recorder.CommitFrame();
        recorder.Stop();
        DOCTEST_REQUIRE(Await([&]() { return recorder.m_state == StreamingRecorder::State::Error; }));
        const auto expected = failure == 0 ? StreamingRecorder::Error::Open
            : failure == 1 ? StreamingRecorder::Error::Write : StreamingRecorder::Error::Close;
        DOCTEST_CHECK(recorder.GetError() == expected);
        DOCTEST_CHECK(recorder.GetState() == StreamingRecorder::State::Error);
        DOCTEST_CHECK((memory->m_bytes.size() < 16
            || std::memcmp(memory->m_bytes.data() + memory->m_bytes.size() - 16, "END1", 4) != 0));
        memory->m_failOpen = false;
        memory->m_failWrite = false;
        memory->m_failClose = false;
        memory->m_throwClose = false;
        Ready(recorder);
        recorder.CommitFrame();
        recorder.Stop();
        recorder.Shutdown();
        DOCTEST_CHECK(recorder.GetState() == StreamingRecorder::State::Idle);
        DOCTEST_CHECK(ReadLE(memory->m_bytes, memory->m_bytes.size() - 12, 8) == 1);
    }
}

DOCTEST_TEST_CASE("streaming recorder: cancellation and rapid one-frame sessions do not lose the tail")
{
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    DOCTEST_REQUIRE(recorder.Start());
    recorder.Stop();
    DOCTEST_REQUIRE(Await([&]() { return recorder.m_state == StreamingRecorder::State::Idle; }));
    for (size_t iteration = 0; iteration < 50; ++iteration)
    {
        Ready(recorder);
        recorder.CommitFrame();
        recorder.Stop();
        DOCTEST_REQUIRE(Await([&]() { return recorder.m_state == StreamingRecorder::State::Idle; }));
        DOCTEST_REQUIRE(memory->m_bytes.size() >= 16);
        DOCTEST_CHECK(ReadLE(memory->m_bytes, memory->m_bytes.size() - 12, 8) == 1);
    }
}

DOCTEST_TEST_CASE("streaming recorder: capture errors enter the async log once with recording context")
{
    auto& logger = AsyncLogQueue::s_instance;
    logger.ResetForTesting();
    ScopedThreadId thread(ThreadId::Audio);
    StreamingRecorder recorder;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::make_unique<MemorySink>()));
    Ready(recorder);
    recorder.CommitFrame();
    recorder.Fail(StreamingRecorder::Error::InvalidSample);
    recorder.BeginFrame();
    recorder.BeginFrame();
    recorder.Shutdown();
    auto& queue = logger.m_queues[ThreadIdToIndex(ThreadId::Audio)];
    DOCTEST_REQUIRE(queue.Size() == 1);
    const std::string message(queue.PeekPtr()->m_message);
    DOCTEST_CHECK(message.find("error=InvalidSample") != std::string::npos);
    DOCTEST_CHECK(message.find("accepted_frames=1") != std::string::npos);
    DOCTEST_CHECK(message.find("written_frames=") != std::string::npos);
    DOCTEST_CHECK(message.find("written_bytes=") != std::string::npos);
    DOCTEST_CHECK(message.find("queue_high_water=") != std::string::npos);
    DOCTEST_CHECK(logger.QueueSizeForTesting(ThreadId::FileWriter) == 0);
    logger.ResetForTesting();
}

DOCTEST_TEST_CASE("streaming recorder: shutdown reports worker errors through the owner's async queue")
{
    auto& logger = AsyncLogQueue::s_instance;
    logger.ResetForTesting();
    ScopedThreadId thread(ThreadId::Message);
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    sink->m_failClose = true;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    Ready(recorder);
    recorder.CommitFrame();
    recorder.Stop();
    DOCTEST_REQUIRE(Await([&]() { return recorder.m_state == StreamingRecorder::State::Error; }));
    recorder.Shutdown();
    auto& queue = logger.m_queues[ThreadIdToIndex(ThreadId::Message)];
    DOCTEST_REQUIRE(queue.Size() == 1);
    const std::string message(queue.PeekPtr()->m_message);
    DOCTEST_CHECK(message.find("error=Close") != std::string::npos);
    DOCTEST_CHECK(message.find("written_frames=1") != std::string::npos);
    DOCTEST_CHECK(logger.QueueSizeForTesting(ThreadId::FileWriter) == 0);
    logger.ResetForTesting();
}
