#pragma once

#include "CircularQueue.hpp"
#include <atomic>
#include <thread>
#include <fstream>
#include <string>
#include <cassert>

struct FileWriter
{
    static constexpr size_t x_bufferSize = 4096;
    static constexpr size_t x_queueSize = 10;
    
    CircularByteQueue<x_bufferSize, x_queueSize> m_queue;
    std::atomic<bool> m_done{false};
    std::thread m_writeThread;
    std::ofstream m_file;

    ~FileWriter()
    {
        if (m_writeThread.joinable())
        {
            Close();
        }
    }

    bool Open(const std::string& filename)
    {
        INFO("Opening file: %s", filename.c_str());
        if (m_writeThread.joinable())
        {
            assert(false);
        }

        m_done = false;
        m_writeThread = std::thread([this, filename]()
        {
            INFO("Starting write thread");
            WriteThreadFunction(filename);
        });

        return true;
    }

    void Close()
    {
        if (!m_writeThread.joinable())
        {
            assert(false);
        }

        // Flush any remaining data in the buffer
        //
        m_queue.Flush();

        // Signal the thread to stop
        //
        m_done = true;

        // Wait for the thread to finish
        //
        m_writeThread.join();
    }

    size_t Write(const uint8_t* data, size_t length)
    {
        return m_queue.Write(data, length);
    } 

    void WriteThreadFunction(const std::string& filename)
    {
        // Open the file for writing (overwrite mode)
        //
        m_file.open(filename, std::ios::binary | std::ios::trunc);
        
        if (!m_file.is_open())
        {
            assert(false);
        }

        ByteBuffer<x_bufferSize> buffer;

        // Keep reading from queue and writing to file until done
        //
        while (!m_done.load() || !m_queue.IsEmpty())
        {
            if (m_queue.Pop(buffer))
            {
                // Write the buffer to file
                //
                m_file.write(reinterpret_cast<const char*>(buffer.m_buffer), buffer.m_size);
                
                if (!m_file.good())
                {
                    assert(false);
                }
            }
            else
            {
                // No data available, sleep briefly
                //
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        m_file.close();
    }
}; 