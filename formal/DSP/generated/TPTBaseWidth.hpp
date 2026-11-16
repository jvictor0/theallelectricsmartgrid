#pragma once

// Generated from DSP.TPTBaseWidth.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const double v1 = (v0 * m_state.m_highPass.m_lowPass.m_G);
        const double v2 = (v1 + m_state.m_highPass.m_lowPass.m_s);
        const double v3 = (v2 + v1);
        const double v4 = (input.m_x - v2);
        const double v5 = (v4 - m_state.m_lowPass.m_s);
        const double v6 = (v5 * m_state.m_lowPass.m_G);
        const double v7 = (v6 + m_state.m_lowPass.m_s);
        const double v8 = (v7 + v6);
        const double v9 = m_state.m_highPass.m_lowPass.m_G;
        const double v10 = v3;
        const double v11 = v2;
        const double v12 = v4;
        const double v13 = m_state.m_lowPass.m_G;
        const double v14 = v8;
        const double v15 = v7;
        m_state.m_highPass.m_lowPass.m_G = v9;
        m_state.m_highPass.m_lowPass.m_s = v10;
        m_state.m_highPass.m_lowPass.m_y = v11;
        m_state.m_highPass.m_y = v12;
        m_state.m_lowPass.m_G = v13;
        m_state.m_lowPass.m_s = v14;
        m_state.m_lowPass.m_y = v15;
    }

};
