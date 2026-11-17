#pragma once

// Generated from DSP.TPTSVF.step. Do not edit.
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
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_a2, m_state.m_ic1, m_state.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = m_state.m_k;
        const Vector v14 = m_state.m_a1;
        const Vector v15 = m_state.m_a2;
        const Vector v16 = m_state.m_a3;
        const Vector v17 = v9;
        const Vector v18 = v11;
        const Vector v19 = v4;
        const Vector v20 = v2;
        const Vector v21 = v7;
        const Vector v22 = v12;
        m_state.m_k = v13;
        m_state.m_a1 = v14;
        m_state.m_a2 = v15;
        m_state.m_a3 = v16;
        m_state.m_ic1 = v17;
        m_state.m_ic2 = v18;
        m_state.m_lowPass = v19;
        m_state.m_bandPass = v20;
        m_state.m_highPass = v21;
        m_state.m_notch = v22;
    }

};
