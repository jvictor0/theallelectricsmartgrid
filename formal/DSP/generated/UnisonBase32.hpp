#pragma once

// Generated from DSP.Resynthesis.Runtime.unisonBase32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"

struct UnisonBase32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Output
    {
        std::array<float, 4> m_lanes;
    };

    template<typename Input>
    static Output Evaluate([[maybe_unused]] const Input& input)
    {
        std::array<float, 4> v0;
        for (std::size_t v1 = 0; v1 < 4; ++v1)
        {
            const bool v2 = (input.m_count == 4);
            float v8;
            if (v2)
            {
                const float v3 = static_cast<float>(v1);
                const float v4 = (0xa00000p-21f * v3);
                const float v5 = (0xa00000p-18f + v4);
                const float v6 = (v5 / 0x960000p-13f);
                const float v7 = MultiChannelNumber<float, 1>::Splat(v6).Exp2()[0];
                v8 = v7;
            }
            else
            {
                v8 = 0x83d70ap-23f;
            }

            v0[v1] = v8;
        }

        Output output;
        output.m_lanes = v0;
        return output;
    }

};
