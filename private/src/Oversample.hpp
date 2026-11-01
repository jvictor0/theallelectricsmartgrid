#pragma once

#include "ButterworthFilter.hpp"
#include "SampleTimer.hpp"

// Per-sample decimator by Ratio. Process every high-rate sample; GetOutput()
// holds the latest filtered sample, read once per Ratio samples. Ratio 1 is a
// passthrough.
//
template<typename Number, size_t Size, size_t Order, size_t Ratio>
struct DownsamplerGeneric
{
    static_assert(Ratio >= 1, "Downsampler ratio must be at least 1");

    using Vector = MultiChannelNumber<Number, Size>;

    // Cutoff in cycles per low-rate sample.
    //
    static constexpr float x_cutoff = 0.40f;

    ButterworthFilterGeneric<Number, Size, Order> m_filter;
    Vector m_output;

    DownsamplerGeneric()
    {
        m_filter.SetCyclesPerSample(x_cutoff / static_cast<float>(Ratio));
    }

    Vector Process(const Vector& input)
    {
        if constexpr (Ratio == 1)
        {
            m_output = input;
        }
        else
        {
            m_output = m_filter.Process(input);
        }

        return m_output;
    }

    Vector GetOutput() const
    {
        return m_output;
    }

    void Reset()
    {
        m_filter.Reset();
        m_output = Vector();
    }
};

// Per-sample interpolator by Ratio. Push once per Ratio samples at the low
// rate, then Process every high-rate sample. Zero-stuffs with gain Ratio and
// low-passes the images away. Ratio 1 is a passthrough.
//
template<typename Number, size_t Size, size_t Order, size_t Ratio>
struct UpsamplerGeneric
{
    static_assert(Ratio >= 1, "Upsampler ratio must be at least 1");

    using Vector = MultiChannelNumber<Number, Size>;

    // Cutoff in cycles per low-rate sample.
    //
    static constexpr float x_cutoff = 0.40f;

    ButterworthFilterGeneric<Number, Size, Order> m_filter;
    Vector m_input;
    Vector m_output;
    bool m_pushed;

    UpsamplerGeneric()
        : m_pushed(false)
    {
        m_filter.SetCyclesPerSample(x_cutoff / static_cast<float>(Ratio));
    }

    void Push(const Vector& input)
    {
        m_input = input;
        m_pushed = true;
    }

    Vector Process()
    {
        if constexpr (Ratio == 1)
        {
            m_output = m_input;
        }
        else
        {
            Vector sample = m_pushed ? m_input * static_cast<Number>(Ratio) : Vector();
            m_output = m_filter.Process(sample);
        }

        m_pushed = false;
        return m_output;
    }

    void Reset()
    {
        m_filter.Reset();
        m_input = Vector();
        m_output = Vector();
        m_pushed = false;
    }
};

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
