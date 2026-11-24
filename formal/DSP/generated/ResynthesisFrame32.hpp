#pragma once

// Generated from DSP.Resynthesis.FrameStorage.run32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct ResynthesisFrame32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State, typename Input>
    static void Process(State& state, [[maybe_unused]] const Input& input)
    {
        if (input.m_stage == 0u)
        {
            const size_t v0 = input.m_count;
            struct loop_v1_Type
            {
            };

            [[maybe_unused]] loop_v1_Type loop_v1;
            for (std::size_t v2 = 0; v2 < v0; ++v2)
            {
                state.PrepareSlew(v2);
            }

            state.FFT(true);
            const size_t v3 = input.m_count;
            struct loop_v4_Type
            {
            };

            [[maybe_unused]] loop_v4_Type loop_v4;
            for (std::size_t v5 = 0; v5 < v3; ++v5)
            {
                struct loop_v6_Type
                {
                };

                [[maybe_unused]] loop_v6_Type loop_v6;
                for (std::size_t v7 = 0; v7 < 2047; ++v7)
                {
                    [[maybe_unused]] const auto v8 = state.Measurement(v5, v7);
                    state.SetPrevious(v5, v7, {v8.m_magnitude, v8.m_phase});
                }

            }

            state.FFT(false);
            const size_t v9 = input.m_count;
            struct loop_v10_Type
            {
            };

            [[maybe_unused]] loop_v10_Type loop_v10;
            for (std::size_t v11 = 0; v11 < v9; ++v11)
            {
                [[maybe_unused]] const auto v12 = state.SlewCutoff(v11);
                const bool v13 = (0x800000p-24f <= v12);
                if (v13)
                {
                    struct loop_v14_Type
                    {
                    };

                    [[maybe_unused]] loop_v14_Type loop_v14;
                    for (std::size_t v15 = 0; v15 < 2047; ++v15)
                    {
                        [[maybe_unused]] const auto v16 = state.Measurement(v11, v15);
                        [[maybe_unused]] const auto v17 = state.PreviousPhase(v11, v15);
                        const size_t v18 = CodegenInteger::Add<size_t>(v15, 1);
                        const float v19 = static_cast<float>(v18);
                        const float v20 = (v19 / 0x800000p-11f);
                        const double v21 = static_cast<double>(v20);
                        const double v22 = static_cast<double>(v16.m_phase);
                        const double v23 = static_cast<double>(v17);
                        const double v24 = (v22 - v23);
                        const double v25 = (v21 * 0x10000000000000p-42);
                        const double v26 = (v24 - v25);
                        const double v27 = std::floor(v26);
                        const double v28 = (v26 - v27);
                        const bool v29 = (0x10000000000000p-53 < v28);
                        double v31;
                        if (v29)
                        {
                            const double v30 = (v28 - 1.0);
                            v31 = v30;
                        }
                        else
                        {
                            v31 = v28;
                        }

                        const double v32 = (v31 / 0x10000000000000p-42);
                        const double v33 = (v32 + v21);
                        state.SetCurrent(v11, v15, {v16.m_magnitude, v16.m_phase}, v33);
                        [[maybe_unused]] const auto v34 = state.Magnitude(v11, v15);
                        const bool v35 = (v34 < v16.m_magnitude);
                        float v39;
                        if (v35)
                        {
                            const float v36 = (v16.m_magnitude - v34);
                            const float v37 = (v36 * 1.0f);
                            const float v38 = (v34 + v37);
                            v39 = v38;
                        }
                        else
                        {
                            v39 = v16.m_magnitude;
                        }

                        state.SetMagnitude(v11, v15, v39);
                    }

                }
                else
                {
                    [[maybe_unused]] const auto v40 = state.SlewCoefficient(v12);
                    struct loop_v41_Type
                    {
                    };

                    [[maybe_unused]] loop_v41_Type loop_v41;
                    for (std::size_t v42 = 0; v42 < 2047; ++v42)
                    {
                        [[maybe_unused]] const auto v43 = state.Measurement(v11, v42);
                        [[maybe_unused]] const auto v44 = state.PreviousPhase(v11, v42);
                        const size_t v45 = CodegenInteger::Add<size_t>(v42, 1);
                        const float v46 = static_cast<float>(v45);
                        const float v47 = (v46 / 0x800000p-11f);
                        const double v48 = static_cast<double>(v47);
                        const double v49 = static_cast<double>(v43.m_phase);
                        const double v50 = static_cast<double>(v44);
                        const double v51 = (v49 - v50);
                        const double v52 = (v48 * 0x10000000000000p-42);
                        const double v53 = (v51 - v52);
                        const double v54 = std::floor(v53);
                        const double v55 = (v53 - v54);
                        const bool v56 = (0x10000000000000p-53 < v55);
                        double v58;
                        if (v56)
                        {
                            const double v57 = (v55 - 1.0);
                            v58 = v57;
                        }
                        else
                        {
                            v58 = v55;
                        }

                        const double v59 = (v58 / 0x10000000000000p-42);
                        const double v60 = (v59 + v48);
                        state.SetCurrent(v11, v42, {v43.m_magnitude, v43.m_phase}, v60);
                        [[maybe_unused]] const auto v61 = state.Magnitude(v11, v42);
                        const bool v62 = (v61 < v43.m_magnitude);
                        float v66;
                        if (v62)
                        {
                            const float v63 = (v43.m_magnitude - v61);
                            const float v64 = (v63 * v40);
                            const float v65 = (v61 + v64);
                            v66 = v65;
                        }
                        else
                        {
                            v66 = v43.m_magnitude;
                        }

                        state.SetMagnitude(v11, v42, v66);
                    }

                }

            }

            const size_t v67 = input.m_count;
            struct loop_v68_Type
            {
            };

            [[maybe_unused]] loop_v68_Type loop_v68;
            for (std::size_t v69 = 0; v69 < v67; ++v69)
            {
                state.AnalyzeGraph(v69);
            }

            state.Synthesize(input.m_count);
            state.IFFT();
            const size_t v70 = input.m_frameSize;
            struct loop_v71_Type
            {
            };

            [[maybe_unused]] loop_v71_Type loop_v71;
            for (std::size_t v72 = 0; v72 < v70; ++v72)
            {
                [[maybe_unused]] const auto v73 = state.Window(v72);
                const size_t v74 = input.m_count;
                struct loop_v75_Type
                {
                };

                [[maybe_unused]] loop_v75_Type loop_v75;
                for (std::size_t v76 = 0; v76 < v74; ++v76)
                {
                    [[maybe_unused]] const auto v77 = state.GetSample(v76, v72);
                    const float v78 = (v77 * v73);
                    state.PutSample(v76, v72, v78);
                }

            }

        }
        else
        {
            if (input.m_stage == 1u)
            {
                state.Synthesize(input.m_count);
            }
            else
            {
                if (input.m_stage == 2u)
                {
                    state.FFT(false);
                    const size_t v79 = input.m_count;
                    struct loop_v80_Type
                    {
                    };

                    [[maybe_unused]] loop_v80_Type loop_v80;
                    for (std::size_t v81 = 0; v81 < v79; ++v81)
                    {
                        [[maybe_unused]] const auto v82 = state.SlewCutoff(v81);
                        const bool v83 = (0x800000p-24f <= v82);
                        if (v83)
                        {
                            struct loop_v84_Type
                            {
                            };

                            [[maybe_unused]] loop_v84_Type loop_v84;
                            for (std::size_t v85 = 0; v85 < 2047; ++v85)
                            {
                                [[maybe_unused]] const auto v86 = state.Measurement(v81, v85);
                                [[maybe_unused]] const auto v87 = state.PreviousPhase(v81, v85);
                                const size_t v88 = CodegenInteger::Add<size_t>(v85, 1);
                                const float v89 = static_cast<float>(v88);
                                const float v90 = (v89 / 0x800000p-11f);
                                const double v91 = static_cast<double>(v90);
                                const double v92 = static_cast<double>(v86.m_phase);
                                const double v93 = static_cast<double>(v87);
                                const double v94 = (v92 - v93);
                                const double v95 = (v91 * 0x10000000000000p-42);
                                const double v96 = (v94 - v95);
                                const double v97 = std::floor(v96);
                                const double v98 = (v96 - v97);
                                const bool v99 = (0x10000000000000p-53 < v98);
                                double v101;
                                if (v99)
                                {
                                    const double v100 = (v98 - 1.0);
                                    v101 = v100;
                                }
                                else
                                {
                                    v101 = v98;
                                }

                                const double v102 = (v101 / 0x10000000000000p-42);
                                const double v103 = (v102 + v91);
                                state.SetCurrent(v81, v85, {v86.m_magnitude, v86.m_phase}, v103);
                                [[maybe_unused]] const auto v104 = state.Magnitude(v81, v85);
                                const bool v105 = (v104 < v86.m_magnitude);
                                float v109;
                                if (v105)
                                {
                                    const float v106 = (v86.m_magnitude - v104);
                                    const float v107 = (v106 * 1.0f);
                                    const float v108 = (v104 + v107);
                                    v109 = v108;
                                }
                                else
                                {
                                    v109 = v86.m_magnitude;
                                }

                                state.SetMagnitude(v81, v85, v109);
                            }

                        }
                        else
                        {
                            [[maybe_unused]] const auto v110 = state.SlewCoefficient(v82);
                            struct loop_v111_Type
                            {
                            };

                            [[maybe_unused]] loop_v111_Type loop_v111;
                            for (std::size_t v112 = 0; v112 < 2047; ++v112)
                            {
                                [[maybe_unused]] const auto v113 = state.Measurement(v81, v112);
                                [[maybe_unused]] const auto v114 = state.PreviousPhase(v81, v112);
                                const size_t v115 = CodegenInteger::Add<size_t>(v112, 1);
                                const float v116 = static_cast<float>(v115);
                                const float v117 = (v116 / 0x800000p-11f);
                                const double v118 = static_cast<double>(v117);
                                const double v119 = static_cast<double>(v113.m_phase);
                                const double v120 = static_cast<double>(v114);
                                const double v121 = (v119 - v120);
                                const double v122 = (v118 * 0x10000000000000p-42);
                                const double v123 = (v121 - v122);
                                const double v124 = std::floor(v123);
                                const double v125 = (v123 - v124);
                                const bool v126 = (0x10000000000000p-53 < v125);
                                double v128;
                                if (v126)
                                {
                                    const double v127 = (v125 - 1.0);
                                    v128 = v127;
                                }
                                else
                                {
                                    v128 = v125;
                                }

                                const double v129 = (v128 / 0x10000000000000p-42);
                                const double v130 = (v129 + v118);
                                state.SetCurrent(v81, v112, {v113.m_magnitude, v113.m_phase}, v130);
                                [[maybe_unused]] const auto v131 = state.Magnitude(v81, v112);
                                const bool v132 = (v131 < v113.m_magnitude);
                                float v136;
                                if (v132)
                                {
                                    const float v133 = (v113.m_magnitude - v131);
                                    const float v134 = (v133 * v110);
                                    const float v135 = (v131 + v134);
                                    v136 = v135;
                                }
                                else
                                {
                                    v136 = v113.m_magnitude;
                                }

                                state.SetMagnitude(v81, v112, v136);
                            }

                        }

                    }

                    const size_t v137 = input.m_count;
                    struct loop_v138_Type
                    {
                    };

                    [[maybe_unused]] loop_v138_Type loop_v138;
                    for (std::size_t v139 = 0; v139 < v137; ++v139)
                    {
                        state.AnalyzeGraph(v139);
                    }

                }
                else
                {
                    if (input.m_stage == 3u)
                    {
                        state.FFT(true);
                        const size_t v140 = input.m_count;
                        struct loop_v141_Type
                        {
                        };

                        [[maybe_unused]] loop_v141_Type loop_v141;
                        for (std::size_t v142 = 0; v142 < v140; ++v142)
                        {
                            struct loop_v143_Type
                            {
                            };

                            [[maybe_unused]] loop_v143_Type loop_v143;
                            for (std::size_t v144 = 0; v144 < 2047; ++v144)
                            {
                                [[maybe_unused]] const auto v145 = state.Measurement(v142, v144);
                                state.SetPrevious(v142, v144, {v145.m_magnitude, v145.m_phase});
                            }

                        }

                    }
                    else
                    {
                        if (input.m_stage == 4u)
                        {
                            const size_t v146 = input.m_count;
                            struct loop_v147_Type
                            {
                            };

                            [[maybe_unused]] loop_v147_Type loop_v147;
                            for (std::size_t v148 = 0; v148 < v146; ++v148)
                            {
                                struct loop_v149_Type
                                {
                                };

                                [[maybe_unused]] loop_v149_Type loop_v149;
                                for (std::size_t v150 = 0; v150 < 2047; ++v150)
                                {
                                    [[maybe_unused]] const auto v151 = state.Measurement(v148, v150);
                                    state.SetPrevious(v148, v150, {v151.m_magnitude, v151.m_phase});
                                }

                            }

                        }
                        else
                        {
                            if (input.m_stage == 5u)
                            {
                                const size_t v152 = input.m_count;
                                struct loop_v153_Type
                                {
                                };

                                [[maybe_unused]] loop_v153_Type loop_v153;
                                for (std::size_t v154 = 0; v154 < v152; ++v154)
                                {
                                    [[maybe_unused]] const auto v155 = state.SlewCutoff(v154);
                                    const bool v156 = (0x800000p-24f <= v155);
                                    if (v156)
                                    {
                                        struct loop_v157_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v157_Type loop_v157;
                                        for (std::size_t v158 = 0; v158 < 2047; ++v158)
                                        {
                                            [[maybe_unused]] const auto v159 = state.Measurement(v154, v158);
                                            [[maybe_unused]] const auto v160 = state.PreviousPhase(v154, v158);
                                            const size_t v161 = CodegenInteger::Add<size_t>(v158, 1);
                                            const float v162 = static_cast<float>(v161);
                                            const float v163 = (v162 / 0x800000p-11f);
                                            const double v164 = static_cast<double>(v163);
                                            const double v165 = static_cast<double>(v159.m_phase);
                                            const double v166 = static_cast<double>(v160);
                                            const double v167 = (v165 - v166);
                                            const double v168 = (v164 * 0x10000000000000p-42);
                                            const double v169 = (v167 - v168);
                                            const double v170 = std::floor(v169);
                                            const double v171 = (v169 - v170);
                                            const bool v172 = (0x10000000000000p-53 < v171);
                                            double v174;
                                            if (v172)
                                            {
                                                const double v173 = (v171 - 1.0);
                                                v174 = v173;
                                            }
                                            else
                                            {
                                                v174 = v171;
                                            }

                                            const double v175 = (v174 / 0x10000000000000p-42);
                                            const double v176 = (v175 + v164);
                                            state.SetCurrent(v154, v158, {v159.m_magnitude, v159.m_phase}, v176);
                                            [[maybe_unused]] const auto v177 = state.Magnitude(v154, v158);
                                            const bool v178 = (v177 < v159.m_magnitude);
                                            float v182;
                                            if (v178)
                                            {
                                                const float v179 = (v159.m_magnitude - v177);
                                                const float v180 = (v179 * 1.0f);
                                                const float v181 = (v177 + v180);
                                                v182 = v181;
                                            }
                                            else
                                            {
                                                v182 = v159.m_magnitude;
                                            }

                                            state.SetMagnitude(v154, v158, v182);
                                        }

                                    }
                                    else
                                    {
                                        [[maybe_unused]] const auto v183 = state.SlewCoefficient(v155);
                                        struct loop_v184_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v184_Type loop_v184;
                                        for (std::size_t v185 = 0; v185 < 2047; ++v185)
                                        {
                                            [[maybe_unused]] const auto v186 = state.Measurement(v154, v185);
                                            [[maybe_unused]] const auto v187 = state.PreviousPhase(v154, v185);
                                            const size_t v188 = CodegenInteger::Add<size_t>(v185, 1);
                                            const float v189 = static_cast<float>(v188);
                                            const float v190 = (v189 / 0x800000p-11f);
                                            const double v191 = static_cast<double>(v190);
                                            const double v192 = static_cast<double>(v186.m_phase);
                                            const double v193 = static_cast<double>(v187);
                                            const double v194 = (v192 - v193);
                                            const double v195 = (v191 * 0x10000000000000p-42);
                                            const double v196 = (v194 - v195);
                                            const double v197 = std::floor(v196);
                                            const double v198 = (v196 - v197);
                                            const bool v199 = (0x10000000000000p-53 < v198);
                                            double v201;
                                            if (v199)
                                            {
                                                const double v200 = (v198 - 1.0);
                                                v201 = v200;
                                            }
                                            else
                                            {
                                                v201 = v198;
                                            }

                                            const double v202 = (v201 / 0x10000000000000p-42);
                                            const double v203 = (v202 + v191);
                                            state.SetCurrent(v154, v185, {v186.m_magnitude, v186.m_phase}, v203);
                                            [[maybe_unused]] const auto v204 = state.Magnitude(v154, v185);
                                            const bool v205 = (v204 < v186.m_magnitude);
                                            float v209;
                                            if (v205)
                                            {
                                                const float v206 = (v186.m_magnitude - v204);
                                                const float v207 = (v206 * v183);
                                                const float v208 = (v204 + v207);
                                                v209 = v208;
                                            }
                                            else
                                            {
                                                v209 = v186.m_magnitude;
                                            }

                                            state.SetMagnitude(v154, v185, v209);
                                        }

                                    }

                                }

                            }
                            else
                            {
                                if (input.m_stage == 6u)
                                {
                                    const size_t v210 = input.m_count;
                                    struct loop_v211_Type
                                    {
                                    };

                                    [[maybe_unused]] loop_v211_Type loop_v211;
                                    for (std::size_t v212 = 0; v212 < v210; ++v212)
                                    {
                                        struct loop_v213_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v213_Type loop_v213;
                                        for (std::size_t v214 = 0; v214 < 2047; ++v214)
                                        {
                                            [[maybe_unused]] const auto v215 = state.Current(v212, v214);
                                            state.SetPrevious(v212, v214, {v215.m_magnitude, v215.m_phase});
                                        }

                                    }

                                }
                                else
                                {
                                    if (input.m_stage == 7u)
                                    {
                                        const size_t v216 = input.m_count;
                                        struct loop_v217_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v217_Type loop_v217;
                                        for (std::size_t v218 = 0; v218 < v216; ++v218)
                                        {
                                            [[maybe_unused]] const auto v219 = state.Current(v218, input.m_bin);
                                            [[maybe_unused]] const auto v220 = state.PreviousPhase(v218, input.m_bin);
                                            const size_t v221 = CodegenInteger::Add<size_t>(input.m_bin, 1);
                                            const float v222 = static_cast<float>(v221);
                                            const float v223 = (v222 / 0x800000p-11f);
                                            const double v224 = static_cast<double>(v223);
                                            const double v225 = static_cast<double>(v219.m_phase);
                                            const double v226 = static_cast<double>(v220);
                                            const double v227 = (v225 - v226);
                                            const double v228 = (v224 * 0x10000000000000p-42);
                                            const double v229 = (v227 - v228);
                                            const double v230 = std::floor(v229);
                                            const double v231 = (v229 - v230);
                                            const bool v232 = (0x10000000000000p-53 < v231);
                                            double v234;
                                            if (v232)
                                            {
                                                const double v233 = (v231 - 1.0);
                                                v234 = v233;
                                            }
                                            else
                                            {
                                                v234 = v231;
                                            }

                                            const double v235 = (v234 / 0x10000000000000p-42);
                                            const double v236 = (v235 + v224);
                                            state.SetCurrent(v218, input.m_bin, {v219.m_magnitude, v219.m_phase}, v236);
                                        }

                                    }
                                    else
                                    {
                                        if (input.m_stage == 8u)
                                        {
                                            const size_t v237 = input.m_count;
                                            struct loop_v238_Type
                                            {
                                            };

                                            [[maybe_unused]] loop_v238_Type loop_v238;
                                            for (std::size_t v239 = 0; v239 < v237; ++v239)
                                            {
                                                state.ClearAnalysisMagnitude(v239);
                                                state.ClearPreviousMagnitude(v239);
                                                state.ClearAnalysisPhase(v239);
                                                state.ClearPreviousPhase(v239);
                                                state.ClearOmega(v239);
                                                struct loop_v240_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v240_Type loop_v240;
                                                for (std::size_t v241 = 0; v241 < 3; ++v241)
                                                {
                                                    state.ClearOscillatorPhase(v239, v241);
                                                }

                                                state.ClearGates(v239);
                                                state.ClearOrder(v239);
                                            }

                                        }
                                        else
                                        {
                                            if (input.m_stage == 9u)
                                            {
                                                state.ClearAnalysisMagnitude(input.m_lane);
                                                state.ClearPreviousMagnitude(input.m_lane);
                                                state.ClearAnalysisPhase(input.m_lane);
                                                state.ClearPreviousPhase(input.m_lane);
                                                state.ClearOmega(input.m_lane);
                                                struct loop_v242_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v242_Type loop_v242;
                                                for (std::size_t v243 = 0; v243 < 3; ++v243)
                                                {
                                                    state.ClearOscillatorPhase(input.m_lane, v243);
                                                }

                                                state.ClearGates(input.m_lane);
                                                state.ClearOrder(input.m_lane);
                                                state.ClearMagnitude(input.m_lane);
                                                state.SetSlewCutoff(input.m_lane, 0.0f);
                                            }
                                            else
                                            {
                                                const size_t v244 = input.m_count;
                                                struct loop_v245_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v245_Type loop_v245;
                                                for (std::size_t v246 = 0; v246 < v244; ++v246)
                                                {
                                                    state.PrepareSlew(v246);
                                                }

                                                state.FFT(false);
                                                const size_t v247 = input.m_count;
                                                struct loop_v248_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v248_Type loop_v248;
                                                for (std::size_t v249 = 0; v249 < v247; ++v249)
                                                {
                                                    [[maybe_unused]] const auto v250 = state.SlewCutoff(v249);
                                                    const bool v251 = (0x800000p-24f <= v250);
                                                    if (v251)
                                                    {
                                                        struct loop_v252_Type
                                                        {
                                                        };

                                                        [[maybe_unused]] loop_v252_Type loop_v252;
                                                        for (std::size_t v253 = 0; v253 < 2047; ++v253)
                                                        {
                                                            [[maybe_unused]] const auto v254 = state.Measurement(v249, v253);
                                                            [[maybe_unused]] const auto v255 = state.PreviousPhase(v249, v253);
                                                            const size_t v256 = CodegenInteger::Add<size_t>(v253, 1);
                                                            const float v257 = static_cast<float>(v256);
                                                            const float v258 = (v257 / 0x800000p-11f);
                                                            const double v259 = static_cast<double>(v258);
                                                            const double v260 = static_cast<double>(v254.m_phase);
                                                            const double v261 = static_cast<double>(v255);
                                                            const double v262 = (v260 - v261);
                                                            const double v263 = (v259 * 0x10000000000000p-42);
                                                            const double v264 = (v262 - v263);
                                                            const double v265 = std::floor(v264);
                                                            const double v266 = (v264 - v265);
                                                            const bool v267 = (0x10000000000000p-53 < v266);
                                                            double v269;
                                                            if (v267)
                                                            {
                                                                const double v268 = (v266 - 1.0);
                                                                v269 = v268;
                                                            }
                                                            else
                                                            {
                                                                v269 = v266;
                                                            }

                                                            const double v270 = (v269 / 0x10000000000000p-42);
                                                            const double v271 = (v270 + v259);
                                                            state.SetCurrent(v249, v253, {v254.m_magnitude, v254.m_phase}, v271);
                                                            [[maybe_unused]] const auto v272 = state.Magnitude(v249, v253);
                                                            const bool v273 = (v272 < v254.m_magnitude);
                                                            float v277;
                                                            if (v273)
                                                            {
                                                                const float v274 = (v254.m_magnitude - v272);
                                                                const float v275 = (v274 * 1.0f);
                                                                const float v276 = (v272 + v275);
                                                                v277 = v276;
                                                            }
                                                            else
                                                            {
                                                                v277 = v254.m_magnitude;
                                                            }

                                                            state.SetMagnitude(v249, v253, v277);
                                                        }

                                                    }
                                                    else
                                                    {
                                                        [[maybe_unused]] const auto v278 = state.SlewCoefficient(v250);
                                                        struct loop_v279_Type
                                                        {
                                                        };

                                                        [[maybe_unused]] loop_v279_Type loop_v279;
                                                        for (std::size_t v280 = 0; v280 < 2047; ++v280)
                                                        {
                                                            [[maybe_unused]] const auto v281 = state.Measurement(v249, v280);
                                                            [[maybe_unused]] const auto v282 = state.PreviousPhase(v249, v280);
                                                            const size_t v283 = CodegenInteger::Add<size_t>(v280, 1);
                                                            const float v284 = static_cast<float>(v283);
                                                            const float v285 = (v284 / 0x800000p-11f);
                                                            const double v286 = static_cast<double>(v285);
                                                            const double v287 = static_cast<double>(v281.m_phase);
                                                            const double v288 = static_cast<double>(v282);
                                                            const double v289 = (v287 - v288);
                                                            const double v290 = (v286 * 0x10000000000000p-42);
                                                            const double v291 = (v289 - v290);
                                                            const double v292 = std::floor(v291);
                                                            const double v293 = (v291 - v292);
                                                            const bool v294 = (0x10000000000000p-53 < v293);
                                                            double v296;
                                                            if (v294)
                                                            {
                                                                const double v295 = (v293 - 1.0);
                                                                v296 = v295;
                                                            }
                                                            else
                                                            {
                                                                v296 = v293;
                                                            }

                                                            const double v297 = (v296 / 0x10000000000000p-42);
                                                            const double v298 = (v297 + v286);
                                                            state.SetCurrent(v249, v280, {v281.m_magnitude, v281.m_phase}, v298);
                                                            [[maybe_unused]] const auto v299 = state.Magnitude(v249, v280);
                                                            const bool v300 = (v299 < v281.m_magnitude);
                                                            float v304;
                                                            if (v300)
                                                            {
                                                                const float v301 = (v281.m_magnitude - v299);
                                                                const float v302 = (v301 * v278);
                                                                const float v303 = (v299 + v302);
                                                                v304 = v303;
                                                            }
                                                            else
                                                            {
                                                                v304 = v281.m_magnitude;
                                                            }

                                                            state.SetMagnitude(v249, v280, v304);
                                                        }

                                                    }

                                                }

                                                const size_t v305 = input.m_count;
                                                struct loop_v306_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v306_Type loop_v306;
                                                for (std::size_t v307 = 0; v307 < v305; ++v307)
                                                {
                                                    state.AnalyzeGraph(v307);
                                                }

                                                state.Synthesize(input.m_count);
                                                state.IFFT();
                                                const size_t v308 = input.m_frameSize;
                                                struct loop_v309_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v309_Type loop_v309;
                                                for (std::size_t v310 = 0; v310 < v308; ++v310)
                                                {
                                                    [[maybe_unused]] const auto v311 = state.Window(v310);
                                                    const size_t v312 = input.m_count;
                                                    struct loop_v313_Type
                                                    {
                                                    };

                                                    [[maybe_unused]] loop_v313_Type loop_v313;
                                                    for (std::size_t v314 = 0; v314 < v312; ++v314)
                                                    {
                                                        [[maybe_unused]] const auto v315 = state.GetSample(v314, v310);
                                                        const float v316 = (v315 * v311);
                                                        state.PutSample(v314, v310, v316);
                                                    }

                                                }

                                            }

                                        }

                                    }

                                }

                            }

                        }

                    }

                }

            }

        }

    }

};
