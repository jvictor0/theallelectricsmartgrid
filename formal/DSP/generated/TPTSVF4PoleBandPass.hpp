#pragma once

// Generated from DSP.TPTSVF4Pole.stepBandPassFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"
#include "MultiChannelNumber.hpp"

#include "ResponseFrequency.hpp"
struct TPTSVF4PoleBandPass
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
        const double v32 = m_state.m_stage1.m_k;
        const double v33 = m_state.m_stage1.m_a1;
        const double v34 = m_state.m_stage1.m_a2;
        const double v35 = m_state.m_stage1.m_a3;
        const double v36 = v9;
        const double v37 = v11;
        const double v38 = v4;
        const double v39 = v2;
        const double v40 = v7;
        const double v41 = v12;
        const double v42 = v15;
        const double v43 = m_state.m_stage2.m_k;
        const double v44 = m_state.m_stage2.m_a1;
        const double v45 = m_state.m_stage2.m_a2;
        const double v46 = m_state.m_stage2.m_a3;
        const double v47 = v25;
        const double v48 = v27;
        const double v49 = v20;
        const double v50 = v18;
        const double v51 = v23;
        const double v52 = v28;
        const double v53 = v31;
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
        m_state.m_stage1.m_allPass = v42;
        m_state.m_stage2.m_k = v43;
        m_state.m_stage2.m_a1 = v44;
        m_state.m_stage2.m_a2 = v45;
        m_state.m_stage2.m_a3 = v46;
        m_state.m_stage2.m_ic1 = v47;
        m_state.m_stage2.m_ic2 = v48;
        m_state.m_stage2.m_lowPass = v49;
        m_state.m_stage2.m_bandPass = v50;
        m_state.m_stage2.m_highPass = v51;
        m_state.m_stage2.m_notch = v52;
        m_state.m_stage2.m_allPass = v53;
    }

    // Coefficients generated from DSP.TPTSVF4Pole.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        struct
        {
            double m_cutoff;
            double m_q;
        } m_stage1;

        struct
        {
            double m_cutoff;
            double m_q;
        } m_stage2;

    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_stage1.m_cutoff))[0];
        const double v1 = (1.0 / spec.m_stage1.m_q);
        const double v2 = (v0 + v1);
        const double v3 = std::fma(v0, v2, 1.0);
        const double v4 = (1.0 / v3);
        const double v5 = (v0 * v4);
        const double v6 = (v0 * v5);
        const double v7 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_stage2.m_cutoff))[0];
        const double v8 = (1.0 / spec.m_stage2.m_q);
        const double v9 = (v7 + v8);
        const double v10 = std::fma(v7, v9, 1.0);
        const double v11 = (1.0 / v10);
        const double v12 = (v7 * v11);
        const double v13 = (v7 * v12);
        const double v14 = v1;
        const double v15 = v4;
        const double v16 = v5;
        const double v17 = v6;
        const double v18 = v8;
        const double v19 = v11;
        const double v20 = v12;
        const double v21 = v13;
        m_state.m_stage1.m_k = v14;
        m_state.m_stage1.m_a1 = v15;
        m_state.m_stage1.m_a2 = v16;
        m_state.m_stage1.m_a3 = v17;
        m_state.m_stage2.m_k = v18;
        m_state.m_stage2.m_a1 = v19;
        m_state.m_stage2.m_a2 = v20;
        m_state.m_stage2.m_a3 = v21;
    }

    // Response generated from DSP.TPTSVF4Pole.Snapshot.bandPassMagnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        struct
        {
            double m_g;
            double m_k;
        } m_stage1;

        struct
        {
            double m_g;
            double m_k;
        } m_stage2;

    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_stage1.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (0.0 * v1);
        const double v5 = (0.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (1.0 * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_stage1.m_k * v3);
        const double v10 = (v7 * v7);
        const double v11 = std::fma(v6, v6, v10);
        const double v12 = (v7 * v7);
        const double v13 = std::fma(v6, v6, v12);
        const bool v14 = (0.0 < v13);
        double v17;
        if (v14)
        {
            const double v15 = (v9 * v9);
            const double v16 = std::fma(v8, v8, v15);
            v17 = v16;
        }
        else
        {
            v17 = 1.0;
        }

        const double v18 = (v11 / v17);
        const double v19 = (snapshot.m_stage2.m_g * input.m_cosHalf);
        const double v20 = (v19 * v19);
        const double v21 = (input.m_sinHalf * input.m_sinHalf);
        const double v22 = (v19 * input.m_sinHalf);
        const double v23 = (0.0 * v20);
        const double v24 = (0.0 * v21);
        const double v25 = (v23 - v24);
        const double v26 = (1.0 * v22);
        const double v27 = (v20 - v21);
        const double v28 = (snapshot.m_stage2.m_k * v22);
        const double v29 = (v26 * v26);
        const double v30 = std::fma(v25, v25, v29);
        const double v31 = (v26 * v26);
        const double v32 = std::fma(v25, v25, v31);
        const bool v33 = (0.0 < v32);
        double v36;
        if (v33)
        {
            const double v34 = (v28 * v28);
            const double v35 = std::fma(v27, v27, v34);
            v36 = v35;
        }
        else
        {
            v36 = 1.0;
        }

        const double v37 = (v30 / v36);
        const double v38 = (v18 * v37);
        return v38;
    }

};
