#include "doctest.h"
#include "MidiSysexQueue.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

DOCTEST_TEST_CASE("SysEx queue owns complete bytes after source mutation")
{
    SmartGrid::MidiSysexQueue<16, 2> queue;
    uint8_t bytes[] = {0xF0, 0x79, 0x01, 0xF7};
    DOCTEST_REQUIRE(queue.TryPush(bytes, sizeof(bytes), 3));
    bytes[1] = 0;
    auto* message = queue.Peek();
    DOCTEST_REQUIRE(message != nullptr);
    DOCTEST_CHECK(message->m_routeId == 3);
    DOCTEST_CHECK(message->m_size == 4);
    DOCTEST_CHECK(message->m_data[0] == 0xF0);
    DOCTEST_CHECK(message->m_data[1] == 0x79);
    DOCTEST_CHECK(message->m_data[2] == 0x01);
    DOCTEST_CHECK(message->m_data[3] == 0xF7);
    queue.Pop();
    DOCTEST_CHECK(queue.Peek() == nullptr);
    DOCTEST_CHECK(queue.Size() == 0);
}

DOCTEST_TEST_CASE("Full SysEx queue preserves held slot and FIFO through producer wraparound")
{
    SmartGrid::MidiSysexQueue<8, 2> queue;
    const uint8_t first[] = {0xF0, 0x11, 0xF7};
    const uint8_t second[] = {0xF0, 0x22, 0x33, 0xF7};
    const uint8_t third[] = {0xF0, 0x44, 0xF7};
    DOCTEST_REQUIRE(queue.TryPush(first, sizeof(first), 5));
    auto* held = queue.Peek();
    DOCTEST_REQUIRE(held != nullptr);
    DOCTEST_REQUIRE(queue.TryPush(second, sizeof(second), 9));
    DOCTEST_CHECK_FALSE(queue.TryPush(third, sizeof(third), 1));
    DOCTEST_CHECK(queue.Size() == 2);
    DOCTEST_CHECK(queue.Peek() == held);
    DOCTEST_CHECK(held->m_routeId == 5);
    DOCTEST_CHECK(held->m_size == 3);
    DOCTEST_CHECK(held->m_data[0] == 0xF0);
    DOCTEST_CHECK(held->m_data[1] == 0x11);
    DOCTEST_CHECK(held->m_data[2] == 0xF7);
    queue.Pop();
    DOCTEST_REQUIRE(queue.TryPush(third, sizeof(third), 1));
    auto* message = queue.Peek();
    DOCTEST_REQUIRE(message != nullptr);
    DOCTEST_CHECK(message->m_routeId == 9);
    DOCTEST_CHECK(message->m_size == 4);
    DOCTEST_CHECK(message->m_data[0] == 0xF0);
    DOCTEST_CHECK(message->m_data[1] == 0x22);
    DOCTEST_CHECK(message->m_data[2] == 0x33);
    DOCTEST_CHECK(message->m_data[3] == 0xF7);
    queue.Pop();
    message = queue.Peek();
    DOCTEST_REQUIRE(message != nullptr);
    DOCTEST_CHECK(message->m_routeId == 1);
    DOCTEST_CHECK(message->m_size == 3);
    DOCTEST_CHECK(message->m_data[0] == 0xF0);
    DOCTEST_CHECK(message->m_data[1] == 0x44);
    DOCTEST_CHECK(message->m_data[2] == 0xF7);
    queue.Pop();
    DOCTEST_CHECK(queue.Size() == 0);
}

DOCTEST_TEST_CASE("SysEx queue rejects invalid payloads without changing queued packets")
{
    SmartGrid::MidiSysexQueue<4, 2> queue;
    const uint8_t bytes[] = {0xF0, 0x79, 0x01, 0xF7, 0x55};
    DOCTEST_CHECK_FALSE(queue.TryPush(nullptr, 4, 2));
    DOCTEST_CHECK_FALSE(queue.TryPush(bytes, 0, 2));
    DOCTEST_CHECK_FALSE(queue.TryPush(bytes, 5, 2));
    DOCTEST_CHECK(queue.Size() == 0);
    DOCTEST_REQUIRE(queue.TryPush(bytes, 4, 2));
    DOCTEST_CHECK_FALSE(queue.TryPush(bytes, 5, 8));
    DOCTEST_CHECK(queue.Size() == 1);
    auto* message = queue.Peek();
    DOCTEST_REQUIRE(message != nullptr);
    DOCTEST_CHECK(message->m_routeId == 2);
    DOCTEST_CHECK(message->m_size == 4);
    DOCTEST_CHECK(message->m_data[3] == 0xF7);
}

DOCTEST_TEST_CASE("SysEx queue preserves exact maximum payload including final F7")
{
    SmartGrid::MidiSysexQueue<2048, 2> queue;
    uint8_t bytes[2048];
    for (size_t i = 0; i < 2048; ++i)
    {
        bytes[i] = static_cast<uint8_t>(i % 128);
    }

    bytes[0] = 0xF0;
    bytes[2047] = 0xF7;
    DOCTEST_REQUIRE(queue.TryPush(bytes, sizeof(bytes), 15));
    auto* message = queue.Peek();
    DOCTEST_REQUIRE(message != nullptr);
    DOCTEST_CHECK(message->m_size == 2048);
    DOCTEST_CHECK(message->m_routeId == 15);
    DOCTEST_CHECK(message->m_data[0] == 0xF0);
    for (size_t i = 1; i < 2047; ++i)
    {
        DOCTEST_CHECK(message->m_data[i] == i % 128);
    }

    DOCTEST_CHECK(message->m_data[2047] == 0xF7);
}

DOCTEST_TEST_CASE("Real SysEx producer and consumer preserve varying published route and bytes")
{
    SmartGrid::MidiSysexQueue<64, 4> queue;
    constexpr size_t x_packetCount = 8192;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::atomic<bool> cancel{false};
    size_t produced = 0;
    size_t consumed = 0;
    bool correct = true;
    std::atomic<bool> observerDone{false};
    bool boundedObservation = true;
    std::thread observer([&]()
    {
        while (!observerDone.load())
        {
            if (queue.Size() > 4)
            {
                boundedObservation = false;
            }
        }
    });
    std::thread producer([&]()
    {
        for (size_t sequence = 0; sequence < x_packetCount && !cancel.load(); ++sequence)
        {
            uint8_t bytes[64];
            const size_t size = 3 + sequence % 62;
            bytes[0] = 0xF0;
            for (size_t i = 1; i + 1 < size; ++i)
            {
                bytes[i] = static_cast<uint8_t>((sequence + i * 7) % 128);
            }

            bytes[size - 1] = 0xF7;
            while (!queue.TryPush(bytes, size, static_cast<int>(sequence % 16)))
            {
                if (cancel.load() || std::chrono::steady_clock::now() >= deadline)
                {
                    cancel.store(true);
                    return;
                }

                std::this_thread::yield();
            }

            ++produced;
            std::memset(bytes, 0, sizeof(bytes));
        }
    });
    while (consumed < x_packetCount && !cancel.load())
    {
        auto* message = queue.Peek();
        if (message != nullptr)
        {
            const size_t size = 3 + consumed % 62;
            correct = correct && message->m_routeId == static_cast<int>(consumed % 16);
            correct = correct && message->m_size == size;
            correct = correct && message->m_data[0] == 0xF0;
            for (size_t i = 1; i + 1 < size; ++i)
            {
                correct = correct && message->m_data[i] == (consumed + i * 7) % 128;
            }

            std::this_thread::yield();
            correct = correct && message->m_data[size - 1] == 0xF7;
            queue.Pop();
            ++consumed;
        }
        else
        {
            std::this_thread::yield();
        }

        if (std::chrono::steady_clock::now() >= deadline)
        {
            cancel.store(true);
        }
    }

    producer.join();
    observerDone.store(true);
    observer.join();
    DOCTEST_CHECK(boundedObservation);
    DOCTEST_CHECK_FALSE(cancel.load());
    DOCTEST_CHECK(produced == 8192);
    DOCTEST_CHECK(consumed == 8192);
    DOCTEST_CHECK(correct);
    DOCTEST_CHECK(queue.Size() == 0);
}
