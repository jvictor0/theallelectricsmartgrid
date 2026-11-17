#pragma once

// Generated from DSP.TPTLadder.step. Do not edit.
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
struct TPTLadderGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_k;
        Vector m_drive;
        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_stage1;

        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_stage2;

        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_stage3;

        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_stage4;

    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) - m_state.m_stage1.m_s);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, m_state.m_stage1.m_G, m_state.m_stage1.m_s);
        const Vector v2 = (v1 - m_state.m_stage2.m_s);
        const Vector v3 = Vector::FusedMultiplyAdd(v2, m_state.m_stage2.m_G, m_state.m_stage2.m_s);
        const Vector v4 = (v3 - m_state.m_stage3.m_s);
        const Vector v5 = Vector::FusedMultiplyAdd(v4, m_state.m_stage3.m_G, m_state.m_stage3.m_s);
        const Vector v6 = (v5 - m_state.m_stage4.m_s);
        const Vector v7 = Vector::FusedMultiplyAdd(v6, m_state.m_stage4.m_G, m_state.m_stage4.m_s);
        const Vector v8 = (m_state.m_stage1.m_G * m_state.m_stage2.m_G);
        const Vector v9 = (v8 * m_state.m_stage3.m_G);
        const Vector v10 = (v9 * m_state.m_stage4.m_G);
        const Vector v11 = (-m_state.m_k);
        const Vector v12 = Vector::FusedMultiplyAdd(v11, v7, input.m_x);
        const Vector v13 = Vector::FusedMultiplyAdd(m_state.m_k, v10, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v14 = (v12 / v13);
        const Vector v15 = (v14 - m_state.m_stage1.m_s);
        const Vector v16 = Vector::FusedMultiplyAdd(v15, m_state.m_stage1.m_G, m_state.m_stage1.m_s);
        const Vector v17 = Vector::FusedMultiplyAdd(v15, m_state.m_stage1.m_G, v16);
        const Vector v18 = (v16 - m_state.m_stage2.m_s);
        const Vector v19 = Vector::FusedMultiplyAdd(v18, m_state.m_stage2.m_G, m_state.m_stage2.m_s);
        const Vector v20 = Vector::FusedMultiplyAdd(v18, m_state.m_stage2.m_G, v19);
        const Vector v21 = (v19 - m_state.m_stage3.m_s);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, m_state.m_stage3.m_G, m_state.m_stage3.m_s);
        const Vector v23 = Vector::FusedMultiplyAdd(v21, m_state.m_stage3.m_G, v22);
        const Vector v24 = (v22 - m_state.m_stage4.m_s);
        const Vector v25 = Vector::FusedMultiplyAdd(v24, m_state.m_stage4.m_G, m_state.m_stage4.m_s);
        const Vector v26 = Vector::FusedMultiplyAdd(v24, m_state.m_stage4.m_G, v25);
        const Vector v27 = m_state.m_k;
        const Vector v28 = v14;
        const Vector v29 = m_state.m_stage1.m_G;
        const Vector v30 = v17;
        const Vector v31 = v16;
        const Vector v32 = m_state.m_stage2.m_G;
        const Vector v33 = v20;
        const Vector v34 = v19;
        const Vector v35 = m_state.m_stage3.m_G;
        const Vector v36 = v23;
        const Vector v37 = v22;
        const Vector v38 = m_state.m_stage4.m_G;
        const Vector v39 = v26;
        const Vector v40 = v25;
        m_state.m_k = v27;
        m_state.m_drive = v28;
        m_state.m_stage1.m_G = v29;
        m_state.m_stage1.m_s = v30;
        m_state.m_stage1.m_y = v31;
        m_state.m_stage2.m_G = v32;
        m_state.m_stage2.m_s = v33;
        m_state.m_stage2.m_y = v34;
        m_state.m_stage3.m_G = v35;
        m_state.m_stage3.m_s = v36;
        m_state.m_stage3.m_y = v37;
        m_state.m_stage4.m_G = v38;
        m_state.m_stage4.m_s = v39;
        m_state.m_stage4.m_y = v40;
    }

};
