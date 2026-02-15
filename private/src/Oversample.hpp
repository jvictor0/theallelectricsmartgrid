#pragma once

#include "ButterworthFilter.hpp"
#include "SampleTimer.hpp"

struct Upsampler
{
    ButterworthFilter m_filter;
    size_t m_oversampleRate;

    Upsampler()
        : m_oversampleRate(1)
    {
    }

    Upsampler(size_t oversampleRate)
        : m_oversampleRate(oversampleRate)
    {
        m_filter.SetCyclesPerSample(0.40 / m_oversampleRate);
    }

    void Process(const float* input, float* output)
    {
        size_t outputSize = SampleTimer::x_controlFrameRate * m_oversampleRate;
        for (size_t i = 0; i < outputSize; ++i)
        {
            // Zero-fill: place source sample at every m_oversampleRate position
            //
            float sample = 0.0f;
            if (i % m_oversampleRate == 0)
            {
                sample = input[i / m_oversampleRate] * m_oversampleRate;
            }

            // Lowpass filter to remove imaging
            //
            output[i] = m_filter.Process(sample);
        }
    }
};

struct Downsampler
{
    ButterworthFilter m_filter;
    size_t m_oversampleRate;

    Downsampler()
        : m_oversampleRate(1)
    {
    }

    Downsampler(size_t oversampleRate)
        : m_oversampleRate(oversampleRate)
    {
        m_filter.SetCyclesPerSample(0.40 / m_oversampleRate);
    }

    void Process(const float* input, float* output)
    {
        size_t inputSize = SampleTimer::x_controlFrameRate * m_oversampleRate;
        for (size_t i = 0; i < inputSize; ++i)
        {
            float filtered = m_filter.Process(input[i]);
            if ((i + 1) % m_oversampleRate == 0)
            {
                output[i / m_oversampleRate] = filtered;
            }
        }
    }
};
