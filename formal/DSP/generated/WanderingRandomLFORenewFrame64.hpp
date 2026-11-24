#pragma once

// Generated from DSP.WanderingRandomLFO.renewFrame64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "WanderingRandomLFOWrapFramed64.hpp"
struct WanderingRandomLFORenewFrame64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    [[gnu::noinline]] static void Process(State& state)
    {
        [[maybe_unused]] size_t loop_v0;
        loop_v0 = 32;
        for (std::size_t v1 = 0; v1 < 32; ++v1)
        {
            const bool v2 = (loop_v0 == 0);
            bool v167;
            size_t v168;
            if (v2)
            {
                v167 = true;
                v168 = loop_v0;
            }
            else
            {
                const size_t v19 = state.m_endpoint.m_frameRemaining;
                const bool v26 = (v19 == 0);
                size_t v166;
                if (v26)
                {
                    const std::uint64_t v27 = (state.m_endpoint.m_speedSeed + static_cast<std::uint64_t>(11400714819323198485ULL));
                    const std::uint64_t v28 = (v27 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
                    const std::uint64_t v29 = (v27 ^ v28);
                    const std::uint64_t v30 = (v29 * static_cast<std::uint64_t>(13787848793156543929ULL));
                    const std::uint64_t v31 = (v30 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
                    const std::uint64_t v32 = (v30 ^ v31);
                    const std::uint64_t v33 = (v32 * static_cast<std::uint64_t>(10723151780598845931ULL));
                    const std::uint64_t v34 = (v33 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
                    const std::uint64_t v35 = (v33 ^ v34);
                    const std::uint64_t v36 = (v35 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
                    const double v37 = static_cast<double>(v36);
                    const double v38 = (v37 + 0x10000000000000p-53);
                    const double v39 = (v38 / 0x10000000000000p0);
                    const double v40 = std::log(v39);
                    const double v41 = (-v40);
                    const double v42 = (v41 / state.m_endpoint.m_lambdaSamples);
                    const bool v43 = (v42 < 1.0);
                    double v44;
                    if (v43)
                    {
                        v44 = v42;
                    }
                    else
                    {
                        v44 = 1.0;
                    }

                    const double v45 = (0x10000000000000p-47 / 0x17700000000000p-37);
                    const double v46 = (-v45);
                    const double v47 = std::exp(v46);
                    const double v48 = (1.0 - v47);
                    const double v49 = (v48 * v44);
                    const double v50 = std::fma(v47, state.m_endpoint.m_phaseIncrement, v49);
                    const size_t v51 = 32;
                    state.m_endpoint.m_phaseIncrement = v50;
                    state.m_endpoint.m_frameRemaining = v51;
                    state.m_endpoint.m_speedSeed = v27;
                    const size_t v68 = state.m_endpoint.m_frameRemaining;
                    const bool v75 = (loop_v0 <= v68);
                    size_t v76;
                    if (v75)
                    {
                        v76 = loop_v0;
                    }
                    else
                    {
                        v76 = v68;
                    }

                    const double v77 = static_cast<double>(v76);
                    const double v78 = (state.m_endpoint.m_phaseIncrement * v77);
                    const double v79 = (state.m_endpoint.m_phase + v78);
                    const size_t v80 = (state.m_endpoint.m_frameRemaining < v76 ? 0 : state.m_endpoint.m_frameRemaining - v76);
                    state.m_endpoint.m_phase = v79;
                    state.m_endpoint.m_frameRemaining = v80;
                    struct loop_v81_Type
                    {
                    };

                    [[maybe_unused]] loop_v81_Type loop_v81;
                    for (std::size_t v82 = 0; v82 < v76; ++v82)
                    {
                        const double v96 = state.m_endpoint.m_phase;
                        const bool v106 = (v96 < 1.0);
                        bool v107;
                        if (v106)
                        {
                            v107 = true;
                        }
                        else
                        {
                            WanderingRandomLFOWrapFramed64::Process(state);
                            v107 = false;
                        }

                        if (v107)
                        {
                            break;
                        }
                    }

                    const size_t v108 = (loop_v0 < v76 ? 0 : loop_v0 - v76);
                    v166 = v108;
                }
                else
                {
                    const size_t v125 = state.m_endpoint.m_frameRemaining;
                    const bool v132 = (loop_v0 <= v125);
                    size_t v133;
                    if (v132)
                    {
                        v133 = loop_v0;
                    }
                    else
                    {
                        v133 = v125;
                    }

                    const double v134 = static_cast<double>(v133);
                    const double v135 = (state.m_endpoint.m_phaseIncrement * v134);
                    const double v136 = (state.m_endpoint.m_phase + v135);
                    const size_t v137 = (state.m_endpoint.m_frameRemaining < v133 ? 0 : state.m_endpoint.m_frameRemaining - v133);
                    state.m_endpoint.m_phase = v136;
                    state.m_endpoint.m_frameRemaining = v137;
                    struct loop_v138_Type
                    {
                    };

                    [[maybe_unused]] loop_v138_Type loop_v138;
                    for (std::size_t v139 = 0; v139 < v133; ++v139)
                    {
                        const double v153 = state.m_endpoint.m_phase;
                        const bool v163 = (v153 < 1.0);
                        bool v164;
                        if (v163)
                        {
                            v164 = true;
                        }
                        else
                        {
                            WanderingRandomLFOWrapFramed64::Process(state);
                            v164 = false;
                        }

                        if (v164)
                        {
                            break;
                        }
                    }

                    const size_t v165 = (loop_v0 < v133 ? 0 : loop_v0 - v133);
                    v166 = v165;
                }

                v167 = false;
                v168 = v166;
            }

            loop_v0 = v168;
            if (v167)
            {
                break;
            }
        }

        const double v169 = std::fma(state.m_endpoint.m_cubic.m_a, state.m_endpoint.m_phase, state.m_endpoint.m_cubic.m_b);
        const double v170 = std::fma(v169, state.m_endpoint.m_phase, state.m_endpoint.m_cubic.m_c);
        const double v171 = std::fma(v170, state.m_endpoint.m_phase, state.m_endpoint.m_cubic.m_d);
        const double v172 = (v171 - state.m_right);
        const double v173 = (v172 / 0x10000000000000p-47);
        const double v174 = state.m_right;
        const size_t v175 = 0;
        state.m_left = v174;
        state.m_right = v171;
        state.m_slope = v173;
        state.m_position = v175;
    }

};
