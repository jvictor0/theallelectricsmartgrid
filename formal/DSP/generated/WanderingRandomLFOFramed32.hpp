#pragma once

// Generated from DSP.WanderingRandomLFO.processFramed32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "WanderingRandomLFORenewFrame32.hpp"
struct WanderingRandomLFOFramed32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    static float Process(State& state)
    {
        const size_t v22 = state.m_position;
        const bool v23 = (v22 < 32);
        float v32;
        if (v23)
        {
            const double v24 = static_cast<double>(v22);
            const float v25 = static_cast<float>(v24);
            const float v26 = std::fma(state.m_slope, v25, state.m_left);
            const size_t v27 = CodegenInteger::Add<size_t>(v22, 1);
            state.m_position = v27;
            v32 = v26;
        }
        else
        {
            WanderingRandomLFORenewFrame32::Process(state);
            const double v28 = static_cast<double>(0);
            const float v29 = static_cast<float>(v28);
            const float v30 = std::fma(state.m_slope, v29, state.m_left);
            const size_t v31 = CodegenInteger::Add<size_t>(0, 1);
            state.m_position = v31;
            v32 = v30;
        }

        return v32;
    }

};
