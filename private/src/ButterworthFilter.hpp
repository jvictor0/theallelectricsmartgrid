#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <type_traits>
#include "ButterworthDownsampler2Generic.hpp"
#include "ButterworthDownsampler2Prepare.hpp"
#include "ButterworthDownsampler4Generic.hpp"
#include "ButterworthDownsampler4Prepare.hpp"
#include "ButterworthDownsampler8Generic.hpp"
#include "ButterworthDownsampler8Prepare.hpp"
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

template<typename Number, size_t Size, size_t Order>
struct ButterworthFilterGeneric
{
    static_assert(Order == 2 || Order == 4 || Order == 8, "Supported Butterworth orders are 2, 4 and 8");

    using Vector = MultiChannelNumber<Number, Size>;
    using Core = std::conditional_t<Order == 2, ButterworthDownsampler2Generic<Number, Size>,
        std::conditional_t<Order == 4, ButterworthDownsampler4Generic<Number, Size>,
            ButterworthDownsampler8Generic<Number, Size>>>;
    using Preparation = std::conditional_t<Order == 2, ButterworthDownsampler2Prepare,
        std::conditional_t<Order == 4, ButterworthDownsampler4Prepare, ButterworthDownsampler8Prepare>>;

    static constexpr float x_minCyclesPerSample = 0.0001f;
    static constexpr float x_maxCyclesPerSample = 0.499f;

    Core m_core;

    explicit ButterworthFilterGeneric(float cyclesPerSample)
    {
        const double cutoff = std::clamp(cyclesPerSample, x_minCyclesPerSample, x_maxCyclesPerSample);
        InstallSections<Order>(m_core.m_state, Preparation::Prepare(cutoff));
    }

    template<size_t SubOrder, typename State, typename Prepared>
    static void InstallSections(State& state, const Prepared& prepared)
    {
        if constexpr (SubOrder == 2)
        {
            state.m_coefficients = {
                Vector::Splat(static_cast<Number>(prepared.m_gain)),
                Vector::Splat(static_cast<Number>(prepared.m_feedback1)),
                Vector::Splat(static_cast<Number>(prepared.m_feedback2))
            };
        }
        else
        {
            InstallSections<SubOrder / 2>(state.m_first, prepared.m_first);
            InstallSections<SubOrder / 2>(state.m_second, prepared.m_second);
        }
    }

    Vector Process(const Vector& input)
    {
        typename Core::Input sample{input};
        m_core.Process(sample);
        return GetOutput();
    }

    Vector GetOutput() const
    {
        return m_core.m_output.m_x;
    }

    void Reset()
    {
        m_core.Reset();
    }
};

using ButterworthFilter = ButterworthFilterGeneric<float, 1, 8>;
