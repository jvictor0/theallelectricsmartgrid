#pragma once

// Generated from DSP.OnePoleTPTLowPass.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

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
        const double v1 = std::fma(v0, m_state.m_G, m_state.m_s);
        const double v2 = std::fma(v0, m_state.m_G, v1);
        const double v3 = m_state.m_G;
        const double v4 = v2;
        const double v5 = v1;
        m_state.m_G = v3;
        m_state.m_s = v4;
        m_state.m_y = v5;
    }

};
