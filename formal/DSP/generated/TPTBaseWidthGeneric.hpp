#pragma once

// Generated from DSP.TPTBaseWidth.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct TPTBaseWidthGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            struct
            {
                Vector m_G;
                Vector m_s;
                Vector m_y;
            } m_lowPass;

            Vector m_y;
        } m_highPass;

        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_lowPass;

    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_highPass.m_lowPass.m_s);
        const Vector v1 = (v0 * m_state.m_highPass.m_lowPass.m_G);
        const Vector v2 = (v1 + m_state.m_highPass.m_lowPass.m_s);
        const Vector v3 = (v2 + v1);
        const Vector v4 = (input.m_x - v2);
        const Vector v5 = (v4 - m_state.m_lowPass.m_s);
        const Vector v6 = (v5 * m_state.m_lowPass.m_G);
        const Vector v7 = (v6 + m_state.m_lowPass.m_s);
        const Vector v8 = (v7 + v6);
        const Vector v9 = m_state.m_highPass.m_lowPass.m_G;
        const Vector v10 = v3;
        const Vector v11 = v2;
        const Vector v12 = v4;
        const Vector v13 = m_state.m_lowPass.m_G;
        const Vector v14 = v8;
        const Vector v15 = v7;
        m_state.m_highPass.m_lowPass.m_G = v9;
        m_state.m_highPass.m_lowPass.m_s = v10;
        m_state.m_highPass.m_lowPass.m_y = v11;
        m_state.m_highPass.m_y = v12;
        m_state.m_lowPass.m_G = v13;
        m_state.m_lowPass.m_s = v14;
        m_state.m_lowPass.m_y = v15;
    }

};
