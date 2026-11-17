#pragma once

// Generated from DSP.TPTSVF4Pole.stepHighPass. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct TPTSVF4PoleHighPassGeneric
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
        const Vector v2 = (m_state.m_stage1.m_a2 * v0);
        const Vector v3 = (v1 + v2);
        const Vector v4 = (m_state.m_stage1.m_a2 * m_state.m_stage1.m_ic1);
        const Vector v5 = (m_state.m_stage1.m_ic2 + v4);
        const Vector v6 = (m_state.m_stage1.m_a3 * v0);
        const Vector v7 = (v5 + v6);
        const Vector v8 = (m_state.m_stage1.m_k * v3);
        const Vector v9 = (input.m_x - v8);
        const Vector v10 = (v9 - v7);
        const Vector v11 = (v3 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v12 = (v11 - m_state.m_stage1.m_ic1);
        const Vector v13 = (v7 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v14 = (v13 - m_state.m_stage1.m_ic2);
        const Vector v15 = (v7 + v10);
        const Vector v16 = (v10 - m_state.m_stage2.m_ic2);
        const Vector v17 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const Vector v18 = (m_state.m_stage2.m_a2 * v16);
        const Vector v19 = (v17 + v18);
        const Vector v20 = (m_state.m_stage2.m_a2 * m_state.m_stage2.m_ic1);
        const Vector v21 = (m_state.m_stage2.m_ic2 + v20);
        const Vector v22 = (m_state.m_stage2.m_a3 * v16);
        const Vector v23 = (v21 + v22);
        const Vector v24 = (m_state.m_stage2.m_k * v19);
        const Vector v25 = (v10 - v24);
        const Vector v26 = (v25 - v23);
        const Vector v27 = (v19 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v28 = (v27 - m_state.m_stage2.m_ic1);
        const Vector v29 = (v23 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v30 = (v29 - m_state.m_stage2.m_ic2);
        const Vector v31 = (v23 + v26);
        const Vector v32 = m_state.m_stage1.m_k;
        const Vector v33 = m_state.m_stage1.m_a1;
        const Vector v34 = m_state.m_stage1.m_a2;
        const Vector v35 = m_state.m_stage1.m_a3;
        const Vector v36 = v12;
        const Vector v37 = v14;
        const Vector v38 = v7;
        const Vector v39 = v3;
        const Vector v40 = v10;
        const Vector v41 = v15;
        const Vector v42 = m_state.m_stage2.m_k;
        const Vector v43 = m_state.m_stage2.m_a1;
        const Vector v44 = m_state.m_stage2.m_a2;
        const Vector v45 = m_state.m_stage2.m_a3;
        const Vector v46 = v28;
        const Vector v47 = v30;
        const Vector v48 = v23;
        const Vector v49 = v19;
        const Vector v50 = v26;
        const Vector v51 = v31;
        m_state.m_stage1.m_k = v32;
        m_state.m_stage1.m_a1 = v33;
        m_state.m_stage1.m_a2 = v34;
        m_state.m_stage1.m_a3 = v35;
        m_state.m_stage1.m_ic1 = v36;
        m_state.m_stage1.m_ic2 = v37;
        m_state.m_stage1.m_lowPass = v38;
        m_state.m_stage1.m_bandPass = v39;
        m_state.m_stage1.m_highPass = v40;
        m_state.m_stage1.m_notch = v41;
        m_state.m_stage2.m_k = v42;
        m_state.m_stage2.m_a1 = v43;
        m_state.m_stage2.m_a2 = v44;
        m_state.m_stage2.m_a3 = v45;
        m_state.m_stage2.m_ic1 = v46;
        m_state.m_stage2.m_ic2 = v47;
        m_state.m_stage2.m_lowPass = v48;
        m_state.m_stage2.m_bandPass = v49;
        m_state.m_stage2.m_highPass = v50;
        m_state.m_stage2.m_notch = v51;
    }

};
