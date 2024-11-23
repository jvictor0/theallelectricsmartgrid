#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>

struct DebugLog
{
    std::ofstream m_file;
    std::mutex m_mutex;

    DebugLog()
    {
        m_file.open("/tmp/joyo_smart_grid.log");
    }

    void Log(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        m_mutex.lock();
        m_file << buffer << std::endl;
        m_mutex.unlock();
    }
};

static DebugLog g_log;
