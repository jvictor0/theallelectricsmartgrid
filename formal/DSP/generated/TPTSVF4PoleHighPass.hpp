#pragma once

// Generated from DSP.TPTSVF4Pole.stepHighPassFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct TPTSVF4PoleHighPass
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            double m_k;
            double m_a1;
            double m_a2;
            double m_a3;
            double m_ic1;
            double m_ic2;
            double m_lowPass;
            double m_bandPass;
            double m_highPass;
            double m_notch;
        } m_stage1;

        struct
        {
            double m_k;
            double m_a1;
            double m_a2;
            double m_a3;
            double m_ic1;
            double m_ic2;
            double m_lowPass;
            double m_bandPass;
            double m_highPass;
            double m_notch;
        } m_stage2;

    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_stage1.m_ic2);
        const double v1 = (m_state.m_stage1.m_a1 * m_state.m_stage1.m_ic1);
        const double v2 = std::fma(m_state.m_stage1.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_stage1.m_a2, m_state.m_stage1.m_ic1, m_state.m_stage1.m_ic2);
        const double v4 = std::fma(m_state.m_stage1.m_a3, v0, v3);
        const double v5 = (-m_state.m_stage1.m_k);
        const double v6 = std::fma(v5, v2, input.m_x);
        const double v7 = (v6 - v4);
        const double v8 = (-m_state.m_stage1.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        const double v10 = (-m_state.m_stage1.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        const double v12 = (v4 + v7);
        const double v13 = (v7 - m_state.m_stage2.m_ic2);
        const double v14 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const double v15 = std::fma(m_state.m_stage2.m_a2, v13, v14);
        const double v16 = std::fma(m_state.m_stage2.m_a2, m_state.m_stage2.m_ic1, m_state.m_stage2.m_ic2);
        const double v17 = std::fma(m_state.m_stage2.m_a3, v13, v16);
        const double v18 = (-m_state.m_stage2.m_k);
        const double v19 = std::fma(v18, v15, v7);
        const double v20 = (v19 - v17);
        const double v21 = (-m_state.m_stage2.m_ic1);
        const double v22 = std::fma(v15, 0x10000000000000p-51, v21);
        const double v23 = (-m_state.m_stage2.m_ic2);
        const double v24 = std::fma(v17, 0x10000000000000p-51, v23);
        const double v25 = (v17 + v20);
        const double v26 = m_state.m_stage1.m_k;
        const double v27 = m_state.m_stage1.m_a1;
        const double v28 = m_state.m_stage1.m_a2;
        const double v29 = m_state.m_stage1.m_a3;
        const double v30 = v9;
        const double v31 = v11;
        const double v32 = v4;
        const double v33 = v2;
        const double v34 = v7;
        const double v35 = v12;
        const double v36 = m_state.m_stage2.m_k;
        const double v37 = m_state.m_stage2.m_a1;
        const double v38 = m_state.m_stage2.m_a2;
        const double v39 = m_state.m_stage2.m_a3;
        const double v40 = v22;
        const double v41 = v24;
        const double v42 = v17;
        const double v43 = v15;
        const double v44 = v20;
        const double v45 = v25;
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
