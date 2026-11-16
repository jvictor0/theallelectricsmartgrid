#pragma once

// Generated from DSP.Coupled.processorFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const double v1 = (m_state.m_model.m_A[0][1] * m_state.m_q[1]);
        const double v2 = (v0 + v1);
        const double v3 = (m_state.m_model.m_B[0] * input.m_x);
        const double v4 = (v2 + v3);
        const double v5 = (m_state.m_model.m_A[1][0] * m_state.m_q[0]);
        const double v6 = (m_state.m_model.m_A[1][1] * m_state.m_q[1]);
        const double v7 = (v5 + v6);
        const double v8 = (m_state.m_model.m_B[1] * input.m_x);
        const double v9 = (v7 + v8);
        const double v10 = (m_state.m_model.m_C[0] * m_state.m_q[0]);
        const double v11 = (m_state.m_model.m_C[1] * m_state.m_q[1]);
        const double v12 = (v10 + v11);
        const double v13 = (m_state.m_model.m_D * input.m_x);
        const double v14 = (v12 + v13);
        const double v15 = m_state.m_model.m_A[0][0];
        const double v16 = m_state.m_model.m_A[0][1];
        const double v17 = m_state.m_model.m_A[1][0];
        const double v18 = m_state.m_model.m_A[1][1];
        const double v19 = m_state.m_model.m_B[0];
        const double v20 = m_state.m_model.m_B[1];
        const double v21 = m_state.m_model.m_C[0];
        const double v22 = m_state.m_model.m_C[1];
        const double v23 = m_state.m_model.m_D;
        const double v24 = v4;
        const double v25 = v9;
        const double v26 = v14;
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
