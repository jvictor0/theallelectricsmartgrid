#pragma once

// Generated from DSP.ButterworthDownsampler.process2. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct ButterworthDownsampler2Generic
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            Vector m_gain;
            Vector m_feedback1;
            Vector m_feedback2;
        } m_coefficients;

        Vector m_delay1;
        Vector m_delay2;
    };

    struct Input
    {
        Vector m_x;
    };

    struct Output
    {
        Vector m_x;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (m_state.m_coefficients.m_gain * input.m_x);
        const Vector v1 = (v0 + m_state.m_delay1);
        const Vector v2 = (v0 + v0);
        const Vector v3 = (v2 + m_state.m_delay2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_coefficients.m_feedback1, v1, v3);
        m_state.m_delay1 = v4;
        const Vector v5 = Vector::FusedMultiplyAdd(m_state.m_coefficients.m_feedback2, v1, v0);
        m_state.m_delay2 = v5;
        m_output.m_x = v1;
    }

    // Reset generated from DSP.ButterworthDownsampler.reset2.
    //

    void Reset()
    {
        const Vector v0 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v1 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v2 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        m_state.m_delay1 = v1;
        m_state.m_delay2 = v2;
        m_output.m_x = v0;
    }

};
