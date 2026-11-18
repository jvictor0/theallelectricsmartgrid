#pragma once

// Generated from DSP.BandPassedEnvelopeFollower.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct BandPassedEnvelopeFollowerCore
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
            double m_allPass;
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
            double m_allPass;
        } m_stage2;

        double m_power;
        double m_powerAlpha;
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
        const double v13 = (-m_state.m_stage1.m_k);
        const double v14 = (v13 * 0x10000000000000p-51);
        const double v15 = std::fma(v14, v2, input.m_x);
        const double v16 = (v2 - m_state.m_stage2.m_ic2);
        const double v17 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const double v18 = std::fma(m_state.m_stage2.m_a2, v16, v17);
        const double v19 = std::fma(m_state.m_stage2.m_a2, m_state.m_stage2.m_ic1, m_state.m_stage2.m_ic2);
        const double v20 = std::fma(m_state.m_stage2.m_a3, v16, v19);
        const double v21 = (-m_state.m_stage2.m_k);
        const double v22 = std::fma(v21, v18, v2);
        const double v23 = (v22 - v20);
        const double v24 = (-m_state.m_stage2.m_ic1);
        const double v25 = std::fma(v18, 0x10000000000000p-51, v24);
        const double v26 = (-m_state.m_stage2.m_ic2);
        const double v27 = std::fma(v20, 0x10000000000000p-51, v26);
        const double v28 = (v20 + v23);
        const double v29 = (-m_state.m_stage2.m_k);
        const double v30 = (v29 * 0x10000000000000p-51);
        const double v31 = std::fma(v30, v18, v2);
        const double v32 = (v18 * v18);
        const double v33 = (v32 - m_state.m_power);
        const double v34 = (m_state.m_powerAlpha * v33);
        const double v35 = (m_state.m_power + v34);
        const double v36 = m_state.m_stage1.m_k;
        const double v37 = m_state.m_stage1.m_a1;
        const double v38 = m_state.m_stage1.m_a2;
        const double v39 = m_state.m_stage1.m_a3;
        const double v40 = v9;
        const double v41 = v11;
        const double v42 = v4;
        const double v43 = v2;
        const double v44 = v7;
        const double v45 = v12;
        const double v46 = v15;
        const double v47 = m_state.m_stage2.m_k;
        const double v48 = m_state.m_stage2.m_a1;
        const double v49 = m_state.m_stage2.m_a2;
        const double v50 = m_state.m_stage2.m_a3;
        const double v51 = v25;
        const double v52 = v27;
        const double v53 = v20;
        const double v54 = v18;
        const double v55 = v23;
        const double v56 = v28;
        const double v57 = v31;
        const double v58 = v35;
        const double v59 = m_state.m_powerAlpha;
        m_state.m_stage1.m_k = v36;
        m_state.m_stage1.m_a1 = v37;
        m_state.m_stage1.m_a2 = v38;
        m_state.m_stage1.m_a3 = v39;
        m_state.m_stage1.m_ic1 = v40;
        m_state.m_stage1.m_ic2 = v41;
        m_state.m_stage1.m_lowPass = v42;
        m_state.m_stage1.m_bandPass = v43;
        m_state.m_stage1.m_highPass = v44;
        m_state.m_stage1.m_notch = v45;
        m_state.m_stage1.m_allPass = v46;
        m_state.m_stage2.m_k = v47;
        m_state.m_stage2.m_a1 = v48;
        m_state.m_stage2.m_a2 = v49;
        m_state.m_stage2.m_a3 = v50;
        m_state.m_stage2.m_ic1 = v51;
        m_state.m_stage2.m_ic2 = v52;
        m_state.m_stage2.m_lowPass = v53;
        m_state.m_stage2.m_bandPass = v54;
        m_state.m_stage2.m_highPass = v55;
        m_state.m_stage2.m_notch = v56;
        m_state.m_stage2.m_allPass = v57;
        m_state.m_power = v58;
        m_state.m_powerAlpha = v59;
    }

    // Coefficients generated from DSP.BandPassedEnvelopeFollower.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        double m_integratorGain;
        double m_powerAlpha;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = (spec.m_integratorGain + 1.0);
        const double v1 = std::fma(spec.m_integratorGain, v0, 1.0);
        const double v2 = (1.0 / v1);
        const double v3 = (spec.m_integratorGain * v2);
        const double v4 = (spec.m_integratorGain * v3);
        const double v5 = (spec.m_integratorGain + 1.0);
        const double v6 = std::fma(spec.m_integratorGain, v5, 1.0);
        const double v7 = (1.0 / v6);
        const double v8 = (spec.m_integratorGain * v7);
        const double v9 = (spec.m_integratorGain * v8);
        const double v10 = 1.0;
        const double v11 = v2;
        const double v12 = v3;
        const double v13 = v4;
        const double v14 = 1.0;
        const double v15 = v7;
        const double v16 = v8;
        const double v17 = v9;
        const double v18 = spec.m_powerAlpha;
        m_state.m_stage1.m_k = v10;
        m_state.m_stage1.m_a1 = v11;
        m_state.m_stage1.m_a2 = v12;
        m_state.m_stage1.m_a3 = v13;
        m_state.m_stage2.m_k = v14;
        m_state.m_stage2.m_a1 = v15;
        m_state.m_stage2.m_a2 = v16;
        m_state.m_stage2.m_a3 = v17;
        m_state.m_powerAlpha = v18;
    }

    // Reset generated from DSP.BandPassedEnvelopeFollower.resetFloat.
    //

    void Reset()
    {
        const double v0 = 0.0;
        const double v1 = 0.0;
        const double v2 = 0.0;
        const double v3 = 0.0;
        const double v4 = 0.0;
        const double v5 = 0.0;
        const double v6 = 0.0;
        const double v7 = 0.0;
        const double v8 = 0.0;
        const double v9 = 0.0;
        const double v10 = 0.0;
        const double v11 = 0.0;
        const double v12 = 0.0;
        const double v13 = 0.0;
        const double v14 = 0.0;
        m_state.m_stage1.m_ic1 = v0;
        m_state.m_stage1.m_ic2 = v1;
        m_state.m_stage1.m_lowPass = v2;
        m_state.m_stage1.m_bandPass = v3;
        m_state.m_stage1.m_highPass = v4;
        m_state.m_stage1.m_notch = v5;
        m_state.m_stage1.m_allPass = v6;
        m_state.m_stage2.m_ic1 = v7;
        m_state.m_stage2.m_ic2 = v8;
        m_state.m_stage2.m_lowPass = v9;
        m_state.m_stage2.m_bandPass = v10;
        m_state.m_stage2.m_highPass = v11;
        m_state.m_stage2.m_notch = v12;
        m_state.m_stage2.m_allPass = v13;
        m_state.m_power = v14;
    }

};
