#pragma once

// Generated from DSP.OnePoleTPTHighPass.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct OnePoleTPTHighPassGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_lowPass;

        Vector m_y;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_lowPass.m_s);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, m_state.m_lowPass.m_G, m_state.m_lowPass.m_s);
        const Vector v2 = Vector::FusedMultiplyAdd(v0, m_state.m_lowPass.m_G, v1);
        const Vector v3 = (input.m_x - v1);
        const Vector v4 = m_state.m_lowPass.m_G;
        const Vector v5 = v2;
        const Vector v6 = v1;
        const Vector v7 = v3;
        m_state.m_lowPass.m_G = v4;
        m_state.m_lowPass.m_s = v5;
        m_state.m_lowPass.m_y = v6;
        m_state.m_y = v7;
    }

};
