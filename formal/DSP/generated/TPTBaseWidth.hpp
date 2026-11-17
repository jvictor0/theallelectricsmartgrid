#pragma once

// Generated from DSP.TPTBaseWidth.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

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

};
