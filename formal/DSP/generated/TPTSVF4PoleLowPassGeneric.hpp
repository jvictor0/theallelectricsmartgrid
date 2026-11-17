#pragma once

// Generated from DSP.TPTSVF4Pole.stepLowPass. Do not edit.
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
struct TPTSVF4PoleLowPassGeneric
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
        } m_stage1;

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
        } m_stage2;

    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_stage1.m_ic2);
        const Vector v1 = (m_state.m_stage1.m_a1 * m_state.m_stage1.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_stage1.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_stage1.m_a2, m_state.m_stage1.m_ic1, m_state.m_stage1.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_stage1.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_stage1.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_stage1.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_stage1.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (v4 - m_state.m_stage2.m_ic2);
        const Vector v14 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const Vector v15 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, v13, v14);
        const Vector v16 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, m_state.m_stage2.m_ic1, m_state.m_stage2.m_ic2);
        const Vector v17 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a3, v13, v16);
        const Vector v18 = (-m_state.m_stage2.m_k);
        const Vector v19 = Vector::FusedMultiplyAdd(v18, v15, v4);
        const Vector v20 = (v19 - v17);
        const Vector v21 = (-m_state.m_stage2.m_ic1);
        const Vector v22 = Vector::FusedMultiplyAdd(v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v21);
        const Vector v23 = (-m_state.m_stage2.m_ic2);
        const Vector v24 = Vector::FusedMultiplyAdd(v17, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v23);
        const Vector v25 = (v17 + v20);
        const Vector v26 = m_state.m_stage1.m_k;
        const Vector v27 = m_state.m_stage1.m_a1;
        const Vector v28 = m_state.m_stage1.m_a2;
        const Vector v29 = m_state.m_stage1.m_a3;
        const Vector v30 = v9;
        const Vector v31 = v11;
        const Vector v32 = v4;
        const Vector v33 = v2;
        const Vector v34 = v7;
        const Vector v35 = v12;
        const Vector v36 = m_state.m_stage2.m_k;
        const Vector v37 = m_state.m_stage2.m_a1;
        const Vector v38 = m_state.m_stage2.m_a2;
        const Vector v39 = m_state.m_stage2.m_a3;
        const Vector v40 = v22;
        const Vector v41 = v24;
        const Vector v42 = v17;
        const Vector v43 = v15;
        const Vector v44 = v20;
        const Vector v45 = v25;
        m_state.m_stage1.m_k = v26;
        m_state.m_stage1.m_a1 = v27;
        m_state.m_stage1.m_a2 = v28;
        m_state.m_stage1.m_a3 = v29;
        m_state.m_stage1.m_ic1 = v30;
        m_state.m_stage1.m_ic2 = v31;
        m_state.m_stage1.m_lowPass = v32;
        m_state.m_stage1.m_bandPass = v33;
        m_state.m_stage1.m_highPass = v34;
        m_state.m_stage1.m_notch = v35;
        m_state.m_stage2.m_k = v36;
        m_state.m_stage2.m_a1 = v37;
        m_state.m_stage2.m_a2 = v38;
        m_state.m_stage2.m_a3 = v39;
        m_state.m_stage2.m_ic1 = v40;
        m_state.m_stage2.m_ic2 = v41;
        m_state.m_stage2.m_lowPass = v42;
        m_state.m_stage2.m_bandPass = v43;
        m_state.m_stage2.m_highPass = v44;
        m_state.m_stage2.m_notch = v45;
    }

};
