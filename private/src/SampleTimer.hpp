#pragma once

#include <atomic>

// Engine samples run at x_sampleRate, x_oversample per host frame. The host
// rate is the audio callback rate; effect internals, the encoder matrix and
// recordings that run at the host rate use the Host conversions.
//
struct SampleTimer
{
    static constexpr size_t x_hostSampleRate = 48000;
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_oversampleBits = x_oversample == 1 ? 0 : x_oversample == 2 ? 1 : x_oversample == 4 ? 2 : 3;
    static_assert((static_cast<size_t>(1) << x_oversampleBits) == x_oversample, "The engine runs a power-of-two multiple of the host rate");
    static constexpr size_t x_sampleRate = x_hostSampleRate * x_oversample;
    static constexpr double x_samplePeriod = 1.0 / static_cast<double>(x_sampleRate);
    static constexpr double x_hostSamplePeriod = 1.0 / static_cast<double>(x_hostSampleRate);
    static constexpr double x_slew = 0.01;
    static constexpr size_t x_samplesPerProcessFrame = 512;
    size_t m_samplesPerFrame;
    size_t m_frame;
    size_t m_sample;
    size_t m_frameZeroTimeUs;
    bool m_needsTimeAnchor = true;
    static SampleTimer* s_instance;
    
    SampleTimer()
        : m_samplesPerFrame(0)
        , m_frame(0)
        , m_sample(0)
        , m_frameZeroTimeUs(0)
    {
    }

    static void Init(size_t samplesPerFrame)
    {
        s_instance = new SampleTimer();
        s_instance->m_samplesPerFrame = samplesPerFrame;
    }

    static void StartFrame(size_t wallclockTimeUs)
    {
        if (s_instance->m_needsTimeAnchor)
        {
            s_instance->m_frameZeroTimeUs = wallclockTimeUs - s_instance->m_sample * 1000 * 1000 / x_sampleRate;
            s_instance->m_needsTimeAnchor = false;
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

    // Reattach wall-clock time on the next audio block, retaining musical time.
    // Call only while the audio source is detached.
    //
    static void Resume()
    {
        s_instance->m_needsTimeAnchor = true;
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

    static size_t GetSample()
    {
        return s_instance ? s_instance->m_sample : 0;
    }

    static size_t GetHostSample()
    {
        return GetSample() / x_oversample;
    }

    static bool IsHostFrame()
    {
        return GetSubSampleIndex() == 0;
    }

    static size_t GetSubSampleIndex()
    {
        return GetSample() % x_oversample;
    }

    static constexpr double HzToCycPerSamp(double hz)
    {
        return hz / static_cast<double>(x_sampleRate);
    }

    static constexpr double HzToCycPerHostSamp(double hz)
    {
        return hz / static_cast<double>(x_hostSampleRate);
    }

    static constexpr double CycPerSampToHz(double cyclesPerSample)
    {
        return cyclesPerSample * static_cast<double>(x_sampleRate);
    }

    static constexpr double CycPerHostSampToHz(double cyclesPerSample)
    {
        return cyclesPerSample * static_cast<double>(x_hostSampleRate);
    }

    static constexpr double SecondsToSamples(double seconds)
    {
        return seconds * static_cast<double>(x_sampleRate);
    }

    static constexpr double SecondsToHostSamples(double seconds)
    {
        return seconds * static_cast<double>(x_hostSampleRate);
    }

    // Per-sample increment that traverses a unit range in the given time.
    //
    static constexpr double SecondsToCycPerSamp(double seconds)
    {
        return 1.0 / (seconds * static_cast<double>(x_sampleRate));
    }

    static constexpr double SecondsToCycPerHostSamp(double seconds)
    {
        return 1.0 / (seconds * static_cast<double>(x_hostSampleRate));
    }
};

inline SampleTimer* SampleTimer::s_instance = nullptr;
