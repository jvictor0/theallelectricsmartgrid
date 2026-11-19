#pragma once

// Generated from DSP.DelayKernels.Runtime.Comb.process. Do not edit.
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
struct TunedDecayCombCoreGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            Vector m_delay;
            Vector m_a;
            Vector m_b;
            Vector m_r;
            Vector m_delayGain;
            Vector m_g;
            Vector m_k;
            Vector m_h;
            Vector m_gain;
            Vector m_sign;
            Vector m_bypass;
        } m_coefficients;

        struct
        {
            Vector m_allpass;
            Vector m_ic1;
            Vector m_ic2;
        } m_memory;

    };

    struct Input
    {
        Vector m_x;
        Vector m_delayed;
    };

    struct Output
    {
        Vector m_y;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (m_state.m_coefficients.m_r * m_state.m_memory.m_allpass);
        const Vector v1 = (input.m_delayed * m_state.m_coefficients.m_delayGain);
        const Vector v2 = (m_state.m_coefficients.m_a * v1);
        const Vector v3 = (m_state.m_coefficients.m_b * v0);
        const Vector v4 = (v2 + v3);
        const Vector v5 = (input.m_delayed * m_state.m_coefficients.m_delayGain);
        const Vector v6 = (m_state.m_coefficients.m_b * v5);
        const Vector v7 = (m_state.m_coefficients.m_a * v0);
        const Vector v8 = (v6 - v7);
        m_state.m_memory.m_allpass = v8;
        const Vector v9 = (m_state.m_coefficients.m_sign * v4);
        const Vector v10 = (input.m_x + v9);
        const Vector v11 = (m_state.m_coefficients.m_r * m_state.m_memory.m_ic1);
        const Vector v12 = (m_state.m_coefficients.m_r * m_state.m_memory.m_ic2);
        const Vector v13 = (v10 - v12);
        const Vector v14 = (m_state.m_coefficients.m_g * v13);
        const Vector v15 = (v11 + v14);
        const Vector v16 = (m_state.m_coefficients.m_h * v15);
        const Vector v17 = (m_state.m_coefficients.m_g * v16);
        const Vector v18 = (v12 + v17);
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v20 = (v16 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v21 = (v20 - v11);
        const Vector v22 = Vector::IfLess(m_state.m_coefficients.m_bypass, v19, v21, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        m_state.m_memory.m_ic1 = v22;
        const Vector v23 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v24 = (v18 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v25 = (v24 - v12);
        const Vector v26 = Vector::IfLess(m_state.m_coefficients.m_bypass, v23, v25, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        m_state.m_memory.m_ic2 = v26;
        const Vector v27 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v28 = (m_state.m_coefficients.m_gain * v18);
        const Vector v29 = Vector::IfLess(m_state.m_coefficients.m_bypass, v27, v28, v10);
        m_output.m_y = v29;
    }

};
