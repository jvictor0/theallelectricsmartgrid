#pragma once

// Generated from DSP.MorphingTPTSVF.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const Vector v2 = (m_state.m_svf.m_a2 * v0);
        const Vector v3 = (v1 + v2);
        const Vector v4 = (m_state.m_svf.m_a2 * m_state.m_svf.m_ic1);
        const Vector v5 = (m_state.m_svf.m_ic2 + v4);
        const Vector v6 = (m_state.m_svf.m_a3 * v0);
        const Vector v7 = (v5 + v6);
        const Vector v8 = (m_state.m_svf.m_k * v3);
        const Vector v9 = (input.m_x - v8);
        const Vector v10 = (v9 - v7);
        const Vector v11 = (v3 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v12 = (v11 - m_state.m_svf.m_ic1);
        const Vector v13 = (v7 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v14 = (v13 - m_state.m_svf.m_ic2);
        const Vector v15 = (v7 + v10);
        const Vector v16 = (v7 * m_state.m_cosine);
        const Vector v17 = (v3 * m_state.m_sine);
        const Vector v18 = (v16 + v17);
        const Vector v19 = (v3 * m_state.m_cosine);
        const Vector v20 = (v10 * m_state.m_sine);
        const Vector v21 = (v19 + v20);
        const Vector v22 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)), m_state.m_blend, v21, v18);
        const Vector v23 = m_state.m_svf.m_k;
        const Vector v24 = m_state.m_svf.m_a1;
        const Vector v25 = m_state.m_svf.m_a2;
        const Vector v26 = m_state.m_svf.m_a3;
        const Vector v27 = v12;
        const Vector v28 = v14;
        const Vector v29 = v7;
        const Vector v30 = v3;
        const Vector v31 = v10;
        const Vector v32 = v15;
        const Vector v33 = m_state.m_cosine;
        const Vector v34 = m_state.m_sine;
        const Vector v35 = m_state.m_blend;
        const Vector v36 = v22;
        m_state.m_svf.m_k = v23;
        m_state.m_svf.m_a1 = v24;
        m_state.m_svf.m_a2 = v25;
        m_state.m_svf.m_a3 = v26;
        m_state.m_svf.m_ic1 = v27;
        m_state.m_svf.m_ic2 = v28;
        m_state.m_svf.m_lowPass = v29;
        m_state.m_svf.m_bandPass = v30;
        m_state.m_svf.m_highPass = v31;
        m_state.m_svf.m_notch = v32;
        m_state.m_cosine = v33;
        m_state.m_sine = v34;
        m_state.m_blend = v35;
        m_state.m_output = v36;
    }

};
