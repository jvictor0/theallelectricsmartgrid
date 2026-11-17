#pragma once

// Generated from DSP.TPTLadder.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const Vector v1 = (v0 * m_state.m_stage1.m_G);
        const Vector v2 = (v1 + m_state.m_stage1.m_s);
        const Vector v3 = (v2 - m_state.m_stage2.m_s);
        const Vector v4 = (v3 * m_state.m_stage2.m_G);
        const Vector v5 = (v4 + m_state.m_stage2.m_s);
        const Vector v6 = (v5 - m_state.m_stage3.m_s);
        const Vector v7 = (v6 * m_state.m_stage3.m_G);
        const Vector v8 = (v7 + m_state.m_stage3.m_s);
        const Vector v9 = (v8 - m_state.m_stage4.m_s);
        const Vector v10 = (v9 * m_state.m_stage4.m_G);
        const Vector v11 = (v10 + m_state.m_stage4.m_s);
        const Vector v12 = (m_state.m_stage1.m_G * m_state.m_stage2.m_G);
        const Vector v13 = (v12 * m_state.m_stage3.m_G);
        const Vector v14 = (v13 * m_state.m_stage4.m_G);
        const Vector v15 = (m_state.m_k * v11);
        const Vector v16 = (input.m_x - v15);
        const Vector v17 = (m_state.m_k * v14);
        const Vector v18 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v17);
        const Vector v19 = (v16 / v18);
        const Vector v20 = (v19 - m_state.m_stage1.m_s);
        const Vector v21 = (v20 * m_state.m_stage1.m_G);
        const Vector v22 = (v21 + m_state.m_stage1.m_s);
        const Vector v23 = (v22 + v21);
        const Vector v24 = (v22 - m_state.m_stage2.m_s);
        const Vector v25 = (v24 * m_state.m_stage2.m_G);
        const Vector v26 = (v25 + m_state.m_stage2.m_s);
        const Vector v27 = (v26 + v25);
        const Vector v28 = (v26 - m_state.m_stage3.m_s);
        const Vector v29 = (v28 * m_state.m_stage3.m_G);
        const Vector v30 = (v29 + m_state.m_stage3.m_s);
        const Vector v31 = (v30 + v29);
        const Vector v32 = (v30 - m_state.m_stage4.m_s);
        const Vector v33 = (v32 * m_state.m_stage4.m_G);
        const Vector v34 = (v33 + m_state.m_stage4.m_s);
        const Vector v35 = (v34 + v33);
        const Vector v36 = m_state.m_k;
        const Vector v37 = v19;
        const Vector v38 = m_state.m_stage1.m_G;
        const Vector v39 = v23;
        const Vector v40 = v22;
        const Vector v41 = m_state.m_stage2.m_G;
        const Vector v42 = v27;
        const Vector v43 = v26;
        const Vector v44 = m_state.m_stage3.m_G;
        const Vector v45 = v31;
        const Vector v46 = v30;
        const Vector v47 = m_state.m_stage4.m_G;
        const Vector v48 = v35;
        const Vector v49 = v34;
        m_state.m_k = v36;
        m_state.m_drive = v37;
        m_state.m_stage1.m_G = v38;
        m_state.m_stage1.m_s = v39;
        m_state.m_stage1.m_y = v40;
        m_state.m_stage2.m_G = v41;
        m_state.m_stage2.m_s = v42;
        m_state.m_stage2.m_y = v43;
        m_state.m_stage3.m_G = v44;
        m_state.m_stage3.m_s = v45;
        m_state.m_stage3.m_y = v46;
        m_state.m_stage4.m_G = v47;
        m_state.m_stage4.m_s = v48;
        m_state.m_stage4.m_y = v49;
    }

};
