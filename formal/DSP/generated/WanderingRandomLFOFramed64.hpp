#pragma once

// Generated from DSP.WanderingRandomLFO.processFramed64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "WanderingRandomLFORenewFrame64.hpp"
struct WanderingRandomLFOFramed64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    static double Process(State& state)
    {
        const size_t v22 = state.m_position;
        const bool v23 = (v22 < 32);
        double v30;
        if (v23)
        {
            const double v24 = static_cast<double>(v22);
            const double v25 = std::fma(state.m_slope, v24, state.m_left);
            const size_t v26 = CodegenInteger::Add<size_t>(v22, 1);
            state.m_position = v26;
            v30 = v25;
        }
        else
        {
            WanderingRandomLFORenewFrame64::Process(state);
            const double v27 = static_cast<double>(0);
            const double v28 = std::fma(state.m_slope, v27, state.m_left);
            const size_t v29 = CodegenInteger::Add<size_t>(0, 1);
            state.m_position = v29;
            v30 = v28;
        }

        return v30;
    }

};
