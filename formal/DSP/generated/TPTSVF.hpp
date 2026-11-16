#pragma once

// Generated from DSP.TPTSVF.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

struct TPTSVF
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
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
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_ic2);
        const double v1 = (m_state.m_a1 * m_state.m_ic1);
        const double v2 = (m_state.m_a2 * v0);
        const double v3 = (v1 + v2);
        const double v4 = (m_state.m_a2 * m_state.m_ic1);
        const double v5 = (m_state.m_ic2 + v4);
        const double v6 = (m_state.m_a3 * v0);
        const double v7 = (v5 + v6);
        const double v8 = (m_state.m_k * v3);
        const double v9 = (input.m_x - v8);
        const double v10 = (v9 - v7);
        const double v11 = (v3 * 0x10000000000000p-51);
        const double v12 = (v11 - m_state.m_ic1);
        const double v13 = (v7 * 0x10000000000000p-51);
        const double v14 = (v13 - m_state.m_ic2);
        const double v15 = (v7 + v10);
        const double v16 = m_state.m_k;
        const double v17 = m_state.m_a1;
        const double v18 = m_state.m_a2;
        const double v19 = m_state.m_a3;
        const double v20 = v12;
        const double v21 = v14;
        const double v22 = v7;
        const double v23 = v3;
        const double v24 = v10;
        const double v25 = v15;
        m_state.m_k = v16;
        m_state.m_a1 = v17;
        m_state.m_a2 = v18;
        m_state.m_a3 = v19;
        m_state.m_ic1 = v20;
        m_state.m_ic2 = v21;
        m_state.m_lowPass = v22;
        m_state.m_bandPass = v23;
        m_state.m_highPass = v24;
        m_state.m_notch = v25;
    }

};
