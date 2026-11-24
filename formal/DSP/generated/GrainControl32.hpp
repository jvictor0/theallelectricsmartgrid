#pragma once

// Generated from DSP.Resynthesis.Runtime.oscillatorControl32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
struct GrainControl32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename Input>
    static float Evaluate([[maybe_unused]] const Input& input)
    {
        const bool v0 = (input.m_gain == true);
        float v15;
        if (v0)
        {
            const bool v1 = (input.m_oscillator == 0);
            float v9;
            if (v1)
            {
                const float v2 = (0x800000p-22f * input.m_value);
                const float v3 = (v2 * input.m_value);
                const float v4 = (v3 / 0xc00000p-22f);
                const float v5 = (1.0f - v4);
                const float v6 = std::sqrt(v5);
                v9 = v6;
            }
            else
            {
                const float v7 = std::sqrt(0xc00000p-22f);
                const float v8 = (input.m_value / v7);
                v9 = v8;
            }

            v15 = v9;
        }
        else
        {
            const bool v10 = (input.m_oscillator == 0);
            float v14;
            if (v10)
            {
                v14 = 1.0f;
            }
            else
            {
                const bool v11 = (input.m_oscillator == 1);
                float v13;
                if (v11)
                {
                    v13 = input.m_value;
                }
                else
                {
                    const float v12 = (1.0f / input.m_value);
                    v13 = v12;
                }

                v14 = v13;
            }

            v15 = v14;
        }

        return v15;
    }

};
