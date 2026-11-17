#pragma once

// Generated from DSP.TPTSVF.stepFloat. Do not edit.
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
        const double v2 = std::fma(m_state.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_a2, m_state.m_ic1, m_state.m_ic2);
        const double v4 = std::fma(m_state.m_a3, v0, v3);
        const double v5 = (-m_state.m_k);
        const double v6 = std::fma(v5, v2, input.m_x);
        const double v7 = (v6 - v4);
        const double v8 = (-m_state.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        const double v10 = (-m_state.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        const double v12 = (v4 + v7);
        const double v13 = m_state.m_k;
        const double v14 = m_state.m_a1;
        const double v15 = m_state.m_a2;
        const double v16 = m_state.m_a3;
        const double v17 = v9;
        const double v18 = v11;
        const double v19 = v4;
        const double v20 = v2;
        const double v21 = v7;
        const double v22 = v12;
        m_state.m_k = v13;
        m_state.m_a1 = v14;
        m_state.m_a2 = v15;
        m_state.m_a3 = v16;
        m_state.m_ic1 = v17;
        m_state.m_ic2 = v18;
        m_state.m_lowPass = v19;
        m_state.m_bandPass = v20;
        m_state.m_highPass = v21;
        m_state.m_notch = v22;
    }

    // Coefficients generated from DSP.TPTSVF.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        double m_cutoff;
        double m_q;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_cutoff))[0];
        const double v1 = (1.0 / spec.m_q);
        const double v2 = (v0 + v1);
        const double v3 = std::fma(v0, v2, 1.0);
        const double v4 = (1.0 / v3);
        const double v5 = (v0 * v4);
        const double v6 = (v0 * v5);
        const double v7 = v1;
        const double v8 = v4;
        const double v9 = v5;
        const double v10 = v6;
        m_state.m_k = v7;
        m_state.m_a1 = v8;
        m_state.m_a2 = v9;
        m_state.m_a3 = v10;
    }

    // Response generated from DSP.TPTSVF.Snapshot.lowPassMagnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        double m_g;
        double m_k;
    };

    static double LowPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (1.0 * v1);
        const double v5 = (0.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat.
    //

    static double BandPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (0.0 * v1);
        const double v5 = (0.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (1.0 * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.highPassMagnitudeSquaredFloat.
    //

    static double HighPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (0.0 * v1);
        const double v5 = (1.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.notchMagnitudeSquaredFloat.
    //

    static double NotchMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (1.0 * v1);
        const double v5 = (1.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_k * v3);
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
