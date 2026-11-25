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
    static_assert(Ratio > 1);

    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr float x_cutoff = 0.40f;

    ButterworthFilterGeneric<Number, Size, Order> m_filter{x_cutoff / static_cast<float>(Ratio)};

    Vector Process(const Vector& input)
    {
        return m_filter.Process(input);
    }

    Vector GetOutput() const
    {
        return m_filter.GetOutput();
    }

    void Reset()
    {
        m_filter.Reset();
    }
};

template<typename Number, size_t Size, size_t Order>
struct DownsamplerGeneric<Number, Size, Order, 1>
{
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr float x_cutoff = 0.40f;

    Vector m_output;

    Vector Process(const Vector& input)
    {
        m_output = input;
        return m_output;
    }

    Vector GetOutput() const
    {
        return m_output;
    }

    void Reset()
    {
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
    bool m_pushed;

    UpsamplerGeneric()
        : m_filter(x_cutoff / static_cast<float>(Ratio))
        , m_pushed(false)
    {
    }

    void Push(const Vector& input)
    {
        m_input = input;
        m_pushed = true;
    }

    Vector Process()
    {
        const bool pushed = m_pushed;
        m_pushed = false;
        if constexpr (Ratio == 1)
        {
            return m_input;
        }
        else
        {
            Vector sample = pushed ? m_input * static_cast<Number>(Ratio) : Vector();
            return m_filter.Process(sample);
        }
    }

    void Reset()
    {
        m_filter.Reset();
        m_input = Vector();
        m_pushed = false;
    }
};
