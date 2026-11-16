#pragma once

// Generated from DSP.OnePoleTPTLowPass.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

struct OnePoleTPTLowPass
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        double m_G;
        double m_s;
        double m_y;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_s);
        const double v1 = (v0 * m_state.m_G);
        const double v2 = (v1 + m_state.m_s);
        const double v3 = (v2 + v1);
        const double v4 = m_state.m_G;
        const double v5 = v3;
        const double v6 = v2;
        m_state.m_G = v4;
        m_state.m_s = v5;
        m_state.m_y = v6;
    }

};
