#pragma once

// Generated from DSP.Resynthesis.Runtime.unisonBase64. Do not edit.
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

struct UnisonBase64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Output
    {
        std::array<double, 4> m_lanes;
    };

    template<typename Input>
    static Output Evaluate([[maybe_unused]] const Input& input)
    {
        std::array<double, 4> v0;
        for (std::size_t v1 = 0; v1 < 4; ++v1)
        {
            const bool v2 = (input.m_count == 4);
            double v8;
            if (v2)
            {
                const double v3 = static_cast<double>(v1);
                const double v4 = (0x14000000000000p-50 * v3);
                const double v5 = (0x14000000000000p-47 + v4);
                const double v6 = (v5 / 0x12c00000000000p-42);
                const double v7 = MultiChannelNumber<double, 1>::Splat(v6).Exp2()[0];
                v8 = v7;
            }
            else
            {
                v8 = 0x107ae147ae147bp-52;
            }

            v0[v1] = v8;
        }

        Output output;
        output.m_lanes = v0;
        return output;
    }

};
