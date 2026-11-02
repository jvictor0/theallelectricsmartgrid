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
