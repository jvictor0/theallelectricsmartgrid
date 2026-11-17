#pragma once

// Generated from DSP.OnePoleZOHLowPass.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "ResponseFrequency.hpp"

struct OnePoleZOHLowPass
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        double m_alpha;
        double m_y;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (1.0 - m_state.m_alpha);
        const double v1 = (v0 * m_state.m_y);
        const double v2 = std::fma(m_state.m_alpha, input.m_x, v1);
        const double v3 = m_state.m_alpha;
        const double v4 = v2;
        m_state.m_alpha = v3;
        m_state.m_y = v4;
    }

    // Response generated from DSP.OnePoleZOHLowPass.Snapshot.magnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        double m_alpha;
    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_alpha * snapshot.m_alpha);
        const double v1 = (1.0 - snapshot.m_alpha);
        const double v2 = (0x10000000000000p-50 * v1);
        const double v3 = (input.m_sinHalf * input.m_sinHalf);
        const double v4 = std::fma(v2, v3, v0);
        const bool v5 = (0.0 < snapshot.m_alpha);
        double v6;
        if (v5)
        {
            v6 = v4;
        }
        else
        {
            v6 = 1.0;
        }

        const double v7 = (v0 / v6);
        return v7;
    }

};
