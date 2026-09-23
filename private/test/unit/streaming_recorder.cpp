#include "doctest.h"
#include "StreamingRecorder.hpp"
#include "AsyncLogger.hpp"
#include "StateInterchange.hpp"

#include <limits>

namespace
{
    struct MemorySink : StreamingRecorder::Sink
    {
        std::vector<uint8_t> m_bytes;
        std::atomic<bool> m_stall{false};
        std::atomic<bool> m_entered{false};
        std::atomic<bool> m_stallOpen{false};
        bool m_failOpen = false;
        bool m_failWrite = false;
        bool m_failClose = false;
        bool m_throwClose = false;

        bool Open(const std::string&) override
        {
            while (m_stallOpen)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            m_bytes.clear();
            return !m_failOpen;
        }

        bool Write(const uint8_t* bytes, size_t size) override
        {
            if (size >= 4 && std::memcmp(bytes, "BLK", 3) == 0)
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
        DOCTEST_REQUIRE(recorder.BeginFrame());
    }

    ParamEvent MakeParamEvent(const char* name, size_t sample, int scene, char value)
    {
        ParamEvent event;
        event.m_type = ParamEvent::Type::StateChange;
        event.m_sample = sample;
        event.m_name = name;
        event.m_scene = scene;
        event.m_valueLen = 1;
        event.m_value[0] = value;
        return event;
    }
}

DOCTEST_TEST_CASE("streaming recorder: queued patch readers prevent reuse until the writer copies them")
{
    StateInterchange interchange;
    DOCTEST_REQUIRE(interchange.RequestLoadText("{\"old\":42}", true));
    JSON original = interchange.GetToLoad();
    interchange.AckLoadCompleted();
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    memory->m_stallOpen = true;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    DOCTEST_REQUIRE(recorder.Start());
    auto& arena = interchange.m_loadArena;
    recorder.RecordParamEvent(ParamEvent::MkPatch(arena, true, false, 0));
    recorder.RecordParamEvent(ParamEvent::MkPatch(arena, false, false, 0));
    DOCTEST_CHECK(arena.m_readers == 2);
    DOCTEST_CHECK_FALSE(arena.TryWrite());
    DOCTEST_CHECK(interchange.RequestLoadText("{\"new\":99}", true));
    DOCTEST_CHECK_FALSE(interchange.m_pendingLoad.empty());
    DOCTEST_CHECK(original.Get("old").IntegerValue() == 42);
    memory->m_stallOpen = false;
    const bool copied = Await([&]() { return arena.m_readers == 0; });
    DOCTEST_CHECK(copied);
    interchange.RetryPendingLoad();
    DOCTEST_CHECK(interchange.m_pendingLoad.empty());
    DOCTEST_CHECK(interchange.GetToLoad().Get("new").IntegerValue() == 99);
    DOCTEST_CHECK(recorder.BeginFrame());
    recorder.CommitFrame();
    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_CHECK(recorder.GetError() == StreamingRecorder::Error::None);
    const std::string bytes(memory->m_bytes.begin(), memory->m_bytes.end());
    const size_t first = bytes.find("{\"old\":42}");
    DOCTEST_REQUIRE(first != std::string::npos);
    DOCTEST_CHECK(bytes.find("{\"old\":42}", first + 1) != std::string::npos);
    DOCTEST_CHECK(bytes.find("{\"new\":99}") == std::string::npos);
}

DOCTEST_TEST_CASE("streaming recorder: failed sessions release queued and out-of-tail patches")
{
    PatchArena arena(4096);
    DOCTEST_REQUIRE(arena.TryWrite());
    arena.FinishWrite(arena.Loads("{\"patch\":1}"));
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    memory->m_stallOpen = true;
    DOCTEST_SUBCASE("open failure")
    {
        memory->m_failOpen = true;
    }

    DOCTEST_SUBCASE("write failure")
    {
        memory->m_failWrite = true;
    }

    DOCTEST_SUBCASE("close failure")
    {
        memory->m_failClose = true;
    }

    DOCTEST_SUBCASE("discarded tail")
    {
    }

    size_t copies = 2;
    DOCTEST_SUBCASE("queue overflow")
    {
        copies = StreamingRecorder::x_queueParamEvents + 1;
    }

    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    DOCTEST_REQUIRE(recorder.Start());
    for (size_t i = 0; i < copies; ++i)
    {
        recorder.RecordParamEvent(ParamEvent::MkPatch(arena, true, false, i));
    }

    if (copies == 2)
    {
        DOCTEST_CHECK(recorder.BeginFrame());
        recorder.CommitFrame();
    }
    recorder.Stop();
    memory->m_stallOpen = false;
    recorder.Shutdown();
    DOCTEST_CHECK(arena.m_readers == 0);
    DOCTEST_CHECK(arena.TryWrite());
    arena.Reset();
    arena.FinishWrite(JSON::Null());
}

DOCTEST_TEST_CASE("streaming recorder: captures audio and events while the file is opening")
{
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    memory->m_stallOpen = true;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    const bool started = recorder.Start();
    std::string name = "A";
    for (size_t frame = 0; frame < 9; ++frame)
    {
        if (frame == 0 || frame == 8)
        {
            recorder.RecordParamEvent(MakeParamEvent(name.c_str(), frame, 0, 1));
        }

        DOCTEST_CHECK(recorder.BeginFrame());
        recorder.CommitFrame();
    }

    recorder.Stop();
    memory->m_stallOpen = false;
    recorder.Shutdown();
    DOCTEST_CHECK(started);
    DOCTEST_CHECK(recorder.GetError() == StreamingRecorder::Error::None);
    DOCTEST_CHECK(recorder.m_writtenFrames == 9);
    const auto& bytes = memory->m_bytes;
    DOCTEST_REQUIRE(bytes.size() >= 12);
    size_t offset = 12 + ReadLE(bytes, 8, 4);
    DOCTEST_CHECK(ReadLE(bytes, offset + 16, 4) == 8);
    DOCTEST_CHECK(ReadLE(bytes, offset + 22, 4) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 35, 4) == 0);
    offset += ReadLE(bytes, offset + 4, 4);
    DOCTEST_CHECK(ReadLE(bytes, offset + 16, 4) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 22, 4) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 35, 4) == 0);
    offset += ReadLE(bytes, offset + 4, 4);
    DOCTEST_CHECK(std::memcmp(bytes.data() + offset, "END1", 4) == 0);
    DOCTEST_CHECK(ReadLE(bytes, offset + 4, 8) == 9);
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

DOCTEST_TEST_CASE("streaming recorder: immediate stop and rapid one-frame sessions do not lose the tail")
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

DOCTEST_TEST_CASE("streaming recorder: event queue is prepared before capture")
{
    StreamingRecorder recorder;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::make_unique<MemorySink>()));
    DOCTEST_REQUIRE(recorder.m_paramEventsRing != nullptr);
}

DOCTEST_TEST_CASE("streaming recorder: snapshot and events share the first recorded frame")
{
    struct Patch
    {
        int m_value = 17;

        JSON ToJSON(JsonArena& arena)
        {
            JSON patch = arena.Object();
            patch.SetNew("unsaved", arena.Integer(m_value));
            return patch;
        }
    } patch;
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    DOCTEST_REQUIRE(recorder.Start(patch.ToJSON(recorder.m_patchArena)));
    patch.m_value = 99;
    std::string name = "A";
    for (size_t frame = 0; frame < 9; ++frame)
    {
        if (frame == 0 || frame == 7 || frame == 8)
        {
            ParamEvent event = MakeParamEvent(name.c_str(), frame, 2, static_cast<char>(frame + 1));
            recorder.RecordParamEvent(event);
            if (frame == 7)
            {
                event.m_value[0] = 42;
                recorder.RecordParamEvent(event);
            }
        }

        DOCTEST_REQUIRE(recorder.BeginFrame());
        recorder.CommitFrame();
    }

    recorder.RecordParamEvent(MakeParamEvent(name.c_str(), 9, 2, 77));
    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_REQUIRE(recorder.GetError() == StreamingRecorder::Error::None);
    const auto& bytes = memory->m_bytes;
    const size_t headerSize = ReadLE(bytes, 8, 4);
    const std::string headerText(bytes.begin() + 12, bytes.begin() + 12 + headerSize);
    JsonArena arena(8192);
    const JSON header = arena.Loads(headerText.c_str());
    DOCTEST_CHECK(header.Get("initial_patch").Get("unsaved").IntegerValue() == 17);
    size_t offset = 12 + headerSize;
    DOCTEST_CHECK(ReadLE(bytes, offset + 22, 4) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 30, 4) == 3);
    DOCTEST_CHECK(ReadLE(bytes, offset + 35, 4) == 0);
    DOCTEST_CHECK(bytes.at(offset + 44) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 45, 4) == 7);
    DOCTEST_CHECK(bytes.at(offset + 54) == 8);
    DOCTEST_CHECK(bytes.at(offset + 64) == 42);
    offset += ReadLE(bytes, offset + 4, 4);
    DOCTEST_CHECK(ReadLE(bytes, offset + 8, 8) == 8);
    DOCTEST_CHECK(ReadLE(bytes, offset + 30, 4) == 1);
    DOCTEST_CHECK(ReadLE(bytes, offset + 35, 4) == 0);
    DOCTEST_CHECK(bytes.at(offset + 44) == 9);
    offset += ReadLE(bytes, offset + 4, 4);
    DOCTEST_CHECK(std::memcmp(bytes.data() + offset, "END1", 4) == 0);

    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::make_unique<MemorySink>()));
    DOCTEST_REQUIRE(recorder.m_paramEventsRing->IsEmpty());
}

DOCTEST_TEST_CASE("streaming recorder: event queue overflow fails instead of losing state edits")
{
    StreamingRecorder recorder;
    auto sink = std::make_unique<MemorySink>();
    auto* memory = sink.get();
    memory->m_stall = true;
    DOCTEST_REQUIRE(recorder.Prepare(Session(), "/unused", std::move(sink)));
    Ready(recorder);
    for (size_t frame = 0; frame < StreamingRecorder::x_pageFrames; ++frame)
    {
        recorder.BeginFrame();
        recorder.CommitFrame();
    }

    const bool stalled = Await([&]() { return memory->m_entered.load(); });
    std::string name = "A";
    for (size_t event = 0; event <= StreamingRecorder::x_queueParamEvents; ++event)
    {
        recorder.RecordParamEvent(MakeParamEvent(
            name.c_str(), StreamingRecorder::x_pageFrames, 0, 1));
    }

    const auto error = recorder.GetError();
    memory->m_stall = false;
    recorder.Shutdown();
    DOCTEST_CHECK(stalled);
    DOCTEST_CHECK(error == StreamingRecorder::Error::Overrun);
    DOCTEST_CHECK(recorder.m_paramEventsRing->IsEmpty());
}
