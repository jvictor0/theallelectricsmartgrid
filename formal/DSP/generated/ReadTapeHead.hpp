#pragma once

// Generated from DSP.TapeHead.run. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
struct ReadTapeHead
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        std::array<size_t, 4> m_loopSelector;
    };

    struct Input
    {
        double m_writePhase;
        std::array<double, 6> m_cycleRatio;
        std::array<bool, 6> m_crossed;
        std::array<size_t, 4> m_requestedLoop;
        std::array<double, 4> m_speed;
        std::array<float, 4> m_fraction;
    };

    struct Output
    {
        std::array<double, 4> m_readPhase;
        std::array<float, 4> m_relativeReadPosition;
        std::array<double, 4> m_loopPeriod;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        std::array<size_t, 4> v0;
        for (std::size_t v1 = 0; v1 < 4; ++v1)
        {
            const bool v2 = (m_state.m_loopSelector[v1] == input.m_requestedLoop[v1]);
            size_t v6;
            if (v2)
            {
                v6 = m_state.m_loopSelector[v1];
            }
            else
            {
                bool v3;
                if (input.m_crossed[m_state.m_loopSelector[v1]])
                {
                    v3 = input.m_crossed[input.m_requestedLoop[v1]];
                }
                else
                {
                    v3 = false;
                }

                const bool v4 = (v3 == true);
                size_t v5;
                if (v4)
                {
                    v5 = input.m_requestedLoop[v1];
                }
                else
                {
                    v5 = m_state.m_loopSelector[v1];
                }

                v6 = v5;
            }

            v0[v1] = v6;
        }

        std::array<double, 4> v7;
        for (std::size_t v8 = 0; v8 < 4; ++v8)
        {
            const double v9 = (1.0 / input.m_cycleRatio[v0[v8]]);
            v7[v8] = v9;
        }
        m_state.m_loopSelector = v0;

        std::array<double, 4> v10;
        for (std::size_t v11 = 0; v11 < 4; ++v11)
        {
            const bool v12 = (input.m_fraction[v11] == 0.0f);
            const bool v13 = (v12 == true);
            double v15;
            if (v13)
            {
                v15 = 1.0;
            }
            else
            {
                const double v14 = static_cast<double>(input.m_fraction[v11]);
                v15 = v14;
            }

            const double v16 = (input.m_writePhase * input.m_speed[v11]);
            const double v17 = (v7[v11] * v15);
            const double v18 = (v16 - v17);
            const double v19 = (input.m_writePhase - v7[v11]);
            const double v20 = (input.m_writePhase - v19);
            const double v21 = (v18 - v19);
            const double v22 = (v21 / v20);
            const double v23 = std::floor(v22);
            const double v24 = (v20 * v23);
            const double v25 = (v18 - v24);
            v10[v11] = v25;
        }
        m_output.m_loopPeriod = v7;

        std::array<float, 4> v26;
        for (std::size_t v27 = 0; v27 < 4; ++v27)
        {
            const double v28 = std::floor(v10[v27]);
            const double v29 = (v10[v27] - v28);
            const float v30 = static_cast<float>(v29);
            v26[v27] = v30;
        }
        m_output.m_readPhase = v10;
        m_output.m_relativeReadPosition = v26;

    }

};
