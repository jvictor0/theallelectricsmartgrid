#pragma once

// Generated from DSP.TPTSVF4Pole.stepLowPassFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

struct TPTSVF4PoleLowPass
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
        const double v2 = (m_state.m_stage1.m_a2 * v0);
        const double v3 = (v1 + v2);
        const double v4 = (m_state.m_stage1.m_a2 * m_state.m_stage1.m_ic1);
        const double v5 = (m_state.m_stage1.m_ic2 + v4);
        const double v6 = (m_state.m_stage1.m_a3 * v0);
        const double v7 = (v5 + v6);
        const double v8 = (m_state.m_stage1.m_k * v3);
        const double v9 = (input.m_x - v8);
        const double v10 = (v9 - v7);
        const double v11 = (v3 * 0x10000000000000p-51);
        const double v12 = (v11 - m_state.m_stage1.m_ic1);
        const double v13 = (v7 * 0x10000000000000p-51);
        const double v14 = (v13 - m_state.m_stage1.m_ic2);
        const double v15 = (v7 + v10);
        const double v16 = (v7 - m_state.m_stage2.m_ic2);
        const double v17 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const double v18 = (m_state.m_stage2.m_a2 * v16);
        const double v19 = (v17 + v18);
        const double v20 = (m_state.m_stage2.m_a2 * m_state.m_stage2.m_ic1);
        const double v21 = (m_state.m_stage2.m_ic2 + v20);
        const double v22 = (m_state.m_stage2.m_a3 * v16);
        const double v23 = (v21 + v22);
        const double v24 = (m_state.m_stage2.m_k * v19);
        const double v25 = (v7 - v24);
        const double v26 = (v25 - v23);
        const double v27 = (v19 * 0x10000000000000p-51);
        const double v28 = (v27 - m_state.m_stage2.m_ic1);
        const double v29 = (v23 * 0x10000000000000p-51);
        const double v30 = (v29 - m_state.m_stage2.m_ic2);
        const double v31 = (v23 + v26);
        const double v32 = m_state.m_stage1.m_k;
        const double v33 = m_state.m_stage1.m_a1;
        const double v34 = m_state.m_stage1.m_a2;
        const double v35 = m_state.m_stage1.m_a3;
        const double v36 = v12;
        const double v37 = v14;
        const double v38 = v7;
        const double v39 = v3;
        const double v40 = v10;
        const double v41 = v15;
        const double v42 = m_state.m_stage2.m_k;
        const double v43 = m_state.m_stage2.m_a1;
        const double v44 = m_state.m_stage2.m_a2;
        const double v45 = m_state.m_stage2.m_a3;
        const double v46 = v28;
        const double v47 = v30;
        const double v48 = v23;
        const double v49 = v19;
        const double v50 = v26;
        const double v51 = v31;
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
