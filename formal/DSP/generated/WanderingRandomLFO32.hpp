#pragma once

// Generated from DSP.WanderingRandomLFO.process32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "WanderingRandomLFORefresh32.hpp"
#include "WanderingRandomLFOWrap32.hpp"
struct WanderingRandomLFO32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    static float Process(State& state)
    {
        const float v9 = state.m_cubic.m_a;
        const float v10 = state.m_cubic.m_b;
        const float v11 = state.m_cubic.m_c;
        const float v12 = state.m_cubic.m_d;
        const double v13 = state.m_phase;
        const float v19 = static_cast<float>(v13);
        const float v20 = std::fma(v9, v19, v10);
        const float v21 = static_cast<float>(v13);
        const float v22 = std::fma(v20, v21, v11);
        const float v23 = static_cast<float>(v13);
        const float v24 = std::fma(v22, v23, v12);
        const size_t v41 = state.m_frameRemaining;
        const bool v44 = (v41 == 0);
        if (v44)
        {
            WanderingRandomLFORefresh32::Process(state);
        }
        else
        {
        }

        const double v45 = (state.m_phaseIncrement * 1.0);
        const double v46 = (state.m_phase + v45);
        const size_t v47 = (state.m_frameRemaining < 1 ? 0 : state.m_frameRemaining - 1);
        state.m_phase = v46;
        state.m_frameRemaining = v47;
        const double v61 = state.m_phase;
        const bool v67 = (v61 < 1.0);
        if (v67)
        {
        }
        else
        {
            WanderingRandomLFOWrap32::Process(state);
        }

        return v24;
    }

};
