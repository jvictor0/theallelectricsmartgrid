#pragma once

// Generated from DSP.TPTLadder.stepFloat. Do not edit.
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

    // Coefficients generated from DSP.TPTLadder.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        double m_cutoff;
        double m_feedback;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_cutoff))[0];
        const double v1 = (1.0 + v0);
        const double v2 = (v0 / v1);
        const double v3 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_cutoff))[0];
        const double v4 = (1.0 + v3);
        const double v5 = (v3 / v4);
        const double v6 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_cutoff))[0];
        const double v7 = (1.0 + v6);
        const double v8 = (v6 / v7);
        const double v9 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_cutoff))[0];
        const double v10 = (1.0 + v9);
        const double v11 = (v9 / v10);
        const double v12 = spec.m_feedback;
        const double v13 = v2;
        const double v14 = v5;
        const double v15 = v8;
        const double v16 = v11;
        m_state.m_k = v12;
        m_state.m_stage1.m_G = v13;
        m_state.m_stage2.m_G = v14;
        m_state.m_stage3.m_G = v15;
        m_state.m_stage4.m_G = v16;
    }

    // Response generated from DSP.TPTLadder.Snapshot.magnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        double m_g1;
        double m_g2;
        double m_g3;
        double m_g4;
        double m_k;
    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g1 * input.m_cosHalf);
        const double v1 = (snapshot.m_g2 * input.m_cosHalf);
        const double v2 = (snapshot.m_g3 * input.m_cosHalf);
        const double v3 = (snapshot.m_g4 * input.m_cosHalf);
        const double v4 = (input.m_sinHalf * input.m_sinHalf);
        const double v5 = (-v4);
        const double v6 = std::fma(v0, v1, v5);
        const double v7 = (input.m_sinHalf * v1);
        const double v8 = std::fma(v0, input.m_sinHalf, v7);
        const double v9 = (v8 * input.m_sinHalf);
        const double v10 = (-v9);
        const double v11 = std::fma(v6, v2, v10);
        const double v12 = (v8 * v2);
        const double v13 = std::fma(v6, input.m_sinHalf, v12);
        const double v14 = (v0 * v1);
        const double v15 = (v14 * v2);
        const double v16 = (v15 * v3);
        const double v17 = (v13 * input.m_sinHalf);
        const double v18 = (-v17);
        const double v19 = std::fma(v11, v3, v18);
        const double v20 = std::fma(snapshot.m_k, v16, v19);
        const double v21 = (v13 * v3);
        const double v22 = std::fma(v11, input.m_sinHalf, v21);
        const double v23 = (v16 * v16);
        const double v24 = (v16 * v16);
        const bool v25 = (0.0 < v24);
        double v28;
        if (v25)
        {
            const double v26 = (v22 * v22);
            const double v27 = std::fma(v20, v20, v26);
            v28 = v27;
        }
        else
        {
            v28 = 1.0;
        }

        const double v29 = (v23 / v28);
        return v29;
    }

};
