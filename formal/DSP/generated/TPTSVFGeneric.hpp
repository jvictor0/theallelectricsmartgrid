#pragma once

// Generated from DSP.TPTSVF.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct TPTSVFGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_k;
        Vector m_a1;
        Vector m_a2;
        Vector m_a3;
        Vector m_ic1;
        Vector m_ic2;
        Vector m_lowPass;
        Vector m_bandPass;
        Vector m_highPass;
        Vector m_notch;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_ic2);
        const Vector v1 = (m_state.m_a1 * m_state.m_ic1);
        const Vector v2 = (m_state.m_a2 * v0);
        const Vector v3 = (v1 + v2);
        const Vector v4 = (m_state.m_a2 * m_state.m_ic1);
        const Vector v5 = (m_state.m_ic2 + v4);
        const Vector v6 = (m_state.m_a3 * v0);
        const Vector v7 = (v5 + v6);
        const Vector v8 = (m_state.m_k * v3);
        const Vector v9 = (input.m_x - v8);
        const Vector v10 = (v9 - v7);
        const Vector v11 = (v3 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v12 = (v11 - m_state.m_ic1);
        const Vector v13 = (v7 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v14 = (v13 - m_state.m_ic2);
        const Vector v15 = (v7 + v10);
        const Vector v16 = m_state.m_k;
        const Vector v17 = m_state.m_a1;
        const Vector v18 = m_state.m_a2;
        const Vector v19 = m_state.m_a3;
        const Vector v20 = v12;
        const Vector v21 = v14;
        const Vector v22 = v7;
        const Vector v23 = v3;
        const Vector v24 = v10;
        const Vector v25 = v15;
        m_state.m_k = v16;
        m_state.m_a1 = v17;
        m_state.m_a2 = v18;
        m_state.m_a3 = v19;
        m_state.m_ic1 = v20;
        m_state.m_ic2 = v21;
        m_state.m_lowPass = v22;
        m_state.m_bandPass = v23;
        m_state.m_highPass = v24;
        m_state.m_notch = v25;
    }

};
