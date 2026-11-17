#pragma once

// Generated from DSP.MorphingTPTSVF.stepFloat. Do not edit.
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
        const double v2 = std::fma(m_state.m_svf.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_svf.m_a2, m_state.m_svf.m_ic1, m_state.m_svf.m_ic2);
        const double v4 = std::fma(m_state.m_svf.m_a3, v0, v3);
        const double v5 = (-m_state.m_svf.m_k);
        const double v6 = std::fma(v5, v2, input.m_x);
        const double v7 = (v6 - v4);
        const double v8 = (-m_state.m_svf.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        const double v10 = (-m_state.m_svf.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        const double v12 = (v4 + v7);
        const double v13 = (v4 * m_state.m_cosine);
        const double v14 = std::fma(v2, m_state.m_sine, v13);
        const double v15 = (v2 * m_state.m_cosine);
        const double v16 = std::fma(v7, m_state.m_sine, v15);
        const bool v17 = (0x10000000000000p-53 < m_state.m_blend);
        double v18;
        if (v17)
        {
            v18 = v16;
        }
        else
        {
            v18 = v14;
        }

        const double v19 = m_state.m_svf.m_k;
        const double v20 = m_state.m_svf.m_a1;
        const double v21 = m_state.m_svf.m_a2;
        const double v22 = m_state.m_svf.m_a3;
        const double v23 = v9;
        const double v24 = v11;
        const double v25 = v4;
        const double v26 = v2;
        const double v27 = v7;
        const double v28 = v12;
        const double v29 = m_state.m_cosine;
        const double v30 = m_state.m_sine;
        const double v31 = m_state.m_blend;
        const double v32 = v18;
        m_state.m_svf.m_k = v19;
        m_state.m_svf.m_a1 = v20;
        m_state.m_svf.m_a2 = v21;
        m_state.m_svf.m_a3 = v22;
        m_state.m_svf.m_ic1 = v23;
        m_state.m_svf.m_ic2 = v24;
        m_state.m_svf.m_lowPass = v25;
        m_state.m_svf.m_bandPass = v26;
        m_state.m_svf.m_highPass = v27;
        m_state.m_svf.m_notch = v28;
        m_state.m_cosine = v29;
        m_state.m_sine = v30;
        m_state.m_blend = v31;
        m_state.m_output = v32;
    }

    // Coefficients generated from DSP.MorphingTPTSVF.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        struct
        {
            double m_cutoff;
            double m_q;
        } m_svf;

        double m_blend;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = (1.0 / 0x10000000000000p-51);
        const bool v1 = (v0 < spec.m_blend);
        double v5;
        if (v1)
        {
            const double v2 = (spec.m_blend - v0);
            const double v3 = (v2 / 0x10000000000000p-51);
            v5 = v3;
        }
        else
        {
            const double v4 = (spec.m_blend / 0x10000000000000p-51);
            v5 = v4;
        }

        const double v6 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_svf.m_cutoff))[0];
        const double v7 = (1.0 / spec.m_svf.m_q);
        const double v8 = (v6 + v7);
        const double v9 = std::fma(v6, v8, 1.0);
        const double v10 = (1.0 / v9);
        const double v11 = (v6 * v10);
        const double v12 = (v6 * v11);
        const double v13 = Math4096::Cos2pi(MultiChannelNumber<double, 1>::Splat(v5))[0];
        const double v14 = Math4096::Sin2pi(MultiChannelNumber<double, 1>::Splat(v5))[0];
        const double v15 = v7;
        const double v16 = v10;
        const double v17 = v11;
        const double v18 = v12;
        const double v19 = v13;
        const double v20 = v14;
        const double v21 = spec.m_blend;
        m_state.m_svf.m_k = v15;
        m_state.m_svf.m_a1 = v16;
        m_state.m_svf.m_a2 = v17;
        m_state.m_svf.m_a3 = v18;
        m_state.m_cosine = v19;
        m_state.m_sine = v20;
        m_state.m_blend = v21;
    }

    // Response generated from DSP.MorphingTPTSVF.Snapshot.magnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        struct
        {
            double m_g;
            double m_k;
        } m_svf;

        double m_low;
        double m_band;
        double m_high;
    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_svf.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (snapshot.m_low * v1);
        const double v5 = (snapshot.m_high * v2);
        const double v6 = (v4 - v5);
        const double v7 = (snapshot.m_band * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_svf.m_k * v3);
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
        return v18;
    }

};
