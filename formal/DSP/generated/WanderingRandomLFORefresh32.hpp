#pragma once

// Generated from DSP.WanderingRandomLFO.refresh32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct WanderingRandomLFORefresh32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    [[gnu::noinline]] static void Process(State& state)
    {
        const std::uint64_t v0 = (state.m_speedSeed + static_cast<std::uint64_t>(11400714819323198485ULL));
        const std::uint64_t v1 = (v0 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
        const std::uint64_t v2 = (v0 ^ v1);
        const std::uint64_t v3 = (v2 * static_cast<std::uint64_t>(13787848793156543929ULL));
        const std::uint64_t v4 = (v3 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
        const std::uint64_t v5 = (v3 ^ v4);
        const std::uint64_t v6 = (v5 * static_cast<std::uint64_t>(10723151780598845931ULL));
        const std::uint64_t v7 = (v6 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
        const std::uint64_t v8 = (v6 ^ v7);
        const std::uint64_t v9 = (v8 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
        const double v10 = static_cast<double>(v9);
        const double v11 = (v10 + 0x10000000000000p-53);
        const double v12 = (v11 / 0x10000000000000p0);
        const double v13 = std::log(v12);
        const double v14 = (-v13);
        const double v15 = (v14 / state.m_lambdaSamples);
        const bool v16 = (v15 < 1.0);
        double v17;
        if (v16)
        {
            v17 = v15;
        }
        else
        {
            v17 = 1.0;
        }

        const double v18 = (0x10000000000000p-47 / 0x17700000000000p-37);
        const double v19 = (-v18);
        const double v20 = std::exp(v19);
        const double v21 = (1.0 - v20);
        const double v22 = (v21 * v17);
        const double v23 = std::fma(v20, state.m_phaseIncrement, v22);
        const size_t v24 = 32;
        state.m_phaseIncrement = v23;
        state.m_frameRemaining = v24;
        state.m_speedSeed = v0;
    }

};
