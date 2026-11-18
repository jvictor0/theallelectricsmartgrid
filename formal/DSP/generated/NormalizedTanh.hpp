#pragma once

// Generated from DSP.NormalizedTanh.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct NormalizedTanh
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            double m_gain;
            double m_tanhGain;
        } m_coefficients;

        double m_output;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x * m_state.m_coefficients.m_gain);
        const double v1 = (input.m_x * m_state.m_coefficients.m_gain);
        const double v2 = (v0 * v1);
        const double v3 = (input.m_x * m_state.m_coefficients.m_gain);
        const double v4 = (v2 + 0x1b000000000000p-48);
        const double v5 = (v3 * v4);
        const double v6 = (input.m_x * m_state.m_coefficients.m_gain);
        const double v7 = (v6 * 0x12000000000000p-49);
        const double v8 = (input.m_x * m_state.m_coefficients.m_gain);
        const double v9 = std::fma(v7, v8, 0x1b000000000000p-48);
        const double v10 = (v5 / v9);
        const double v11 = (-1.0);
        const bool v12 = (v11 < v10);
        double v14;
        if (v12)
        {
            v14 = v10;
        }
        else
        {
            const double v13 = (-1.0);
            v14 = v13;
        }

        const bool v15 = (v14 < 1.0);
        double v20;
        if (v15)
        {
            const double v16 = (-1.0);
            const bool v17 = (v16 < v10);
            double v19;
            if (v17)
            {
                v19 = v10;
            }
            else
            {
                const double v18 = (-1.0);
                v19 = v18;
            }

            v20 = v19;
        }
        else
        {
            v20 = 1.0;
        }

        const double v21 = (v20 / m_state.m_coefficients.m_tanhGain);
        const double v22 = m_state.m_coefficients.m_gain;
        const double v23 = m_state.m_coefficients.m_tanhGain;
        const double v24 = v21;
        m_state.m_coefficients.m_gain = v22;
        m_state.m_coefficients.m_tanhGain = v23;
        m_state.m_output = v24;
    }

    // Coefficients generated from DSP.NormalizedTanh.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        double m_gain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = (spec.m_gain * spec.m_gain);
        const double v1 = (v0 + 0x1b000000000000p-48);
        const double v2 = (spec.m_gain * v1);
        const double v3 = (spec.m_gain * 0x12000000000000p-49);
        const double v4 = std::fma(v3, spec.m_gain, 0x1b000000000000p-48);
        const double v5 = (v2 / v4);
        const double v6 = (-1.0);
        const bool v7 = (v6 < v5);
        double v9;
        if (v7)
        {
            v9 = v5;
        }
        else
        {
            const double v8 = (-1.0);
            v9 = v8;
        }

        const bool v10 = (v9 < 1.0);
        double v15;
        if (v10)
        {
            const double v11 = (-1.0);
            const bool v12 = (v11 < v5);
            double v14;
            if (v12)
            {
                v14 = v5;
            }
            else
            {
                const double v13 = (-1.0);
                v14 = v13;
            }

            v15 = v14;
        }
        else
        {
            v15 = 1.0;
        }

        const double v16 = spec.m_gain;
        const double v17 = v15;
        m_state.m_coefficients.m_gain = v16;
        m_state.m_coefficients.m_tanhGain = v17;
    }

    // Reset generated from DSP.NormalizedTanh.resetFloat.
    //

    void Reset()
    {
        const double v0 = 0.0;
        m_state.m_output = v0;
    }

};
