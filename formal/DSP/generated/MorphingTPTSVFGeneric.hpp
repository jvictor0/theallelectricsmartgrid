#pragma once

// Generated from DSP.MorphingTPTSVF.step. Do not edit.
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
struct MorphingTPTSVFGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
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
        } m_svf;

        Vector m_cosine;
        Vector m_sine;
        Vector m_blend;
        Vector m_output;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_svf.m_ic2);
        const Vector v1 = (m_state.m_svf.m_a1 * m_state.m_svf.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_svf.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_svf.m_a2, m_state.m_svf.m_ic1, m_state.m_svf.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_svf.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_svf.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_svf.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_svf.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (v4 * m_state.m_cosine);
        const Vector v14 = Vector::FusedMultiplyAdd(v2, m_state.m_sine, v13);
        const Vector v15 = (v2 * m_state.m_cosine);
        const Vector v16 = Vector::FusedMultiplyAdd(v7, m_state.m_sine, v15);
        const Vector v17 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)), m_state.m_blend, v16, v14);
        const Vector v18 = m_state.m_svf.m_k;
        const Vector v19 = m_state.m_svf.m_a1;
        const Vector v20 = m_state.m_svf.m_a2;
        const Vector v21 = m_state.m_svf.m_a3;
        const Vector v22 = v9;
        const Vector v23 = v11;
        const Vector v24 = v4;
        const Vector v25 = v2;
        const Vector v26 = v7;
        const Vector v27 = v12;
        const Vector v28 = m_state.m_cosine;
        const Vector v29 = m_state.m_sine;
        const Vector v30 = m_state.m_blend;
        const Vector v31 = v17;
        m_state.m_svf.m_k = v18;
        m_state.m_svf.m_a1 = v19;
        m_state.m_svf.m_a2 = v20;
        m_state.m_svf.m_a3 = v21;
        m_state.m_svf.m_ic1 = v22;
        m_state.m_svf.m_ic2 = v23;
        m_state.m_svf.m_lowPass = v24;
        m_state.m_svf.m_bandPass = v25;
        m_state.m_svf.m_highPass = v26;
        m_state.m_svf.m_notch = v27;
        m_state.m_cosine = v28;
        m_state.m_sine = v29;
        m_state.m_blend = v30;
        m_state.m_output = v31;
    }

};
