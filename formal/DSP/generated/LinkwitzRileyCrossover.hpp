#pragma once

// Generated from DSP.LinkwitzRileyCrossover.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct LinkwitzRileyCrossover
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
        } m_first;

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
        } m_lowSecond;

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
        } m_highSecond;

    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_first.m_ic2);
        const double v1 = (m_state.m_first.m_a1 * m_state.m_first.m_ic1);
        const double v2 = std::fma(m_state.m_first.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_first.m_a2, m_state.m_first.m_ic1, m_state.m_first.m_ic2);
        const double v4 = std::fma(m_state.m_first.m_a3, v0, v3);
        const double v5 = (-m_state.m_first.m_k);
        const double v6 = std::fma(v5, v2, input.m_x);
        const double v7 = (v6 - v4);
        const double v8 = (-m_state.m_first.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        const double v10 = (-m_state.m_first.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        const double v12 = (v4 + v7);
        const double v13 = (v4 - m_state.m_lowSecond.m_ic2);
        const double v14 = (m_state.m_lowSecond.m_a1 * m_state.m_lowSecond.m_ic1);
        const double v15 = std::fma(m_state.m_lowSecond.m_a2, v13, v14);
        const double v16 = std::fma(m_state.m_lowSecond.m_a2, m_state.m_lowSecond.m_ic1, m_state.m_lowSecond.m_ic2);
        const double v17 = std::fma(m_state.m_lowSecond.m_a3, v13, v16);
        const double v18 = (-m_state.m_lowSecond.m_k);
        const double v19 = std::fma(v18, v15, v4);
        const double v20 = (v19 - v17);
        const double v21 = (-m_state.m_lowSecond.m_ic1);
        const double v22 = std::fma(v15, 0x10000000000000p-51, v21);
        const double v23 = (-m_state.m_lowSecond.m_ic2);
        const double v24 = std::fma(v17, 0x10000000000000p-51, v23);
        const double v25 = (v17 + v20);
        const double v26 = (v7 - m_state.m_highSecond.m_ic2);
        const double v27 = (m_state.m_highSecond.m_a1 * m_state.m_highSecond.m_ic1);
        const double v28 = std::fma(m_state.m_highSecond.m_a2, v26, v27);
        const double v29 = std::fma(m_state.m_highSecond.m_a2, m_state.m_highSecond.m_ic1, m_state.m_highSecond.m_ic2);
        const double v30 = std::fma(m_state.m_highSecond.m_a3, v26, v29);
        const double v31 = (-m_state.m_highSecond.m_k);
        const double v32 = std::fma(v31, v28, v7);
        const double v33 = (v32 - v30);
        const double v34 = (-m_state.m_highSecond.m_ic1);
        const double v35 = std::fma(v28, 0x10000000000000p-51, v34);
        const double v36 = (-m_state.m_highSecond.m_ic2);
        const double v37 = std::fma(v30, 0x10000000000000p-51, v36);
        const double v38 = (v30 + v33);
        const double v39 = m_state.m_first.m_k;
        const double v40 = m_state.m_first.m_a1;
        const double v41 = m_state.m_first.m_a2;
        const double v42 = m_state.m_first.m_a3;
        const double v43 = v9;
        const double v44 = v11;
        const double v45 = v4;
        const double v46 = v2;
        const double v47 = v7;
        const double v48 = v12;
        const double v49 = m_state.m_lowSecond.m_k;
        const double v50 = m_state.m_lowSecond.m_a1;
        const double v51 = m_state.m_lowSecond.m_a2;
        const double v52 = m_state.m_lowSecond.m_a3;
        const double v53 = v22;
        const double v54 = v24;
        const double v55 = v17;
        const double v56 = v15;
        const double v57 = v20;
        const double v58 = v25;
        const double v59 = m_state.m_highSecond.m_k;
        const double v60 = m_state.m_highSecond.m_a1;
        const double v61 = m_state.m_highSecond.m_a2;
        const double v62 = m_state.m_highSecond.m_a3;
        const double v63 = v35;
        const double v64 = v37;
        const double v65 = v30;
        const double v66 = v28;
        const double v67 = v33;
        const double v68 = v38;
        m_state.m_first.m_k = v39;
        m_state.m_first.m_a1 = v40;
        m_state.m_first.m_a2 = v41;
        m_state.m_first.m_a3 = v42;
        m_state.m_first.m_ic1 = v43;
        m_state.m_first.m_ic2 = v44;
        m_state.m_first.m_lowPass = v45;
        m_state.m_first.m_bandPass = v46;
        m_state.m_first.m_highPass = v47;
        m_state.m_first.m_notch = v48;
        m_state.m_lowSecond.m_k = v49;
        m_state.m_lowSecond.m_a1 = v50;
        m_state.m_lowSecond.m_a2 = v51;
        m_state.m_lowSecond.m_a3 = v52;
        m_state.m_lowSecond.m_ic1 = v53;
        m_state.m_lowSecond.m_ic2 = v54;
        m_state.m_lowSecond.m_lowPass = v55;
        m_state.m_lowSecond.m_bandPass = v56;
        m_state.m_lowSecond.m_highPass = v57;
        m_state.m_lowSecond.m_notch = v58;
        m_state.m_highSecond.m_k = v59;
        m_state.m_highSecond.m_a1 = v60;
        m_state.m_highSecond.m_a2 = v61;
        m_state.m_highSecond.m_a3 = v62;
        m_state.m_highSecond.m_ic1 = v63;
        m_state.m_highSecond.m_ic2 = v64;
        m_state.m_highSecond.m_lowPass = v65;
        m_state.m_highSecond.m_bandPass = v66;
        m_state.m_highSecond.m_highPass = v67;
        m_state.m_highSecond.m_notch = v68;
    }

};
