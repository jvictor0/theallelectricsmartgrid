#pragma once

// Generated from DSP.OnePoleZOHLowPass.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const double v2 = (m_state.m_alpha * input.m_x);
        const double v3 = (v1 + v2);
        const double v4 = m_state.m_alpha;
        const double v5 = v3;
        m_state.m_alpha = v4;
        m_state.m_y = v5;
    }

};
