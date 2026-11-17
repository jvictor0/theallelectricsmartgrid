#pragma once

// Generated from DSP.MorphingTPTSVF.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

struct MorphingTPTSVF
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
        } m_svf;

        double m_cosine;
        double m_sine;
        double m_blend;
        double m_output;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_svf.m_ic2);
        const double v1 = (m_state.m_svf.m_a1 * m_state.m_svf.m_ic1);
        const double v2 = (m_state.m_svf.m_a2 * v0);
        const double v3 = (v1 + v2);
        const double v4 = (m_state.m_svf.m_a2 * m_state.m_svf.m_ic1);
        const double v5 = (m_state.m_svf.m_ic2 + v4);
        const double v6 = (m_state.m_svf.m_a3 * v0);
        const double v7 = (v5 + v6);
        const double v8 = (m_state.m_svf.m_k * v3);
        const double v9 = (input.m_x - v8);
        const double v10 = (v9 - v7);
        const double v11 = (v3 * 0x10000000000000p-51);
        const double v12 = (v11 - m_state.m_svf.m_ic1);
        const double v13 = (v7 * 0x10000000000000p-51);
        const double v14 = (v13 - m_state.m_svf.m_ic2);
        const double v15 = (v7 + v10);
        const double v16 = (v7 * m_state.m_cosine);
        const double v17 = (v3 * m_state.m_sine);
        const double v18 = (v16 + v17);
        const double v19 = (v3 * m_state.m_cosine);
        const double v20 = (v10 * m_state.m_sine);
        const double v21 = (v19 + v20);
        const bool v22 = (0x10000000000000p-53 < m_state.m_blend);
        double v23;
        if (v22)
        {
            v23 = v21;
        }
        else
        {
            v23 = v18;
        }

        const double v24 = m_state.m_svf.m_k;
        const double v25 = m_state.m_svf.m_a1;
        const double v26 = m_state.m_svf.m_a2;
        const double v27 = m_state.m_svf.m_a3;
        const double v28 = v12;
        const double v29 = v14;
        const double v30 = v7;
        const double v31 = v3;
        const double v32 = v10;
        const double v33 = v15;
        const double v34 = m_state.m_cosine;
        const double v35 = m_state.m_sine;
        const double v36 = m_state.m_blend;
        const double v37 = v23;
        m_state.m_svf.m_k = v24;
        m_state.m_svf.m_a1 = v25;
        m_state.m_svf.m_a2 = v26;
        m_state.m_svf.m_a3 = v27;
        m_state.m_svf.m_ic1 = v28;
        m_state.m_svf.m_ic2 = v29;
        m_state.m_svf.m_lowPass = v30;
        m_state.m_svf.m_bandPass = v31;
        m_state.m_svf.m_highPass = v32;
        m_state.m_svf.m_notch = v33;
        m_state.m_cosine = v34;
        m_state.m_sine = v35;
        m_state.m_blend = v36;
        m_state.m_output = v37;
    }

};
