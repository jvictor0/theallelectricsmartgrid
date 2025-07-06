#pragma once

#include <atomic>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

template <typename T, size_t N>
struct CircularQueue
{
    T m_data[N];
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};

    bool Push(const T& value)
    {
        if (m_head.load() - m_tail.load() == N)
        {
            return false;
        }

        m_data[m_head.load() % N] = value;
        m_head.fetch_add(1);
        return true;
    }

    bool Pop(T& value)
    {
        if (m_head.load() == m_tail.load())
        {
            return false;
        }

        value = m_data[m_tail.load() % N];
        m_tail.fetch_add(1);
        return true;
    }

    bool Peek(T& value)
    {
        if (m_head.load() == m_tail.load())
        {
            return false;
        }

        value = m_data[m_tail.load() % N];
        return true;
    }

    size_t Size() const
    {
        return m_head.load() - m_tail.load();
    }

    bool IsEmpty() const
    {
        return m_head.load() == m_tail.load();
    }
};

// ByteBuffer struct to hold fixed-size byte buffers with size information
template <size_t BufferSize>
struct ByteBuffer
{
    uint8_t m_buffer[BufferSize];
    size_t m_size = 0;

    void Clear()
    {
        m_size = 0;
    }

    bool IsFull() const
    {
        return m_size >= BufferSize;
    }

    bool IsEmpty() const
    {
        return m_size == 0;
    }

    size_t AvailableSpace() const
    {
        return BufferSize - m_size;
    }
};

template <size_t BufferSize, size_t QueueSize>
struct CircularByteQueue : public CircularQueue<ByteBuffer<BufferSize>, QueueSize>
{
    ByteBuffer<BufferSize> m_nextToSend;

    // Write bytes into the current buffer
    //
    size_t Write(const uint8_t* data, size_t length)
    {
        size_t written = 0;
        
        while (written < length)
        {
            size_t available = m_nextToSend.AvailableSpace();
            
            size_t toWrite = std::min(available, length - written);
            std::memcpy(m_nextToSend.m_buffer + m_nextToSend.m_size, data + written, toWrite);
            m_nextToSend.m_size += toWrite;
            written += toWrite;

            // If we filled the buffer completely, flush it
            //
            if (toWrite == available)
            {
                Flush();
            }
        }

        return written;
    }

    // Flush the current buffer to the queue (even if not full)
    //
    bool Flush()
    {
        if (!m_nextToSend.IsEmpty())
        {
            while (!this->Push(m_nextToSend))
            {
                // Queue is full, sleep and try again
                //
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            m_nextToSend.Clear();
        }

        return true;
    }

    // Check if there's data in the current buffer
    //
    bool HasPendingData() const
    {
        return !m_nextToSend.IsEmpty();
    }

    // Get the current buffer size
    //
    size_t GetCurrentBufferSize() const
    {
        return m_nextToSend.m_size;
    }
}; 