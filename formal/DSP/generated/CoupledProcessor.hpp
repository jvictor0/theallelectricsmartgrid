#pragma once

// Generated from DSP.Coupled.processorFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct CoupledProcessor
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            std::array<std::array<double, 2>, 2> m_A;
            std::array<double, 2> m_B;
            std::array<double, 2> m_C;
            double m_D;
        } m_model;

        std::array<double, 2> m_q;
    };

    struct Input
    {
        double m_x;
    };

    struct Output
    {
        double m_y;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const double v0 = (m_state.m_model.m_A[0][0] * m_state.m_q[0]);
        const double v1 = std::fma(m_state.m_model.m_A[0][1], m_state.m_q[1], v0);
        const double v2 = std::fma(m_state.m_model.m_B[0], input.m_x, v1);
        const double v3 = (m_state.m_model.m_A[1][0] * m_state.m_q[0]);
        const double v4 = std::fma(m_state.m_model.m_A[1][1], m_state.m_q[1], v3);
        const double v5 = std::fma(m_state.m_model.m_B[1], input.m_x, v4);
        const double v6 = (m_state.m_model.m_C[0] * m_state.m_q[0]);
        const double v7 = std::fma(m_state.m_model.m_C[1], m_state.m_q[1], v6);
        const double v8 = std::fma(m_state.m_model.m_D, input.m_x, v7);
        const double v9 = m_state.m_model.m_A[0][0];
        const double v10 = m_state.m_model.m_A[0][1];
        const double v11 = m_state.m_model.m_A[1][0];
        const double v12 = m_state.m_model.m_A[1][1];
        const double v13 = m_state.m_model.m_B[0];
        const double v14 = m_state.m_model.m_B[1];
        const double v15 = m_state.m_model.m_C[0];
        const double v16 = m_state.m_model.m_C[1];
        const double v17 = m_state.m_model.m_D;
        const double v18 = v2;
        const double v19 = v5;
        const double v20 = v8;
        m_state.m_model.m_A[0][0] = v9;
        m_state.m_model.m_A[0][1] = v10;
        m_state.m_model.m_A[1][0] = v11;
        m_state.m_model.m_A[1][1] = v12;
        m_state.m_model.m_B[0] = v13;
        m_state.m_model.m_B[1] = v14;
        m_state.m_model.m_C[0] = v15;
        m_state.m_model.m_C[1] = v16;
        m_state.m_model.m_D = v17;
        m_state.m_q[0] = v18;
        m_state.m_q[1] = v19;
        m_output.m_y = v20;
    }

};
