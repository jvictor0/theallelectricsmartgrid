#pragma once

// Generated from DSP.Coupled.processor. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const Vector v1 = (m_state.m_model.m_A[0][1] * m_state.m_q[1]);
        const Vector v2 = (v0 + v1);
        const Vector v3 = (m_state.m_model.m_B[0] * input.m_x);
        const Vector v4 = (v2 + v3);
        const Vector v5 = (m_state.m_model.m_A[1][0] * m_state.m_q[0]);
        const Vector v6 = (m_state.m_model.m_A[1][1] * m_state.m_q[1]);
        const Vector v7 = (v5 + v6);
        const Vector v8 = (m_state.m_model.m_B[1] * input.m_x);
        const Vector v9 = (v7 + v8);
        const Vector v10 = (m_state.m_model.m_C[0] * m_state.m_q[0]);
        const Vector v11 = (m_state.m_model.m_C[1] * m_state.m_q[1]);
        const Vector v12 = (v10 + v11);
        const Vector v13 = (m_state.m_model.m_D * input.m_x);
        const Vector v14 = (v12 + v13);
        const Vector v15 = m_state.m_model.m_A[0][0];
        const Vector v16 = m_state.m_model.m_A[0][1];
        const Vector v17 = m_state.m_model.m_A[1][0];
        const Vector v18 = m_state.m_model.m_A[1][1];
        const Vector v19 = m_state.m_model.m_B[0];
        const Vector v20 = m_state.m_model.m_B[1];
        const Vector v21 = m_state.m_model.m_C[0];
        const Vector v22 = m_state.m_model.m_C[1];
        const Vector v23 = m_state.m_model.m_D;
        const Vector v24 = v4;
        const Vector v25 = v9;
        const Vector v26 = v14;
        m_state.m_model.m_A[0][0] = v15;
        m_state.m_model.m_A[0][1] = v16;
        m_state.m_model.m_A[1][0] = v17;
        m_state.m_model.m_A[1][1] = v18;
        m_state.m_model.m_B[0] = v19;
        m_state.m_model.m_B[1] = v20;
        m_state.m_model.m_C[0] = v21;
        m_state.m_model.m_C[1] = v22;
        m_state.m_model.m_D = v23;
        m_state.m_q[0] = v24;
        m_state.m_q[1] = v25;
        m_output.m_y = v26;
    }

};
