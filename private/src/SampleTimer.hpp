#pragma once

#include <atomic>

struct SampleTimer
{
    static constexpr size_t x_sampleRate = 48000;
    static constexpr double x_slew = 0.01;
    static constexpr size_t x_samplesPerProcessFrame = 512;
    static constexpr size_t x_controlFrameRate = 8;
    size_t m_samplesPerFrame;
    size_t m_frame;
    size_t m_sample;
    size_t m_frameZeroTimeUs;
    static SampleTimer* s_instance;
    
    SampleTimer()
    {
        m_frame = 0;
        m_sample = 0;
        m_frameZeroTimeUs = 0;
    }

    static void Init(size_t samplesPerFrame)
    {
        s_instance = new SampleTimer();
        s_instance->m_samplesPerFrame = samplesPerFrame;
    }

    static void StartFrame(size_t wallclockTimeUs)
    {
        if (s_instance->m_frame == 0)
        {
            s_instance->m_frameZeroTimeUs = wallclockTimeUs;
        }
        else
        {
            // Estimate frame 0 time based on current wallclock and number of samples processed
            //
            size_t frameZeroEstimate = wallclockTimeUs - s_instance->m_sample * 1000 * 1000 / x_sampleRate;
            s_instance->m_frameZeroTimeUs = s_instance->m_frameZeroTimeUs * (1 - x_slew) + frameZeroEstimate * x_slew;            
        }

        ++s_instance->m_frame;
    }

    static size_t GetAbsTimeUs()
    {
        return s_instance->m_frameZeroTimeUs + s_instance->m_sample * 1000 * 1000 / x_sampleRate;
    }

    static bool IncrementSample()
    {
        ++s_instance->m_sample;
        return s_instance->m_sample % x_samplesPerProcessFrame == 0;
    }

    static bool IsControlFrame()
    {
        return s_instance->m_sample % x_controlFrameRate == 0;
    }

    static int GetUBlockIndex()
    {
        return s_instance->m_sample % x_controlFrameRate;
    }

    static size_t GetSample()
    {
        return s_instance->m_sample;
    }
};

inline SampleTimer* SampleTimer::s_instance = nullptr;