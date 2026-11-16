#pragma once

// Generated from DSP.TPTLadder.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const double v1 = (v0 * m_state.m_stage1.m_G);
        const double v2 = (v1 + m_state.m_stage1.m_s);
        const double v3 = (v2 - m_state.m_stage2.m_s);
        const double v4 = (v3 * m_state.m_stage2.m_G);
        const double v5 = (v4 + m_state.m_stage2.m_s);
        const double v6 = (v5 - m_state.m_stage3.m_s);
        const double v7 = (v6 * m_state.m_stage3.m_G);
        const double v8 = (v7 + m_state.m_stage3.m_s);
        const double v9 = (v8 - m_state.m_stage4.m_s);
        const double v10 = (v9 * m_state.m_stage4.m_G);
        const double v11 = (v10 + m_state.m_stage4.m_s);
        const double v12 = (m_state.m_stage1.m_G * m_state.m_stage2.m_G);
        const double v13 = (v12 * m_state.m_stage3.m_G);
        const double v14 = (v13 * m_state.m_stage4.m_G);
        const double v15 = (m_state.m_k * v11);
        const double v16 = (input.m_x - v15);
        const double v17 = (m_state.m_k * v14);
        const double v18 = (1.0 + v17);
        const double v19 = (v16 / v18);
        const double v20 = (v19 - m_state.m_stage1.m_s);
        const double v21 = (v20 * m_state.m_stage1.m_G);
        const double v22 = (v21 + m_state.m_stage1.m_s);
        const double v23 = (v22 + v21);
        const double v24 = (v22 - m_state.m_stage2.m_s);
        const double v25 = (v24 * m_state.m_stage2.m_G);
        const double v26 = (v25 + m_state.m_stage2.m_s);
        const double v27 = (v26 + v25);
        const double v28 = (v26 - m_state.m_stage3.m_s);
        const double v29 = (v28 * m_state.m_stage3.m_G);
        const double v30 = (v29 + m_state.m_stage3.m_s);
        const double v31 = (v30 + v29);
        const double v32 = (v30 - m_state.m_stage4.m_s);
        const double v33 = (v32 * m_state.m_stage4.m_G);
        const double v34 = (v33 + m_state.m_stage4.m_s);
        const double v35 = (v34 + v33);
        const double v36 = m_state.m_k;
        const double v37 = v19;
        const double v38 = m_state.m_stage1.m_G;
        const double v39 = v23;
        const double v40 = v22;
        const double v41 = m_state.m_stage2.m_G;
        const double v42 = v27;
        const double v43 = v26;
        const double v44 = m_state.m_stage3.m_G;
        const double v45 = v31;
        const double v46 = v30;
        const double v47 = m_state.m_stage4.m_G;
        const double v48 = v35;
        const double v49 = v34;
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
