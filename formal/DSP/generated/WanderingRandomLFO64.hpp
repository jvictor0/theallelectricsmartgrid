#pragma once

// Generated from DSP.WanderingRandomLFO.process64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "WanderingRandomLFORefresh64.hpp"
#include "WanderingRandomLFOWrap64.hpp"
struct WanderingRandomLFO64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    static double Process(State& state)
    {
        const double v9 = state.m_cubic.m_a;
        const double v10 = state.m_cubic.m_b;
        const double v11 = state.m_cubic.m_c;
        const double v12 = state.m_cubic.m_d;
        const double v13 = state.m_phase;
        const double v19 = std::fma(v9, v13, v10);
        const double v20 = std::fma(v19, v13, v11);
        const double v21 = std::fma(v20, v13, v12);
        const size_t v38 = state.m_frameRemaining;
        const bool v41 = (v38 == 0);
        if (v41)
        {
            WanderingRandomLFORefresh64::Process(state);
        }
        else
        {
        }

        const double v42 = (state.m_phaseIncrement * 1.0);
        const double v43 = (state.m_phase + v42);
        const size_t v44 = (state.m_frameRemaining < 1 ? 0 : state.m_frameRemaining - 1);
        state.m_phase = v43;
        state.m_frameRemaining = v44;
        const double v58 = state.m_phase;
        const bool v64 = (v58 < 1.0);
        if (v64)
        {
        }
        else
        {
            WanderingRandomLFOWrap64::Process(state);
        }

        return v21;
    }

};
