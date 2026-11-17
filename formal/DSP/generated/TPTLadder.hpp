#pragma once

// Generated from DSP.TPTLadder.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct TPTLadder
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        double m_k;
        double m_drive;
        struct
        {
            double m_G;
            double m_s;
            double m_y;
        } m_stage1;

        struct
        {
            double m_G;
            double m_s;
            double m_y;
        } m_stage2;

        struct
        {
            double m_G;
            double m_s;
            double m_y;
        } m_stage3;

        struct
        {
            double m_G;
            double m_s;
            double m_y;
        } m_stage4;

    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (0.0 - m_state.m_stage1.m_s);
        const double v1 = std::fma(v0, m_state.m_stage1.m_G, m_state.m_stage1.m_s);
        const double v2 = (v1 - m_state.m_stage2.m_s);
        const double v3 = std::fma(v2, m_state.m_stage2.m_G, m_state.m_stage2.m_s);
        const double v4 = (v3 - m_state.m_stage3.m_s);
        const double v5 = std::fma(v4, m_state.m_stage3.m_G, m_state.m_stage3.m_s);
        const double v6 = (v5 - m_state.m_stage4.m_s);
        const double v7 = std::fma(v6, m_state.m_stage4.m_G, m_state.m_stage4.m_s);
        const double v8 = (m_state.m_stage1.m_G * m_state.m_stage2.m_G);
        const double v9 = (v8 * m_state.m_stage3.m_G);
        const double v10 = (v9 * m_state.m_stage4.m_G);
        const double v11 = (-m_state.m_k);
        const double v12 = std::fma(v11, v7, input.m_x);
        const double v13 = std::fma(m_state.m_k, v10, 1.0);
        const double v14 = (v12 / v13);
        const double v15 = (v14 - m_state.m_stage1.m_s);
        const double v16 = std::fma(v15, m_state.m_stage1.m_G, m_state.m_stage1.m_s);
        const double v17 = std::fma(v15, m_state.m_stage1.m_G, v16);
        const double v18 = (v16 - m_state.m_stage2.m_s);
        const double v19 = std::fma(v18, m_state.m_stage2.m_G, m_state.m_stage2.m_s);
        const double v20 = std::fma(v18, m_state.m_stage2.m_G, v19);
        const double v21 = (v19 - m_state.m_stage3.m_s);
        const double v22 = std::fma(v21, m_state.m_stage3.m_G, m_state.m_stage3.m_s);
        const double v23 = std::fma(v21, m_state.m_stage3.m_G, v22);
        const double v24 = (v22 - m_state.m_stage4.m_s);
        const double v25 = std::fma(v24, m_state.m_stage4.m_G, m_state.m_stage4.m_s);
        const double v26 = std::fma(v24, m_state.m_stage4.m_G, v25);
        const double v27 = m_state.m_k;
        const double v28 = v14;
        const double v29 = m_state.m_stage1.m_G;
        const double v30 = v17;
        const double v31 = v16;
        const double v32 = m_state.m_stage2.m_G;
        const double v33 = v20;
        const double v34 = v19;
        const double v35 = m_state.m_stage3.m_G;
        const double v36 = v23;
        const double v37 = v22;
        const double v38 = m_state.m_stage4.m_G;
        const double v39 = v26;
        const double v40 = v25;
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
