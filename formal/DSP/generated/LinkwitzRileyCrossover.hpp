#pragma once

// Generated from DSP.LinkwitzRileyCrossover.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const double v2 = (m_state.m_first.m_a2 * v0);
        const double v3 = (v1 + v2);
        const double v4 = (m_state.m_first.m_a2 * m_state.m_first.m_ic1);
        const double v5 = (m_state.m_first.m_ic2 + v4);
        const double v6 = (m_state.m_first.m_a3 * v0);
        const double v7 = (v5 + v6);
        const double v8 = (m_state.m_first.m_k * v3);
        const double v9 = (input.m_x - v8);
        const double v10 = (v9 - v7);
        const double v11 = (v3 * 0x10000000000000p-51);
        const double v12 = (v11 - m_state.m_first.m_ic1);
        const double v13 = (v7 * 0x10000000000000p-51);
        const double v14 = (v13 - m_state.m_first.m_ic2);
        const double v15 = (v7 + v10);
        const double v16 = (v7 - m_state.m_lowSecond.m_ic2);
        const double v17 = (m_state.m_lowSecond.m_a1 * m_state.m_lowSecond.m_ic1);
        const double v18 = (m_state.m_lowSecond.m_a2 * v16);
        const double v19 = (v17 + v18);
        const double v20 = (m_state.m_lowSecond.m_a2 * m_state.m_lowSecond.m_ic1);
        const double v21 = (m_state.m_lowSecond.m_ic2 + v20);
        const double v22 = (m_state.m_lowSecond.m_a3 * v16);
        const double v23 = (v21 + v22);
        const double v24 = (m_state.m_lowSecond.m_k * v19);
        const double v25 = (v7 - v24);
        const double v26 = (v25 - v23);
        const double v27 = (v19 * 0x10000000000000p-51);
        const double v28 = (v27 - m_state.m_lowSecond.m_ic1);
        const double v29 = (v23 * 0x10000000000000p-51);
        const double v30 = (v29 - m_state.m_lowSecond.m_ic2);
        const double v31 = (v23 + v26);
        const double v32 = (v10 - m_state.m_highSecond.m_ic2);
        const double v33 = (m_state.m_highSecond.m_a1 * m_state.m_highSecond.m_ic1);
        const double v34 = (m_state.m_highSecond.m_a2 * v32);
        const double v35 = (v33 + v34);
        const double v36 = (m_state.m_highSecond.m_a2 * m_state.m_highSecond.m_ic1);
        const double v37 = (m_state.m_highSecond.m_ic2 + v36);
        const double v38 = (m_state.m_highSecond.m_a3 * v32);
        const double v39 = (v37 + v38);
        const double v40 = (m_state.m_highSecond.m_k * v35);
        const double v41 = (v10 - v40);
        const double v42 = (v41 - v39);
        const double v43 = (v35 * 0x10000000000000p-51);
        const double v44 = (v43 - m_state.m_highSecond.m_ic1);
        const double v45 = (v39 * 0x10000000000000p-51);
        const double v46 = (v45 - m_state.m_highSecond.m_ic2);
        const double v47 = (v39 + v42);
        const double v48 = m_state.m_first.m_k;
        const double v49 = m_state.m_first.m_a1;
        const double v50 = m_state.m_first.m_a2;
        const double v51 = m_state.m_first.m_a3;
        const double v52 = v12;
        const double v53 = v14;
        const double v54 = v7;
        const double v55 = v3;
        const double v56 = v10;
        const double v57 = v15;
        const double v58 = m_state.m_lowSecond.m_k;
        const double v59 = m_state.m_lowSecond.m_a1;
        const double v60 = m_state.m_lowSecond.m_a2;
        const double v61 = m_state.m_lowSecond.m_a3;
        const double v62 = v28;
        const double v63 = v30;
        const double v64 = v23;
        const double v65 = v19;
        const double v66 = v26;
        const double v67 = v31;
        const double v68 = m_state.m_highSecond.m_k;
        const double v69 = m_state.m_highSecond.m_a1;
        const double v70 = m_state.m_highSecond.m_a2;
        const double v71 = m_state.m_highSecond.m_a3;
        const double v72 = v44;
        const double v73 = v46;
        const double v74 = v39;
        const double v75 = v35;
        const double v76 = v42;
        const double v77 = v47;
        m_state.m_first.m_k = v48;
        m_state.m_first.m_a1 = v49;
        m_state.m_first.m_a2 = v50;
        m_state.m_first.m_a3 = v51;
        m_state.m_first.m_ic1 = v52;
        m_state.m_first.m_ic2 = v53;
        m_state.m_first.m_lowPass = v54;
        m_state.m_first.m_bandPass = v55;
        m_state.m_first.m_highPass = v56;
        m_state.m_first.m_notch = v57;
        m_state.m_lowSecond.m_k = v58;
        m_state.m_lowSecond.m_a1 = v59;
        m_state.m_lowSecond.m_a2 = v60;
        m_state.m_lowSecond.m_a3 = v61;
        m_state.m_lowSecond.m_ic1 = v62;
        m_state.m_lowSecond.m_ic2 = v63;
        m_state.m_lowSecond.m_lowPass = v64;
        m_state.m_lowSecond.m_bandPass = v65;
        m_state.m_lowSecond.m_highPass = v66;
        m_state.m_lowSecond.m_notch = v67;
        m_state.m_highSecond.m_k = v68;
        m_state.m_highSecond.m_a1 = v69;
        m_state.m_highSecond.m_a2 = v70;
        m_state.m_highSecond.m_a3 = v71;
        m_state.m_highSecond.m_ic1 = v72;
        m_state.m_highSecond.m_ic2 = v73;
        m_state.m_highSecond.m_lowPass = v74;
        m_state.m_highSecond.m_bandPass = v75;
        m_state.m_highSecond.m_highPass = v76;
        m_state.m_highSecond.m_notch = v77;
    }

};
