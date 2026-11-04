#pragma once

#include "SampleTimer.hpp"
#include "SampleTop.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

inline double CPUSeconds()
{
    timespec time;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0)
    {
        throw std::runtime_error("Cannot read thread CPU time");
    }

    return time.tv_sec + time.tv_nsec * 1e-9;
}

inline void PrepareCPU()
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

inline double PositiveSeconds(const char* text)
{
    size_t consumed = 0;
    double seconds = std::stod(text, &consumed);
    if (consumed != std::string(text).size() || !std::isfinite(seconds) || seconds <= 0 || seconds > 86400)
    {
        throw std::runtime_error("Seconds must be finite and in (0, 86400]");
    }

    return seconds;
}

inline int Integer(const char* text)
{
    size_t consumed = 0;
    int value = std::stoi(text, &consumed);
    if (consumed != std::string(text).size())
    {
        throw std::runtime_error("Invalid integer argument");
    }

    return value;
}

struct ComponentOptions
{
    const char* m_kind;
    int m_lanes;
    int m_mode;
    double m_seconds;
    const char* m_dump;

    ComponentOptions(int argc, char** argv)
    {
        if (argc != 5 && argc != 6)
        {
            throw std::runtime_error("Usage: benchmark KIND LANES MODE SECONDS [OUTPUT.f32]");
        }

        m_kind = argv[1];
        m_lanes = Integer(argv[2]);
        m_mode = Integer(argv[3]);
        m_seconds = PositiveSeconds(argv[4]);
        m_dump = argc == 6 ? argv[5] : nullptr;
        if (m_lanes < 1 || m_lanes > 4 || m_mode < 0 || m_mode > 3)
        {
            throw std::runtime_error("LANES must be 1..4; MODE must be 0..3");
        }
    }
};

struct Signals
{
    size_t m_period;
    std::vector<std::array<float, 4>> m_audio;
    std::vector<std::array<float, 4>> m_controls;

    explicit Signals(size_t rate) : m_period(32768 * rate / 48000), m_audio(m_period), m_controls(m_period)
    {
        for (size_t n = 0; n < m_period; ++n)
        {
            double phase = 2 * std::acos(-1.0) * n / m_period;
            for (size_t c = 0; c < 4; ++c)
            {
                m_audio[n][c] = static_cast<float>(0.12 * std::sin(phase * (113 + 67 * c)) +
                                                   0.04 * std::cos(phase * (397 + 97 * c)));
                m_controls[n][c] = static_cast<float>(std::sin(phase + c * 0.7));
            }
        }
    }
};

template <typename Work>
auto PrepareBlock(Work& work, size_t n, size_t count, int) -> decltype(work.PrepareBlock(n, count), void())
{
    work.PrepareBlock(n, count);
}

template <typename Work>
void PrepareBlock(Work&, size_t, size_t, long)
{
}

template <typename Work>
[[gnu::noinline]] void RunBlock(Work& work, size_t start, size_t count,
                                std::vector<std::array<float, 4>>& output)
{
    for (size_t n = start; n < start + count; ++n)
    {
        SampleTimer::s_instance->m_sample = n;
        output[n % output.size()] = work.Process(n);
    }

    // Consume the output and preserve observable state across timed blocks.
    //
    asm volatile("" : : "g"(output.data()) : "memory");
}

template <typename Work>
void Measure(Work& work, const char* kind, int mode, size_t rate, double seconds, double warmup,
             const char* path)
{
    const size_t block = 128 * rate / 48000;
    const size_t frames = static_cast<size_t>(seconds * rate) / block * block;
    const size_t warm = static_cast<size_t>(warmup * rate) / block * block;
    if (frames == 0)
    {
        throw std::runtime_error("Duration must include at least one 128-host-sample block");
    }

    std::vector<std::array<float, 4>> output(32768 * rate / 48000);
    for (size_t n = 0; n < warm; n += block)
    {
        PrepareBlock(work, n, block, 0);
        RunBlock(work, n, block, output);
    }

    double cpuElapsed = 0;
    std::vector<double> times;
    times.reserve(frames / block);
    for (size_t n = warm; n < warm + frames; n += block)
    {
        // Clock fixtures are prepared outside the measured DSP work.
        // Parameter mapping and setters inside Process remain included.
        //
        PrepareBlock(work, n, block, 0);
        const auto wallStart = std::chrono::steady_clock::now();
        double cpuStart = CPUSeconds();
        RunBlock(work, n, block, output);
        cpuElapsed += CPUSeconds() - cpuStart;
        times.push_back(
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - wallStart).count());
    }

    double sum = 0;
    double peak = 0;
    for (const auto& frame : output)
    {
        for (float value : frame)
        {
            if (!std::isfinite(value))
            {
                throw std::runtime_error("Nonfinite benchmark output");
            }

            sum += static_cast<double>(value) * value;
            peak = std::max(peak, std::abs(static_cast<double>(value)));
        }
    }

    std::sort(times.begin(), times.end());
    if (path)
    {
        std::ofstream dump(path, std::ios::binary);
        dump.write(reinterpret_cast<const char*>(output.data()), output.size() * sizeof(output[0]));
        if (!dump)
        {
            throw std::runtime_error("Cannot write output capture");
        }
    }

    double audioSeconds = static_cast<double>(frames) / rate;
    std::printf("{\"kind\":\"%s\",\"mode\":%d,\"rate\":%zu,\"frames\":%zu,"
                "\"audio_seconds\":%.9f,\"cpu_seconds\":%.9f,\"ms_per_audio_second\":%.6f,"
                "\"block_p50_us\":%.6f,\"block_p99_us\":%.6f,\"rms\":%.9g,\"peak\":%.9g,\"finite\":true}\n",
                kind, mode, rate, frames, audioSeconds, cpuElapsed, cpuElapsed * 1000 / audioSeconds,
                times[times.size() / 2], times[times.size() * 99 / 100], std::sqrt(sum / (output.size() * 4)),
                peak);
}

inline void Require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

template <typename Action>
int BenchmarkMain(int argc, char** argv, Action action)
{
    try
    {
        ComponentOptions options(argc, argv);
        PrepareCPU();
        SampleTimer::Init(512);
        action(options);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
