#pragma once

// Generated from PhaseTime.InverseRuntime.writeFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct TimeWarpInverseWrite
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    static_assert(std::numeric_limits<std::size_t>::digits >= 64);

    template<typename State>
    static void Process(State& state, double input_m_phase, double input_m_time)
    {
        [[maybe_unused]] const auto v0 = state.LoadPrevious();
        [[maybe_unused]] const auto v1 = state.GridResolution();
        [[maybe_unused]] const auto v2 = state.LoadHighest();
        [[maybe_unused]] const auto v3 = state.CapacityMinusOne();
        double v4;
        double v5;
        if (v0.m_present)
        {
            v4 = v0.m_value.m_phase;
            v5 = v0.m_value.m_time;
        }
        else
        {
            v4 = input_m_phase;
            v5 = input_m_time;
        }

        const double v6 = (v4 * v1);
        const double v7 = (input_m_phase * v1);
        const bool v8 = (v0.m_present == true);
        double v9;
        double v10;
        if (v8)
        {
            v9 = v4;
            v10 = v5;
        }
        else
        {
            v9 = input_m_phase;
            v10 = input_m_time;
        }

        state.StorePreviousObservation(v9, v10);
        state.StoreObservation(input_m_phase, input_m_time);
        const bool v11 = (v0.m_present == true);
        if (v11)
        {
            const bool v12 = (v4 < input_m_phase);
            bool v13;
            if (v12)
            {
                v13 = true;
            }
            else
            {
                v13 = false;
            }

            const bool v14 = (v13 == true);
            if (v14)
            {
                [[maybe_unused]] const auto v15 = state.HasForward();
                const bool v16 = (v15 == true);
                if (v16)
                {
                    [[maybe_unused]] const auto v17 = state.IsDescending();
                    const bool v18 = (v17 == true);
                    if (v18)
                    {
                        const bool v19 = (v2.m_present == true);
                        bool v33;
                        if (v19)
                        {
                            std::int64_t v21;
                            if (v2.m_present)
                            {
                                v21 = v2.m_value;
                            }
                            else
                            {
                                const std::int64_t v20 = CodegenInteger::FromIndex(0);
                                v21 = v20;
                            }

                            const std::int64_t v22 = (v21 - v3);
                            const double v23 = std::floor(v6);
                            const std::int64_t v24 = static_cast<std::int64_t>(v23);
                            const bool v25 = (v22 <= v24);
                            bool v32;
                            if (v25)
                            {
                                const double v26 = std::floor(v6);
                                const std::int64_t v27 = static_cast<std::int64_t>(v26);
                                std::int64_t v29;
                                if (v2.m_present)
                                {
                                    v29 = v2.m_value;
                                }
                                else
                                {
                                    const std::int64_t v28 = CodegenInteger::FromIndex(0);
                                    v29 = v28;
                                }

                                const bool v30 = (v27 <= v29);
                                bool v31;
                                if (v30)
                                {
                                    v31 = true;
                                }
                                else
                                {
                                    v31 = false;
                                }

                                v32 = v31;
                            }
                            else
                            {
                                v32 = false;
                            }

                            v33 = v32;
                        }
                        else
                        {
                            v33 = false;
                        }

                        const double v34 = std::floor(v6);
                        const std::int64_t v35 = static_cast<std::int64_t>(v34);
                        const double v36 = static_cast<double>(v35);
                        const bool v37 = (v6 == v36);
                        const bool v38 = (v37 == true);
                        if (v38)
                        {
                        }
                        else
                        {
                            state.BeginWrite();
                            const double v39 = std::floor(v6);
                            const std::int64_t v40 = static_cast<std::int64_t>(v39);
                            [[maybe_unused]] const auto v41 = state.LoadBase(v40);
                            [[maybe_unused]] const auto v42 = state.LoadSeed();
                            bool v49;
                            if (v42.m_present)
                            {
                                const double v43 = std::floor(v6);
                                const std::int64_t v44 = static_cast<std::int64_t>(v43);
                                const bool v45 = (v42.m_value.m_index == v44);
                                bool v48;
                                if (v45)
                                {
                                    const bool v46 = (v6 < v42.m_value.m_phase);
                                    bool v47;
                                    if (v46)
                                    {
                                        v47 = true;
                                    }
                                    else
                                    {
                                        v47 = false;
                                    }

                                    v48 = v47;
                                }
                                else
                                {
                                    v48 = false;
                                }

                                v49 = v48;
                            }
                            else
                            {
                                v49 = false;
                            }

                            const bool v50 = !v33;
                            bool v52;
                            if (v50)
                            {
                                v52 = true;
                            }
                            else
                            {
                                const bool v51 = !v41.m_present;
                                v52 = v51;
                            }

                            bool v53;
                            if (v52)
                            {
                                v53 = true;
                            }
                            else
                            {
                                v53 = v49;
                            }

                            const bool v54 = (v53 == true);
                            if (v54)
                            {
                                [[maybe_unused]] const auto v55 = state.LoadSeed();
                                if (v55.m_present)
                                {
                                    state.ClearIndex(v55.m_value.m_index);
                                }
                                else
                                {
                                }

                                [[maybe_unused]] const auto v56 = state.LoadSeed();
                                if (v56.m_present)
                                {
                                    const double v57 = std::floor(v6);
                                    const std::int64_t v58 = static_cast<std::int64_t>(v57);
                                    [[maybe_unused]] const auto v59 = state.SameSlot(v56.m_value.m_index, v58);
                                    const bool v60 = (v59 == true);
                                    if (v60)
                                    {
                                        state.ClearSeed();
                                    }
                                    else
                                    {
                                    }

                                }
                                else
                                {
                                }

                                const double v61 = std::floor(v6);
                                const std::int64_t v62 = static_cast<std::int64_t>(v61);
                                state.PutCell(v62, v5, false, v5);
                                const double v63 = std::floor(v6);
                                const std::int64_t v64 = static_cast<std::int64_t>(v63);
                                state.StoreSeed(v6, v64);
                            }
                            else
                            {
                                const double v65 = std::floor(v6);
                                const std::int64_t v66 = static_cast<std::int64_t>(v65);
                                [[maybe_unused]] const auto v67 = state.LoadFallback(v66);
                                const bool v68 = (v67.m_present == true);
                                if (v68)
                                {
                                    if (v41.m_present)
                                    {
                                        const double v69 = std::floor(v6);
                                        const std::int64_t v70 = static_cast<std::int64_t>(v69);
                                        [[maybe_unused]] const auto v71 = state.LoadCut(v70);
                                        [[maybe_unused]] const auto v72 = state.LoadFrontier();
                                        bool v85;
                                        if (v71.m_present)
                                        {
                                            bool v84;
                                            if (v72.m_present)
                                            {
                                                const double v73 = std::floor(v6);
                                                const std::int64_t v74 = static_cast<std::int64_t>(v73);
                                                const std::int64_t v75 = CodegenInteger::FromIndex(1);
                                                const std::int64_t v76 = (v74 + v75);
                                                const double v77 = static_cast<double>(v76);
                                                const bool v78 = (v72.m_value.m_phase <= v77);
                                                bool v79;
                                                if (v78)
                                                {
                                                    v79 = true;
                                                }
                                                else
                                                {
                                                    v79 = false;
                                                }

                                                bool v83;
                                                if (v79)
                                                {
                                                    const bool v80 = (v6 < v71.m_value);
                                                    bool v81;
                                                    if (v80)
                                                    {
                                                        v81 = true;
                                                    }
                                                    else
                                                    {
                                                        v81 = false;
                                                    }

                                                    const bool v82 = !v81;
                                                    v83 = v82;
                                                }
                                                else
                                                {
                                                    v83 = false;
                                                }

                                                v84 = v83;
                                            }
                                            else
                                            {
                                                v84 = false;
                                            }

                                            v85 = v84;
                                        }
                                        else
                                        {
                                            v85 = false;
                                        }

                                        const bool v86 = (v85 == true);
                                        if (v86)
                                        {
                                            const double v87 = std::floor(v6);
                                            const std::int64_t v88 = static_cast<std::int64_t>(v87);
                                            const double v89 = (v41.m_value - v41.m_value);
                                            const float v90 = static_cast<float>(v89);
                                            state.PutEndDelta(v88, false, v90);
                                        }
                                        else
                                        {
                                            bool v93;
                                            if (v71.m_present)
                                            {
                                                const bool v91 = (v71.m_value < v6);
                                                bool v92;
                                                if (v91)
                                                {
                                                    v92 = true;
                                                }
                                                else
                                                {
                                                    v92 = false;
                                                }

                                                v93 = v92;
                                            }
                                            else
                                            {
                                                v93 = false;
                                            }

                                            const bool v94 = (v93 == true);
                                            if (v94)
                                            {
                                                state.IncrementIslands();
                                                const double v95 = std::floor(v6);
                                                const std::int64_t v96 = static_cast<std::int64_t>(v95);
                                                const double v97 = (v5 - v41.m_value);
                                                const float v98 = static_cast<float>(v97);
                                                state.PutReplacement(v96, v6, v98);
                                            }
                                            else
                                            {
                                                const double v99 = std::floor(v6);
                                                const std::int64_t v100 = static_cast<std::int64_t>(v99);
                                                const double v101 = (v5 - v41.m_value);
                                                const float v102 = static_cast<float>(v101);
                                                state.PutReplacement(v100, v6, v102);
                                            }

                                        }

                                    }
                                    else
                                    {
                                    }

                                }
                                else
                                {
                                }

                            }

                            state.StoreFrontier(v6, v5);
                            state.EndWrite();
                        }

                    }
                    else
                    {
                    }

                }
                else
                {
                    const bool v103 = (v2.m_present == true);
                    bool v117;
                    if (v103)
                    {
                        std::int64_t v105;
                        if (v2.m_present)
                        {
                            v105 = v2.m_value;
                        }
                        else
                        {
                            const std::int64_t v104 = CodegenInteger::FromIndex(0);
                            v105 = v104;
                        }

                        const std::int64_t v106 = (v105 - v3);
                        const double v107 = std::floor(v6);
                        const std::int64_t v108 = static_cast<std::int64_t>(v107);
                        const bool v109 = (v106 <= v108);
                        bool v116;
                        if (v109)
                        {
                            const double v110 = std::floor(v6);
                            const std::int64_t v111 = static_cast<std::int64_t>(v110);
                            std::int64_t v113;
                            if (v2.m_present)
                            {
                                v113 = v2.m_value;
                            }
                            else
                            {
                                const std::int64_t v112 = CodegenInteger::FromIndex(0);
                                v113 = v112;
                            }

                            const bool v114 = (v111 <= v113);
                            bool v115;
                            if (v114)
                            {
                                v115 = true;
                            }
                            else
                            {
                                v115 = false;
                            }

                            v116 = v115;
                        }
                        else
                        {
                            v116 = false;
                        }

                        v117 = v116;
                    }
                    else
                    {
                        v117 = false;
                    }

                    const double v118 = std::floor(v6);
                    const std::int64_t v119 = static_cast<std::int64_t>(v118);
                    const double v120 = static_cast<double>(v119);
                    const bool v121 = (v6 == v120);
                    const bool v122 = (v121 == true);
                    if (v122)
                    {
                    }
                    else
                    {
                        state.BeginWrite();
                        const double v123 = std::floor(v6);
                        const std::int64_t v124 = static_cast<std::int64_t>(v123);
                        [[maybe_unused]] const auto v125 = state.LoadBase(v124);
                        [[maybe_unused]] const auto v126 = state.LoadSeed();
                        bool v133;
                        if (v126.m_present)
                        {
                            const double v127 = std::floor(v6);
                            const std::int64_t v128 = static_cast<std::int64_t>(v127);
                            const bool v129 = (v126.m_value.m_index == v128);
                            bool v132;
                            if (v129)
                            {
                                const bool v130 = (v6 < v126.m_value.m_phase);
                                bool v131;
                                if (v130)
                                {
                                    v131 = true;
                                }
                                else
                                {
                                    v131 = false;
                                }

                                v132 = v131;
                            }
                            else
                            {
                                v132 = false;
                            }

                            v133 = v132;
                        }
                        else
                        {
                            v133 = false;
                        }

                        const bool v134 = !v117;
                        bool v136;
                        if (v134)
                        {
                            v136 = true;
                        }
                        else
                        {
                            const bool v135 = !v125.m_present;
                            v136 = v135;
                        }

                        bool v137;
                        if (v136)
                        {
                            v137 = true;
                        }
                        else
                        {
                            v137 = v133;
                        }

                        const bool v138 = (v137 == true);
                        if (v138)
                        {
                            [[maybe_unused]] const auto v139 = state.LoadSeed();
                            if (v139.m_present)
                            {
                                state.ClearIndex(v139.m_value.m_index);
                            }
                            else
                            {
                            }

                            [[maybe_unused]] const auto v140 = state.LoadSeed();
                            if (v140.m_present)
                            {
                                const double v141 = std::floor(v6);
                                const std::int64_t v142 = static_cast<std::int64_t>(v141);
                                [[maybe_unused]] const auto v143 = state.SameSlot(v140.m_value.m_index, v142);
                                const bool v144 = (v143 == true);
                                if (v144)
                                {
                                    state.ClearSeed();
                                }
                                else
                                {
                                }

                            }
                            else
                            {
                            }

                            const double v145 = std::floor(v6);
                            const std::int64_t v146 = static_cast<std::int64_t>(v145);
                            state.PutCell(v146, v5, false, v5);
                            const double v147 = std::floor(v6);
                            const std::int64_t v148 = static_cast<std::int64_t>(v147);
                            state.StoreSeed(v6, v148);
                        }
                        else
                        {
                            const double v149 = std::floor(v6);
                            const std::int64_t v150 = static_cast<std::int64_t>(v149);
                            [[maybe_unused]] const auto v151 = state.LoadFallback(v150);
                            const bool v152 = (v151.m_present == true);
                            if (v152)
                            {
                                if (v125.m_present)
                                {
                                    const double v153 = std::floor(v6);
                                    const std::int64_t v154 = static_cast<std::int64_t>(v153);
                                    [[maybe_unused]] const auto v155 = state.LoadCut(v154);
                                    [[maybe_unused]] const auto v156 = state.LoadFrontier();
                                    bool v169;
                                    if (v155.m_present)
                                    {
                                        bool v168;
                                        if (v156.m_present)
                                        {
                                            const double v157 = std::floor(v6);
                                            const std::int64_t v158 = static_cast<std::int64_t>(v157);
                                            const std::int64_t v159 = CodegenInteger::FromIndex(1);
                                            const std::int64_t v160 = (v158 + v159);
                                            const double v161 = static_cast<double>(v160);
                                            const bool v162 = (v156.m_value.m_phase <= v161);
                                            bool v163;
                                            if (v162)
                                            {
                                                v163 = true;
                                            }
                                            else
                                            {
                                                v163 = false;
                                            }

                                            bool v167;
                                            if (v163)
                                            {
                                                const bool v164 = (v6 < v155.m_value);
                                                bool v165;
                                                if (v164)
                                                {
                                                    v165 = true;
                                                }
                                                else
                                                {
                                                    v165 = false;
                                                }

                                                const bool v166 = !v165;
                                                v167 = v166;
                                            }
                                            else
                                            {
                                                v167 = false;
                                            }

                                            v168 = v167;
                                        }
                                        else
                                        {
                                            v168 = false;
                                        }

                                        v169 = v168;
                                    }
                                    else
                                    {
                                        v169 = false;
                                    }

                                    const bool v170 = (v169 == true);
                                    if (v170)
                                    {
                                        const double v171 = std::floor(v6);
                                        const std::int64_t v172 = static_cast<std::int64_t>(v171);
                                        const double v173 = (v125.m_value - v125.m_value);
                                        const float v174 = static_cast<float>(v173);
                                        state.PutEndDelta(v172, false, v174);
                                    }
                                    else
                                    {
                                        bool v177;
                                        if (v155.m_present)
                                        {
                                            const bool v175 = (v155.m_value < v6);
                                            bool v176;
                                            if (v175)
                                            {
                                                v176 = true;
                                            }
                                            else
                                            {
                                                v176 = false;
                                            }

                                            v177 = v176;
                                        }
                                        else
                                        {
                                            v177 = false;
                                        }

                                        const bool v178 = (v177 == true);
                                        if (v178)
                                        {
                                            state.IncrementIslands();
                                            const double v179 = std::floor(v6);
                                            const std::int64_t v180 = static_cast<std::int64_t>(v179);
                                            const double v181 = (v5 - v125.m_value);
                                            const float v182 = static_cast<float>(v181);
                                            state.PutReplacement(v180, v6, v182);
                                        }
                                        else
                                        {
                                            const double v183 = std::floor(v6);
                                            const std::int64_t v184 = static_cast<std::int64_t>(v183);
                                            const double v185 = (v5 - v125.m_value);
                                            const float v186 = static_cast<float>(v185);
                                            state.PutReplacement(v184, v6, v186);
                                        }

                                    }

                                }
                                else
                                {
                                }

                            }
                            else
                            {
                            }

                        }

                        state.StoreFrontier(v6, v5);
                        state.EndWrite();
                    }

                }

                const double v187 = std::ceil(v6);
                const std::int64_t v188 = static_cast<std::int64_t>(v187);
                const double v189 = std::floor(v7);
                const std::int64_t v190 = static_cast<std::int64_t>(v189);
                const bool v191 = (v188 <= v190);
                if (v191)
                {
                    const std::int64_t v192 = (v190 - v188);
                    const std::int64_t v193 = CodegenInteger::FromIndex(1);
                    const std::int64_t v194 = (v192 + v193);
                    const size_t v195 = static_cast<size_t>(v194);
                    [[maybe_unused]] std::int64_t loop_v196;
                    loop_v196 = v188;
                    for (std::size_t v197 = 0; v197 < v195; ++v197)
                    {
                        const std::int64_t v198 = (v190 - loop_v196);
                        const std::int64_t v199 = CodegenInteger::FromIndex(1);
                        const std::int64_t v200 = (v198 + v199);
                        const std::int64_t v201 = CodegenInteger::FromIndex(0);
                        const bool v202 = (v201 < v200);
                        bool v264;
                        std::int64_t v265;
                        if (v202)
                        {
                            const std::int64_t v203 = static_cast<std::int64_t>(8);
                            const bool v204 = (v200 <= v203);
                            std::int64_t v205;
                            if (v204)
                            {
                                v205 = v200;
                            }
                            else
                            {
                                v205 = v203;
                            }

                            const std::int64_t v206 = CodegenInteger::FromIndex(1);
                            const std::int64_t v207 = (v205 - v206);
                            const size_t v208 = static_cast<size_t>(v207);
                            const size_t v209 = (v208 + 1);
                            std::array<double, 8> v210;
                            for (std::size_t v211 = 0; v211 < v209; ++v211)
                            {
                                const std::int64_t v212 = static_cast<std::int64_t>(v211);
                                const std::int64_t v213 = (loop_v196 + v212);
                                const double v214 = static_cast<double>(v213);
                                const double v215 = (v214 - v6);
                                const double v216 = (v7 - v6);
                                const double v217 = (v215 / v216);
                                const double v218 = (input_m_time - v5);
                                const double v219 = (v217 * v218);
                                const double v220 = (v5 + v219);
                                v210[v211] = v220;
                            }

                            state.BeginWrite();
                            const double v221 = static_cast<double>(loop_v196);
                            const bool v222 = (v6 < v221);
                            bool v223;
                            if (v222)
                            {
                                v223 = true;
                            }
                            else
                            {
                                v223 = false;
                            }

                            const bool v224 = (v223 == true);
                            if (v224)
                            {
                                const std::int64_t v225 = CodegenInteger::FromIndex(1);
                                const std::int64_t v226 = (loop_v196 - v225);
                                [[maybe_unused]] const auto v227 = state.LoadFallback(v226);
                                const bool v228 = !v227.m_present;
                                const bool v229 = (v228 == true);
                                if (v229)
                                {
                                    const std::int64_t v230 = CodegenInteger::FromIndex(1);
                                    const std::int64_t v231 = (loop_v196 - v230);
                                    state.PutFallback(v231, v210[0]);
                                }
                                else
                                {
                                    const std::int64_t v232 = CodegenInteger::FromIndex(1);
                                    const std::int64_t v233 = (loop_v196 - v232);
                                    [[maybe_unused]] const auto v234 = state.LoadCut(v233);
                                    const bool v235 = (v234.m_present == true);
                                    if (v235)
                                    {
                                        const std::int64_t v236 = CodegenInteger::FromIndex(1);
                                        const std::int64_t v237 = (loop_v196 - v236);
                                        [[maybe_unused]] const auto v238 = state.LoadBase(v237);
                                        if (v238.m_present)
                                        {
                                            const std::int64_t v239 = CodegenInteger::FromIndex(1);
                                            const std::int64_t v240 = (loop_v196 - v239);
                                            const double v241 = (v210[0] - v238.m_value);
                                            const float v242 = static_cast<float>(v241);
                                            state.PutEndDelta(v240, true, v242);
                                        }
                                        else
                                        {
                                            const std::int64_t v243 = CodegenInteger::FromIndex(1);
                                            const std::int64_t v244 = (loop_v196 - v243);
                                            const double v245 = (v210[0] - v210[0]);
                                            const float v246 = static_cast<float>(v245);
                                            state.PutEndDelta(v244, false, v246);
                                        }

                                    }
                                    else
                                    {
                                    }

                                }

                            }
                            else
                            {
                            }

                            struct loop_v247_Type
                            {
                            };

                            [[maybe_unused]] loop_v247_Type loop_v247;
                            for (std::size_t v248 = 0; v248 < v208; ++v248)
                            {
                                const size_t v249 = (v248 + 1);
                                [[maybe_unused]] const auto v250 = state.LoadSeed();
                                if (v250.m_present)
                                {
                                    const std::int64_t v251 = static_cast<std::int64_t>(v248);
                                    const std::int64_t v252 = (loop_v196 + v251);
                                    [[maybe_unused]] const auto v253 = state.SameSlot(v250.m_value.m_index, v252);
                                    const bool v254 = (v253 == true);
                                    if (v254)
                                    {
                                        state.ClearSeed();
                                    }
                                    else
                                    {
                                    }

                                }
                                else
                                {
                                }

                                const std::int64_t v255 = static_cast<std::int64_t>(v248);
                                const std::int64_t v256 = (loop_v196 + v255);
                                state.PutCell(v256, v210[v248], true, v210[v249]);
                            }

                            const std::int64_t v257 = static_cast<std::int64_t>(v208);
                            const std::int64_t v258 = (loop_v196 + v257);
                            [[maybe_unused]] const auto v259 = state.LoadSeed();
                            if (v259.m_present)
                            {
                                [[maybe_unused]] const auto v260 = state.SameSlot(v259.m_value.m_index, v258);
                                const bool v261 = (v260 == true);
                                if (v261)
                                {
                                    state.ClearSeed();
                                }
                                else
                                {
                                }

                            }
                            else
                            {
                            }

                            state.PutCell(v258, v210[v208], false, v210[v208]);
                            const double v262 = static_cast<double>(v258);
                            state.StoreFrontier(v262, v210[v208]);
                            state.EndWrite();
                            const std::int64_t v263 = (loop_v196 + v205);
                            v264 = false;
                            v265 = v263;
                        }
                        else
                        {
                            v264 = true;
                            v265 = loop_v196;
                        }

                        loop_v196 = v265;
                        if (v264)
                        {
                            break;
                        }
                    }

                }
                else
                {
                }

                const bool v266 = (v2.m_present == true);
                std::int64_t v273;
                if (v266)
                {
                    std::int64_t v268;
                    if (v2.m_present)
                    {
                        v268 = v2.m_value;
                    }
                    else
                    {
                        const std::int64_t v267 = CodegenInteger::FromIndex(0);
                        v268 = v267;
                    }

                    const bool v269 = (v268 <= v190);
                    std::int64_t v272;
                    if (v269)
                    {
                        v272 = v190;
                    }
                    else
                    {
                        std::int64_t v271;
                        if (v2.m_present)
                        {
                            v271 = v2.m_value;
                        }
                        else
                        {
                            const std::int64_t v270 = CodegenInteger::FromIndex(0);
                            v271 = v270;
                        }

                        v272 = v271;
                    }

                    v273 = v272;
                }
                else
                {
                    v273 = v190;
                }

                state.StoreHighest(v273);
                const std::int64_t v274 = (v273 - v3);
                const double v275 = static_cast<double>(v274);
                const double v276 = (v275 / v1);
                state.StoreOldest(v276);
                state.BeginWrite();
                state.StoreFrontier(v7, input_m_time);
                state.EndWrite();
                state.StoreDirection(true, false);
            }
            else
            {
                const bool v277 = (input_m_phase < v4);
                bool v278;
                if (v277)
                {
                    v278 = true;
                }
                else
                {
                    v278 = false;
                }

                const bool v279 = (v278 == true);
                if (v279)
                {
                    state.MarkDescending();
                }
                else
                {
                }

            }

        }
        else
        {
        }

    }

};
