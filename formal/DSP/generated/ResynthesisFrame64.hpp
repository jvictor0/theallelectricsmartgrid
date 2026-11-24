#pragma once

// Generated from DSP.Resynthesis.FrameStorage.run64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct ResynthesisFrame64
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
                const bool v13 = (0x10000000000000p-53 <= v12);
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
                        const double v19 = static_cast<double>(v18);
                        const double v20 = (v19 / 0x10000000000000p-40);
                        const double v21 = (v16.m_phase - v17);
                        const double v22 = (v20 * 0x10000000000000p-42);
                        const double v23 = (v21 - v22);
                        const double v24 = std::floor(v23);
                        const double v25 = (v23 - v24);
                        const bool v26 = (0x10000000000000p-53 < v25);
                        double v28;
                        if (v26)
                        {
                            const double v27 = (v25 - 1.0);
                            v28 = v27;
                        }
                        else
                        {
                            v28 = v25;
                        }

                        const double v29 = (v28 / 0x10000000000000p-42);
                        const double v30 = (v29 + v20);
                        state.SetCurrent(v11, v15, {v16.m_magnitude, v16.m_phase}, v30);
                        [[maybe_unused]] const auto v31 = state.Magnitude(v11, v15);
                        const bool v32 = (v31 < v16.m_magnitude);
                        double v36;
                        if (v32)
                        {
                            const double v33 = (v16.m_magnitude - v31);
                            const double v34 = (v33 * 1.0);
                            const double v35 = (v31 + v34);
                            v36 = v35;
                        }
                        else
                        {
                            v36 = v16.m_magnitude;
                        }

                        state.SetMagnitude(v11, v15, v36);
                    }

                }
                else
                {
                    [[maybe_unused]] const auto v37 = state.SlewCoefficient(v12);
                    struct loop_v38_Type
                    {
                    };

                    [[maybe_unused]] loop_v38_Type loop_v38;
                    for (std::size_t v39 = 0; v39 < 2047; ++v39)
                    {
                        [[maybe_unused]] const auto v40 = state.Measurement(v11, v39);
                        [[maybe_unused]] const auto v41 = state.PreviousPhase(v11, v39);
                        const size_t v42 = CodegenInteger::Add<size_t>(v39, 1);
                        const double v43 = static_cast<double>(v42);
                        const double v44 = (v43 / 0x10000000000000p-40);
                        const double v45 = (v40.m_phase - v41);
                        const double v46 = (v44 * 0x10000000000000p-42);
                        const double v47 = (v45 - v46);
                        const double v48 = std::floor(v47);
                        const double v49 = (v47 - v48);
                        const bool v50 = (0x10000000000000p-53 < v49);
                        double v52;
                        if (v50)
                        {
                            const double v51 = (v49 - 1.0);
                            v52 = v51;
                        }
                        else
                        {
                            v52 = v49;
                        }

                        const double v53 = (v52 / 0x10000000000000p-42);
                        const double v54 = (v53 + v44);
                        state.SetCurrent(v11, v39, {v40.m_magnitude, v40.m_phase}, v54);
                        [[maybe_unused]] const auto v55 = state.Magnitude(v11, v39);
                        const bool v56 = (v55 < v40.m_magnitude);
                        double v60;
                        if (v56)
                        {
                            const double v57 = (v40.m_magnitude - v55);
                            const double v58 = (v57 * v37);
                            const double v59 = (v55 + v58);
                            v60 = v59;
                        }
                        else
                        {
                            v60 = v40.m_magnitude;
                        }

                        state.SetMagnitude(v11, v39, v60);
                    }

                }

            }

            const size_t v61 = input.m_count;
            struct loop_v62_Type
            {
            };

            [[maybe_unused]] loop_v62_Type loop_v62;
            for (std::size_t v63 = 0; v63 < v61; ++v63)
            {
                state.AnalyzeGraph(v63);
            }

            state.Synthesize(input.m_count);
            state.IFFT();
            const size_t v64 = input.m_frameSize;
            struct loop_v65_Type
            {
            };

            [[maybe_unused]] loop_v65_Type loop_v65;
            for (std::size_t v66 = 0; v66 < v64; ++v66)
            {
                [[maybe_unused]] const auto v67 = state.Window(v66);
                const size_t v68 = input.m_count;
                struct loop_v69_Type
                {
                };

                [[maybe_unused]] loop_v69_Type loop_v69;
                for (std::size_t v70 = 0; v70 < v68; ++v70)
                {
                    [[maybe_unused]] const auto v71 = state.GetSample(v70, v66);
                    const double v72 = (v71 * v67);
                    state.PutSample(v70, v66, v72);
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
                    const size_t v73 = input.m_count;
                    struct loop_v74_Type
                    {
                    };

                    [[maybe_unused]] loop_v74_Type loop_v74;
                    for (std::size_t v75 = 0; v75 < v73; ++v75)
                    {
                        [[maybe_unused]] const auto v76 = state.SlewCutoff(v75);
                        const bool v77 = (0x10000000000000p-53 <= v76);
                        if (v77)
                        {
                            struct loop_v78_Type
                            {
                            };

                            [[maybe_unused]] loop_v78_Type loop_v78;
                            for (std::size_t v79 = 0; v79 < 2047; ++v79)
                            {
                                [[maybe_unused]] const auto v80 = state.Measurement(v75, v79);
                                [[maybe_unused]] const auto v81 = state.PreviousPhase(v75, v79);
                                const size_t v82 = CodegenInteger::Add<size_t>(v79, 1);
                                const double v83 = static_cast<double>(v82);
                                const double v84 = (v83 / 0x10000000000000p-40);
                                const double v85 = (v80.m_phase - v81);
                                const double v86 = (v84 * 0x10000000000000p-42);
                                const double v87 = (v85 - v86);
                                const double v88 = std::floor(v87);
                                const double v89 = (v87 - v88);
                                const bool v90 = (0x10000000000000p-53 < v89);
                                double v92;
                                if (v90)
                                {
                                    const double v91 = (v89 - 1.0);
                                    v92 = v91;
                                }
                                else
                                {
                                    v92 = v89;
                                }

                                const double v93 = (v92 / 0x10000000000000p-42);
                                const double v94 = (v93 + v84);
                                state.SetCurrent(v75, v79, {v80.m_magnitude, v80.m_phase}, v94);
                                [[maybe_unused]] const auto v95 = state.Magnitude(v75, v79);
                                const bool v96 = (v95 < v80.m_magnitude);
                                double v100;
                                if (v96)
                                {
                                    const double v97 = (v80.m_magnitude - v95);
                                    const double v98 = (v97 * 1.0);
                                    const double v99 = (v95 + v98);
                                    v100 = v99;
                                }
                                else
                                {
                                    v100 = v80.m_magnitude;
                                }

                                state.SetMagnitude(v75, v79, v100);
                            }

                        }
                        else
                        {
                            [[maybe_unused]] const auto v101 = state.SlewCoefficient(v76);
                            struct loop_v102_Type
                            {
                            };

                            [[maybe_unused]] loop_v102_Type loop_v102;
                            for (std::size_t v103 = 0; v103 < 2047; ++v103)
                            {
                                [[maybe_unused]] const auto v104 = state.Measurement(v75, v103);
                                [[maybe_unused]] const auto v105 = state.PreviousPhase(v75, v103);
                                const size_t v106 = CodegenInteger::Add<size_t>(v103, 1);
                                const double v107 = static_cast<double>(v106);
                                const double v108 = (v107 / 0x10000000000000p-40);
                                const double v109 = (v104.m_phase - v105);
                                const double v110 = (v108 * 0x10000000000000p-42);
                                const double v111 = (v109 - v110);
                                const double v112 = std::floor(v111);
                                const double v113 = (v111 - v112);
                                const bool v114 = (0x10000000000000p-53 < v113);
                                double v116;
                                if (v114)
                                {
                                    const double v115 = (v113 - 1.0);
                                    v116 = v115;
                                }
                                else
                                {
                                    v116 = v113;
                                }

                                const double v117 = (v116 / 0x10000000000000p-42);
                                const double v118 = (v117 + v108);
                                state.SetCurrent(v75, v103, {v104.m_magnitude, v104.m_phase}, v118);
                                [[maybe_unused]] const auto v119 = state.Magnitude(v75, v103);
                                const bool v120 = (v119 < v104.m_magnitude);
                                double v124;
                                if (v120)
                                {
                                    const double v121 = (v104.m_magnitude - v119);
                                    const double v122 = (v121 * v101);
                                    const double v123 = (v119 + v122);
                                    v124 = v123;
                                }
                                else
                                {
                                    v124 = v104.m_magnitude;
                                }

                                state.SetMagnitude(v75, v103, v124);
                            }

                        }

                    }

                    const size_t v125 = input.m_count;
                    struct loop_v126_Type
                    {
                    };

                    [[maybe_unused]] loop_v126_Type loop_v126;
                    for (std::size_t v127 = 0; v127 < v125; ++v127)
                    {
                        state.AnalyzeGraph(v127);
                    }

                }
                else
                {
                    if (input.m_stage == 3u)
                    {
                        state.FFT(true);
                        const size_t v128 = input.m_count;
                        struct loop_v129_Type
                        {
                        };

                        [[maybe_unused]] loop_v129_Type loop_v129;
                        for (std::size_t v130 = 0; v130 < v128; ++v130)
                        {
                            struct loop_v131_Type
                            {
                            };

                            [[maybe_unused]] loop_v131_Type loop_v131;
                            for (std::size_t v132 = 0; v132 < 2047; ++v132)
                            {
                                [[maybe_unused]] const auto v133 = state.Measurement(v130, v132);
                                state.SetPrevious(v130, v132, {v133.m_magnitude, v133.m_phase});
                            }

                        }

                    }
                    else
                    {
                        if (input.m_stage == 4u)
                        {
                            const size_t v134 = input.m_count;
                            struct loop_v135_Type
                            {
                            };

                            [[maybe_unused]] loop_v135_Type loop_v135;
                            for (std::size_t v136 = 0; v136 < v134; ++v136)
                            {
                                struct loop_v137_Type
                                {
                                };

                                [[maybe_unused]] loop_v137_Type loop_v137;
                                for (std::size_t v138 = 0; v138 < 2047; ++v138)
                                {
                                    [[maybe_unused]] const auto v139 = state.Measurement(v136, v138);
                                    state.SetPrevious(v136, v138, {v139.m_magnitude, v139.m_phase});
                                }

                            }

                        }
                        else
                        {
                            if (input.m_stage == 5u)
                            {
                                const size_t v140 = input.m_count;
                                struct loop_v141_Type
                                {
                                };

                                [[maybe_unused]] loop_v141_Type loop_v141;
                                for (std::size_t v142 = 0; v142 < v140; ++v142)
                                {
                                    [[maybe_unused]] const auto v143 = state.SlewCutoff(v142);
                                    const bool v144 = (0x10000000000000p-53 <= v143);
                                    if (v144)
                                    {
                                        struct loop_v145_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v145_Type loop_v145;
                                        for (std::size_t v146 = 0; v146 < 2047; ++v146)
                                        {
                                            [[maybe_unused]] const auto v147 = state.Measurement(v142, v146);
                                            [[maybe_unused]] const auto v148 = state.PreviousPhase(v142, v146);
                                            const size_t v149 = CodegenInteger::Add<size_t>(v146, 1);
                                            const double v150 = static_cast<double>(v149);
                                            const double v151 = (v150 / 0x10000000000000p-40);
                                            const double v152 = (v147.m_phase - v148);
                                            const double v153 = (v151 * 0x10000000000000p-42);
                                            const double v154 = (v152 - v153);
                                            const double v155 = std::floor(v154);
                                            const double v156 = (v154 - v155);
                                            const bool v157 = (0x10000000000000p-53 < v156);
                                            double v159;
                                            if (v157)
                                            {
                                                const double v158 = (v156 - 1.0);
                                                v159 = v158;
                                            }
                                            else
                                            {
                                                v159 = v156;
                                            }

                                            const double v160 = (v159 / 0x10000000000000p-42);
                                            const double v161 = (v160 + v151);
                                            state.SetCurrent(v142, v146, {v147.m_magnitude, v147.m_phase}, v161);
                                            [[maybe_unused]] const auto v162 = state.Magnitude(v142, v146);
                                            const bool v163 = (v162 < v147.m_magnitude);
                                            double v167;
                                            if (v163)
                                            {
                                                const double v164 = (v147.m_magnitude - v162);
                                                const double v165 = (v164 * 1.0);
                                                const double v166 = (v162 + v165);
                                                v167 = v166;
                                            }
                                            else
                                            {
                                                v167 = v147.m_magnitude;
                                            }

                                            state.SetMagnitude(v142, v146, v167);
                                        }

                                    }
                                    else
                                    {
                                        [[maybe_unused]] const auto v168 = state.SlewCoefficient(v143);
                                        struct loop_v169_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v169_Type loop_v169;
                                        for (std::size_t v170 = 0; v170 < 2047; ++v170)
                                        {
                                            [[maybe_unused]] const auto v171 = state.Measurement(v142, v170);
                                            [[maybe_unused]] const auto v172 = state.PreviousPhase(v142, v170);
                                            const size_t v173 = CodegenInteger::Add<size_t>(v170, 1);
                                            const double v174 = static_cast<double>(v173);
                                            const double v175 = (v174 / 0x10000000000000p-40);
                                            const double v176 = (v171.m_phase - v172);
                                            const double v177 = (v175 * 0x10000000000000p-42);
                                            const double v178 = (v176 - v177);
                                            const double v179 = std::floor(v178);
                                            const double v180 = (v178 - v179);
                                            const bool v181 = (0x10000000000000p-53 < v180);
                                            double v183;
                                            if (v181)
                                            {
                                                const double v182 = (v180 - 1.0);
                                                v183 = v182;
                                            }
                                            else
                                            {
                                                v183 = v180;
                                            }

                                            const double v184 = (v183 / 0x10000000000000p-42);
                                            const double v185 = (v184 + v175);
                                            state.SetCurrent(v142, v170, {v171.m_magnitude, v171.m_phase}, v185);
                                            [[maybe_unused]] const auto v186 = state.Magnitude(v142, v170);
                                            const bool v187 = (v186 < v171.m_magnitude);
                                            double v191;
                                            if (v187)
                                            {
                                                const double v188 = (v171.m_magnitude - v186);
                                                const double v189 = (v188 * v168);
                                                const double v190 = (v186 + v189);
                                                v191 = v190;
                                            }
                                            else
                                            {
                                                v191 = v171.m_magnitude;
                                            }

                                            state.SetMagnitude(v142, v170, v191);
                                        }

                                    }

                                }

                            }
                            else
                            {
                                if (input.m_stage == 6u)
                                {
                                    const size_t v192 = input.m_count;
                                    struct loop_v193_Type
                                    {
                                    };

                                    [[maybe_unused]] loop_v193_Type loop_v193;
                                    for (std::size_t v194 = 0; v194 < v192; ++v194)
                                    {
                                        struct loop_v195_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v195_Type loop_v195;
                                        for (std::size_t v196 = 0; v196 < 2047; ++v196)
                                        {
                                            [[maybe_unused]] const auto v197 = state.Current(v194, v196);
                                            state.SetPrevious(v194, v196, {v197.m_magnitude, v197.m_phase});
                                        }

                                    }

                                }
                                else
                                {
                                    if (input.m_stage == 7u)
                                    {
                                        const size_t v198 = input.m_count;
                                        struct loop_v199_Type
                                        {
                                        };

                                        [[maybe_unused]] loop_v199_Type loop_v199;
                                        for (std::size_t v200 = 0; v200 < v198; ++v200)
                                        {
                                            [[maybe_unused]] const auto v201 = state.Current(v200, input.m_bin);
                                            [[maybe_unused]] const auto v202 = state.PreviousPhase(v200, input.m_bin);
                                            const size_t v203 = CodegenInteger::Add<size_t>(input.m_bin, 1);
                                            const double v204 = static_cast<double>(v203);
                                            const double v205 = (v204 / 0x10000000000000p-40);
                                            const double v206 = (v201.m_phase - v202);
                                            const double v207 = (v205 * 0x10000000000000p-42);
                                            const double v208 = (v206 - v207);
                                            const double v209 = std::floor(v208);
                                            const double v210 = (v208 - v209);
                                            const bool v211 = (0x10000000000000p-53 < v210);
                                            double v213;
                                            if (v211)
                                            {
                                                const double v212 = (v210 - 1.0);
                                                v213 = v212;
                                            }
                                            else
                                            {
                                                v213 = v210;
                                            }

                                            const double v214 = (v213 / 0x10000000000000p-42);
                                            const double v215 = (v214 + v205);
                                            state.SetCurrent(v200, input.m_bin, {v201.m_magnitude, v201.m_phase}, v215);
                                        }

                                    }
                                    else
                                    {
                                        if (input.m_stage == 8u)
                                        {
                                            const size_t v216 = input.m_count;
                                            struct loop_v217_Type
                                            {
                                            };

                                            [[maybe_unused]] loop_v217_Type loop_v217;
                                            for (std::size_t v218 = 0; v218 < v216; ++v218)
                                            {
                                                state.ClearAnalysisMagnitude(v218);
                                                state.ClearPreviousMagnitude(v218);
                                                state.ClearAnalysisPhase(v218);
                                                state.ClearPreviousPhase(v218);
                                                state.ClearOmega(v218);
                                                struct loop_v219_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v219_Type loop_v219;
                                                for (std::size_t v220 = 0; v220 < 3; ++v220)
                                                {
                                                    state.ClearOscillatorPhase(v218, v220);
                                                }

                                                state.ClearGates(v218);
                                                state.ClearOrder(v218);
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
                                                struct loop_v221_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v221_Type loop_v221;
                                                for (std::size_t v222 = 0; v222 < 3; ++v222)
                                                {
                                                    state.ClearOscillatorPhase(input.m_lane, v222);
                                                }

                                                state.ClearGates(input.m_lane);
                                                state.ClearOrder(input.m_lane);
                                                state.ClearMagnitude(input.m_lane);
                                                state.SetSlewCutoff(input.m_lane, 0.0);
                                            }
                                            else
                                            {
                                                const size_t v223 = input.m_count;
                                                struct loop_v224_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v224_Type loop_v224;
                                                for (std::size_t v225 = 0; v225 < v223; ++v225)
                                                {
                                                    state.PrepareSlew(v225);
                                                }

                                                state.FFT(false);
                                                const size_t v226 = input.m_count;
                                                struct loop_v227_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v227_Type loop_v227;
                                                for (std::size_t v228 = 0; v228 < v226; ++v228)
                                                {
                                                    [[maybe_unused]] const auto v229 = state.SlewCutoff(v228);
                                                    const bool v230 = (0x10000000000000p-53 <= v229);
                                                    if (v230)
                                                    {
                                                        struct loop_v231_Type
                                                        {
                                                        };

                                                        [[maybe_unused]] loop_v231_Type loop_v231;
                                                        for (std::size_t v232 = 0; v232 < 2047; ++v232)
                                                        {
                                                            [[maybe_unused]] const auto v233 = state.Measurement(v228, v232);
                                                            [[maybe_unused]] const auto v234 = state.PreviousPhase(v228, v232);
                                                            const size_t v235 = CodegenInteger::Add<size_t>(v232, 1);
                                                            const double v236 = static_cast<double>(v235);
                                                            const double v237 = (v236 / 0x10000000000000p-40);
                                                            const double v238 = (v233.m_phase - v234);
                                                            const double v239 = (v237 * 0x10000000000000p-42);
                                                            const double v240 = (v238 - v239);
                                                            const double v241 = std::floor(v240);
                                                            const double v242 = (v240 - v241);
                                                            const bool v243 = (0x10000000000000p-53 < v242);
                                                            double v245;
                                                            if (v243)
                                                            {
                                                                const double v244 = (v242 - 1.0);
                                                                v245 = v244;
                                                            }
                                                            else
                                                            {
                                                                v245 = v242;
                                                            }

                                                            const double v246 = (v245 / 0x10000000000000p-42);
                                                            const double v247 = (v246 + v237);
                                                            state.SetCurrent(v228, v232, {v233.m_magnitude, v233.m_phase}, v247);
                                                            [[maybe_unused]] const auto v248 = state.Magnitude(v228, v232);
                                                            const bool v249 = (v248 < v233.m_magnitude);
                                                            double v253;
                                                            if (v249)
                                                            {
                                                                const double v250 = (v233.m_magnitude - v248);
                                                                const double v251 = (v250 * 1.0);
                                                                const double v252 = (v248 + v251);
                                                                v253 = v252;
                                                            }
                                                            else
                                                            {
                                                                v253 = v233.m_magnitude;
                                                            }

                                                            state.SetMagnitude(v228, v232, v253);
                                                        }

                                                    }
                                                    else
                                                    {
                                                        [[maybe_unused]] const auto v254 = state.SlewCoefficient(v229);
                                                        struct loop_v255_Type
                                                        {
                                                        };

                                                        [[maybe_unused]] loop_v255_Type loop_v255;
                                                        for (std::size_t v256 = 0; v256 < 2047; ++v256)
                                                        {
                                                            [[maybe_unused]] const auto v257 = state.Measurement(v228, v256);
                                                            [[maybe_unused]] const auto v258 = state.PreviousPhase(v228, v256);
                                                            const size_t v259 = CodegenInteger::Add<size_t>(v256, 1);
                                                            const double v260 = static_cast<double>(v259);
                                                            const double v261 = (v260 / 0x10000000000000p-40);
                                                            const double v262 = (v257.m_phase - v258);
                                                            const double v263 = (v261 * 0x10000000000000p-42);
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
                                                            const double v271 = (v270 + v261);
                                                            state.SetCurrent(v228, v256, {v257.m_magnitude, v257.m_phase}, v271);
                                                            [[maybe_unused]] const auto v272 = state.Magnitude(v228, v256);
                                                            const bool v273 = (v272 < v257.m_magnitude);
                                                            double v277;
                                                            if (v273)
                                                            {
                                                                const double v274 = (v257.m_magnitude - v272);
                                                                const double v275 = (v274 * v254);
                                                                const double v276 = (v272 + v275);
                                                                v277 = v276;
                                                            }
                                                            else
                                                            {
                                                                v277 = v257.m_magnitude;
                                                            }

                                                            state.SetMagnitude(v228, v256, v277);
                                                        }

                                                    }

                                                }

                                                const size_t v278 = input.m_count;
                                                struct loop_v279_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v279_Type loop_v279;
                                                for (std::size_t v280 = 0; v280 < v278; ++v280)
                                                {
                                                    state.AnalyzeGraph(v280);
                                                }

                                                state.Synthesize(input.m_count);
                                                state.IFFT();
                                                const size_t v281 = input.m_frameSize;
                                                struct loop_v282_Type
                                                {
                                                };

                                                [[maybe_unused]] loop_v282_Type loop_v282;
                                                for (std::size_t v283 = 0; v283 < v281; ++v283)
                                                {
                                                    [[maybe_unused]] const auto v284 = state.Window(v283);
                                                    const size_t v285 = input.m_count;
                                                    struct loop_v286_Type
                                                    {
                                                    };

                                                    [[maybe_unused]] loop_v286_Type loop_v286;
                                                    for (std::size_t v287 = 0; v287 < v285; ++v287)
                                                    {
                                                        [[maybe_unused]] const auto v288 = state.GetSample(v287, v283);
                                                        const double v289 = (v288 * v284);
                                                        state.PutSample(v287, v283, v289);
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
