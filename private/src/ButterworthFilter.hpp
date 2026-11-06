#pragma once

#include <cmath>
#include <algorithm>
#include <complex>
#include <array>
#include "MultiChannelNumber.hpp"

// Streaming linear-phase decimation. Process returns true on every Ratio-th
// input; GetOutput then represents that input's time minus x_groupDelay.
// Coefficients are a normalized Blackman-windowed sinc. The cutoff is expressed
// in cycles per output sample. Symmetry halves the vector multiplies, and the
// convolution runs only when an output is due.
//
template<typename Number, size_t Size, size_t Taps, size_t Ratio>
struct FIRDownsamplerGeneric
{
    static_assert(Ratio > 0);
    static_assert(Taps >= 3 && Taps % 2 == 1);
    using Vector = MultiChannelNumber<Number, Size>;
    static constexpr size_t x_groupDelay = Ratio == 1 ? 0 : (Taps - 1) / 2;
    static constexpr size_t x_outputPhase = Ratio - 1;

    std::array<Number, Taps> m_coefficients{};
    std::array<Vector, Taps> m_history{};
    Vector m_output;
    size_t m_index = 0;
    size_t m_phase = 0;

    FIRDownsamplerGeneric()
        : FIRDownsamplerGeneric(static_cast<Number>(0.35))
    {
    }

    explicit FIRDownsamplerGeneric(Number cutoff)
    {
        assert(cutoff > 0 && cutoff < static_cast<Number>(0.5));
        double frequency = static_cast<double>(cutoff) / Ratio;
        double sum = 0.0;
        for (size_t tap = 0; tap <= (Taps - 1) / 2; ++tap)
        {
            double offset = static_cast<double>(tap) - (Taps - 1) / 2;
            double sinc = offset == 0.0 ? 2.0 * frequency
                : std::sin(2.0 * M_PI * frequency * offset) / (M_PI * offset);
            double phase = 2.0 * M_PI * tap / (Taps - 1);
            double window = 0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
            m_coefficients[tap] = static_cast<Number>(sinc * window);
            m_coefficients[Taps - 1 - tap] = m_coefficients[tap];
            sum += m_coefficients[tap] * (tap == (Taps - 1) / 2 ? 1.0 : 2.0);
        }

        for (Number& coefficient : m_coefficients)
        {
            coefficient = static_cast<Number>(coefficient / sum);
        }
    }

    bool Process(const Vector& input)
    {
        if constexpr (Ratio == 1)
        {
            m_output = input;
            return true;
        }

        m_history[m_index] = input;
        m_index = (m_index + 1) % Taps;
        if (++m_phase != Ratio)
        {
            return false;
        }

        m_phase = 0;
        constexpr size_t x_middle = (Taps - 1) / 2;
        Vector output = m_history[(m_index + x_middle) % Taps] * m_coefficients[x_middle];
        for (size_t tap = 0; tap < x_middle; ++tap)
        {
            Vector pair = m_history[(m_index + tap) % Taps]
                + m_history[(m_index + Taps - 1 - tap) % Taps];
            output += pair * m_coefficients[tap];
        }

        m_output = output;
        return true;
    }

    const Vector& GetOutput() const
    {
        return m_output;
    }

    void Reset()
    {
        m_history = {};
        m_output = Vector();
        m_index = 0;
        m_phase = 0;
    }
};

// One second-order section; coefficients are shared by every channel.
//
template<typename Number, size_t Size>
struct BiquadSectionGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;

    float m_b0, m_b1, m_b2;
    float m_a1, m_a2;
    Vector m_x1, m_x2;
    Vector m_y1, m_y2;

    BiquadSectionGeneric()
        : m_b0(1.0f)
        , m_b1(0.0f)
        , m_b2(0.0f)
        , m_a1(0.0f)
        , m_a2(0.0f)
    {
    }

    Vector Process(const Vector& input)
    {
        typename Vector::Vector x = input.Load();
        typename Vector::Vector x1 = m_x1.Load();
        typename Vector::Vector x2 = m_x2.Load();
        typename Vector::Vector y1 = m_y1.Load();
        typename Vector::Vector y2 = m_y2.Load();
        Vector output = Vector::FromVector(m_b0 * x + m_b1 * x1 + m_b2 * x2 - m_a1 * y1 - m_a2 * y2);

        m_x2 = m_x1;
        m_x1 = input;
        m_y2 = m_y1;
        m_y1 = output;

        return output;
    }

    void Reset()
    {
        m_x1 = Vector();
        m_x2 = Vector();
        m_y1 = Vector();
        m_y2 = Vector();
    }

    void SetCoefficients(float cosw, float sinw, float q, bool isHighPass)
    {
        float alpha = sinw / (2.0f * q);
        float a0 = 1.0f + alpha;
        float a2 = 1.0f - alpha;
        
        float b0, b1, b2;
        if (isHighPass)
        {
            b0 = (1.0f + cosw) / 2.0f;
            b1 = -(1.0f + cosw);
            b2 = (1.0f + cosw) / 2.0f;
        }
        else
        {
            b0 = (1.0f - cosw) / 2.0f;
            b1 = 1.0f - cosw;
            b2 = (1.0f - cosw) / 2.0f;
        }

        m_b0 = b0 / a0;
        m_b1 = b1 / a0;
        m_b2 = b2 / a0;
        m_a1 = -2.0f * cosw / a0;
        m_a2 = a2 / a0;
    }

    static std::complex<float> TransferFunction(float cosw, float sinw, float q, float freq, bool isHighPass)
    {
        BiquadSectionGeneric biquad;
        biquad.SetCoefficients(cosw, sinw, q, isHighPass);

        // Evaluate transfer function at frequency
        // H(e^jω) = (b0 + b1*e^-jω + b2*e^-j2ω) / (1 + a1*e^-jω + a2*e^-j2ω)
        //
        float omega = 2.0f * M_PI * freq;
        float cos_omega = std::cos(omega);
        float sin_omega = std::sin(omega);
        float cos_2omega = std::cos(2.0f * omega);
        float sin_2omega = std::sin(2.0f * omega);

        // Numerator: b0 + b1*e^-jω + b2*e^-j2ω
        //
        float num_real = biquad.m_b0 + biquad.m_b1 * cos_omega + biquad.m_b2 * cos_2omega;
        float num_imag = -biquad.m_b1 * sin_omega - biquad.m_b2 * sin_2omega;

        // Denominator: 1 + a1*e^-jω + a2*e^-j2ω
        //
        float den_real = 1.0f + biquad.m_a1 * cos_omega + biquad.m_a2 * cos_2omega;
        float den_imag = -biquad.m_a1 * sin_omega - biquad.m_a2 * sin_2omega;

        // Return complex transfer function
        //
        std::complex<float> numerator(num_real, num_imag);
        std::complex<float> denominator(den_real, den_imag);
        return numerator / denominator;
    }

    static float FrequencyResponse(float cosw, float sinw, float q, float freq, bool isHighPass)
    {
        return std::abs(TransferFunction(cosw, sinw, q, freq, isHighPass));
    }
};

using BiquadSection = BiquadSectionGeneric<float, 1>;

// Butterworth low-pass of even Order, built from Order / 2 cascaded biquads.
//
template<typename Number, size_t Size, size_t Order>
struct ButterworthFilterGeneric
{
    static_assert(Order >= 2 && Order % 2 == 0, "Butterworth order must be even");

    using Vector = MultiChannelNumber<Number, Size>;

    static constexpr size_t x_numSections = Order / 2;
    static constexpr float x_minCyclesPerSample = 0.0001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    BiquadSectionGeneric<Number, Size> m_sections[x_numSections];

    float m_cyclesPerSample;

    ButterworthFilterGeneric()
        : m_cyclesPerSample(0.1f)
    {
    }

    Vector Process(const Vector& input)
    {
        Vector output = input;
        for (size_t i = 0; i < x_numSections; ++i)
        {
            output = m_sections[i].Process(output);
        }

        return output;
    }

    // Q of section i: poles at angles (2i + 1) pi / (2 Order).
    //
    static float SectionQ(size_t i)
    {
        return 1.0f / (2.0f * std::cos(static_cast<double>(2 * i + 1) * M_PI / static_cast<double>(2 * Order)));
    }

    void SetCyclesPerSample(float cyclesPerSample)
    {
        m_cyclesPerSample = (cyclesPerSample < x_minCyclesPerSample) ? x_minCyclesPerSample : (cyclesPerSample > x_maxCyclesPerSample) ? x_maxCyclesPerSample : cyclesPerSample;

        // Calculate normalized frequency
        //
        float omega = 2.0f * M_PI * m_cyclesPerSample;
        float cosw = std::cos(omega);
        float sinw = std::sin(omega);

        for (size_t i = 0; i < x_numSections; ++i)
        {
            m_sections[i].SetCoefficients(cosw, sinw, SectionQ(i), false);
        }
    }

    void Reset()
    {
        for (size_t i = 0; i < x_numSections; ++i)
        {
            m_sections[i].Reset();
        }
    }
};

using ButterworthFilter = ButterworthFilterGeneric<float, 1, 8>;
