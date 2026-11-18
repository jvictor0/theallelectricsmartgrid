#pragma once

// Generated from DSP.Coupled.processor. Do not edit.
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
struct CoupledProcessorGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            std::array<std::array<Vector, 2>, 2> m_A;
            std::array<Vector, 2> m_B;
            std::array<Vector, 2> m_C;
            Vector m_D;
        } m_model;

        std::array<Vector, 2> m_q;
    };

    struct Input
    {
        Vector m_x;
    };

    struct Output
    {
        Vector m_y;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (m_state.m_model.m_A[0][0] * m_state.m_q[0]);
        const Vector v1 = Vector::FusedMultiplyAdd(m_state.m_model.m_A[0][1], m_state.m_q[1], v0);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_model.m_B[0], input.m_x, v1);
        const Vector v3 = (m_state.m_model.m_A[1][0] * m_state.m_q[0]);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_model.m_A[1][1], m_state.m_q[1], v3);
        const Vector v5 = Vector::FusedMultiplyAdd(m_state.m_model.m_B[1], input.m_x, v4);
        const Vector v6 = (m_state.m_model.m_C[0] * m_state.m_q[0]);
        m_state.m_q[0] = v2;
        const Vector v7 = Vector::FusedMultiplyAdd(m_state.m_model.m_C[1], m_state.m_q[1], v6);
        m_state.m_q[1] = v5;
        const Vector v8 = Vector::FusedMultiplyAdd(m_state.m_model.m_D, input.m_x, v7);
        m_output.m_y = v8;
    }

};
