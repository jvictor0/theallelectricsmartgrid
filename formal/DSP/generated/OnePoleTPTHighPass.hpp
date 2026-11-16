#pragma once

// Generated from DSP.OnePoleTPTHighPass.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

struct OnePoleTPTHighPass
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            double m_G;
            double m_s;
            double m_y;
        } m_lowPass;

        double m_y;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_lowPass.m_s);
        const double v1 = (v0 * m_state.m_lowPass.m_G);
        const double v2 = (v1 + m_state.m_lowPass.m_s);
        const double v3 = (v2 + v1);
        const double v4 = (input.m_x - v2);
        const double v5 = m_state.m_lowPass.m_G;
        const double v6 = v3;
        const double v7 = v2;
        const double v8 = v4;
        m_state.m_lowPass.m_G = v5;
        m_state.m_lowPass.m_s = v6;
        m_state.m_lowPass.m_y = v7;
        m_state.m_y = v8;
    }

};
