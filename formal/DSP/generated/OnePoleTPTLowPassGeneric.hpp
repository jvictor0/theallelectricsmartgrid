#pragma once

// Generated from DSP.OnePoleTPTLowPass.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct OnePoleTPTLowPassGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_G;
        Vector m_s;
        Vector m_y;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_s);
        const Vector v1 = (v0 * m_state.m_G);
        const Vector v2 = (v1 + m_state.m_s);
        const Vector v3 = (v2 + v1);
        const Vector v4 = m_state.m_G;
        const Vector v5 = v3;
        const Vector v6 = v2;
        m_state.m_G = v4;
        m_state.m_s = v5;
        m_state.m_y = v6;
    }

};
