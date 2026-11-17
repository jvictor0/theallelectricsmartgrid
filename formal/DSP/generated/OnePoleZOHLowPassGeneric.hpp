#pragma once

// Generated from DSP.OnePoleZOHLowPass.step. Do not edit.
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
struct OnePoleZOHLowPassGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_alpha;
        Vector m_y;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v1 = (v0 * m_state.m_y);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_alpha, input.m_x, v1);
        const Vector v3 = m_state.m_alpha;
        const Vector v4 = v2;
        m_state.m_alpha = v3;
        m_state.m_y = v4;
    }

};
