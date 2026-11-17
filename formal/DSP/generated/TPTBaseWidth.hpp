#pragma once

// Generated from DSP.TPTBaseWidth.stepFloat. Do not edit.
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
struct TPTBaseWidth
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            struct
            {
                double m_G;
                double m_s;
                double m_y;
            } m_lowPass;

            double m_y;
        } m_highPass;

        struct
        {
            double m_G;
            double m_s;
            double m_y;
        } m_lowPass;

    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_highPass.m_lowPass.m_s);
        const double v1 = std::fma(v0, m_state.m_highPass.m_lowPass.m_G, m_state.m_highPass.m_lowPass.m_s);
        const double v2 = std::fma(v0, m_state.m_highPass.m_lowPass.m_G, v1);
        const double v3 = (input.m_x - v1);
        const double v4 = (v3 - m_state.m_lowPass.m_s);
        const double v5 = std::fma(v4, m_state.m_lowPass.m_G, m_state.m_lowPass.m_s);
        const double v6 = std::fma(v4, m_state.m_lowPass.m_G, v5);
        const double v7 = m_state.m_highPass.m_lowPass.m_G;
        const double v8 = v2;
        const double v9 = v1;
        const double v10 = v3;
        const double v11 = m_state.m_lowPass.m_G;
        const double v12 = v6;
        const double v13 = v5;
        m_state.m_highPass.m_lowPass.m_G = v7;
        m_state.m_highPass.m_lowPass.m_s = v8;
        m_state.m_highPass.m_lowPass.m_y = v9;
        m_state.m_highPass.m_y = v10;
        m_state.m_lowPass.m_G = v11;
        m_state.m_lowPass.m_s = v12;
        m_state.m_lowPass.m_y = v13;
    }

    // Coefficients generated from DSP.TPTBaseWidth.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        struct
        {
            double m_cutoff;
        } m_highPass;

        struct
        {
            double m_cutoff;
        } m_lowPass;

    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_highPass.m_cutoff))[0];
        const double v1 = (1.0 + v0);
        const double v2 = (v0 / v1);
        const double v3 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_lowPass.m_cutoff))[0];
        const double v4 = (1.0 + v3);
        const double v5 = (v3 / v4);
        const double v6 = v2;
        const double v7 = v5;
        m_state.m_highPass.m_lowPass.m_G = v6;
        m_state.m_lowPass.m_G = v7;
    }

    // Response generated from DSP.TPTBaseWidth.Snapshot.magnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        struct
        {
            double m_g;
        } m_highPass;

        struct
        {
            double m_g;
        } m_lowPass;

    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (input.m_sinHalf * input.m_sinHalf);
        const double v1 = (input.m_sinHalf * input.m_sinHalf);
        const bool v2 = (0.0 < v1);
        double v7;
        if (v2)
        {
            const double v3 = (snapshot.m_highPass.m_g * input.m_cosHalf);
            const double v4 = (snapshot.m_highPass.m_g * input.m_cosHalf);
            const double v5 = (input.m_sinHalf * input.m_sinHalf);
            const double v6 = std::fma(v3, v4, v5);
            v7 = v6;
        }
        else
        {
            v7 = 1.0;
        }

        const double v8 = (v0 / v7);
        const double v9 = (snapshot.m_lowPass.m_g * input.m_cosHalf);
        const double v10 = (v9 * v9);
        const double v11 = (v9 * v9);
        const bool v12 = (0.0 < v11);
        double v15;
        if (v12)
        {
            const double v13 = (input.m_sinHalf * input.m_sinHalf);
            const double v14 = std::fma(v9, v9, v13);
            v15 = v14;
        }
        else
        {
            v15 = 1.0;
        }

        const double v16 = (v10 / v15);
        const double v17 = (v8 * v16);
        return v17;
    }

};
