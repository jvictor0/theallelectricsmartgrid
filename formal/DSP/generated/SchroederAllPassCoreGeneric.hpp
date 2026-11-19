#pragma once

// Generated from DSP.DelayKernels.Runtime.AllPass.process. Do not edit.
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
struct SchroederAllPassCoreGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_gain;
    };

    struct Input
    {
        Vector m_x;
        Vector m_delayed;
    };

    struct Output
    {
        Vector m_y;
        Vector m_written;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (-m_state.m_gain);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, input.m_x, input.m_delayed);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_gain, v1, input.m_x);
        m_output.m_y = v1;
        m_output.m_written = v2;
    }

};
