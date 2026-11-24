#pragma once

// Generated from DSP.Resynthesis.SpectrumStorage.synthesize32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct SynthesizeSpectrum32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State, typename Input>
    static void Process(State& state, [[maybe_unused]] const Input& input)
    {
        if (input.m_stage == 0u)
        {
            state.ClearOutput();
            const size_t v0 = input.m_count;
            struct loop_v1_Type
            {
            };

            [[maybe_unused]] loop_v1_Type loop_v1;
            for (std::size_t v2 = 0; v2 < v0; ++v2)
            {
                [[maybe_unused]] const auto v3 = state.Controls(v2);
                struct loop_v4_Type
                {
                };

                [[maybe_unused]] loop_v4_Type loop_v4;
                for (std::size_t v5 = 0; v5 < 3; ++v5)
                {
                    [[maybe_unused]] const auto v6 = state.GraphSize(v2);
                    struct loop_v7_Type
                    {
                    };

                    [[maybe_unused]] loop_v7_Type loop_v7;
                    for (std::size_t v8 = 0; v8 < v6; ++v8)
                    {
                        [[maybe_unused]] const auto v9 = state.VisitAt(v2, v8);
                        const bool v10 = (v9.m_link.m_root == v9.m_bin);
                        const bool v11 = (v10 == true);
                        if (v11)
                        {
                            [[maybe_unused]] const auto v12 = state.Phase(v2, v5, v9.m_bin);
                            [[maybe_unused]] const auto v13 = state.AnalysisPhase(v2, v9.m_bin);
                            [[maybe_unused]] const auto v14 = state.Gate(v2, v9.m_bin);
                            bool v16;
                            if (v14 == 0u)
                            {
                                v16 = false;
                            }
                            else
                            {
                                bool v15;
                                if (v14 == 1u)
                                {
                                    v15 = true;
                                }
                                else
                                {
                                    v15 = false;
                                }

                                v16 = v15;
                            }

                            const bool v17 = (v16 == true);
                            double v27;
                            if (v17)
                            {
                                const double v18 = static_cast<double>(v13);
                                v27 = v18;
                            }
                            else
                            {
                                const bool v19 = (v5 == 0);
                                float v23;
                                if (v19)
                                {
                                    v23 = 1.0f;
                                }
                                else
                                {
                                    const bool v20 = (v5 == 1);
                                    float v22;
                                    if (v20)
                                    {
                                        v22 = v3.m_unisonDetune;
                                    }
                                    else
                                    {
                                        const float v21 = (1.0f / v3.m_unisonDetune);
                                        v22 = v21;
                                    }

                                    v23 = v22;
                                }

                                const double v24 = static_cast<double>(v23);
                                const double v25 = (v9.m_link.m_delta * v24);
                                const double v26 = (v12 + v25);
                                v27 = v26;
                            }

                            state.SetPhase(v2, v5, v9.m_bin, v27);
                        }
                        else
                        {
                            [[maybe_unused]] const auto v28 = state.Phase(v2, v5, v9.m_link.m_root);
                            [[maybe_unused]] const auto v29 = state.Omega(v2, v9.m_link.m_root);
                            state.SetOmega(v2, v9.m_bin, v29);
                            const double v30 = (v28 + v9.m_link.m_delta);
                            state.SetPhase(v2, v5, v9.m_bin, v30);
                        }

                    }

                }

                const float v31 = (v3.m_fade * v3.m_fade);
                const float v32 = (v31 * v3.m_fade);
                const float v33 = (v32 * v3.m_fade);
                struct loop_v34_Type
                {
                };

                [[maybe_unused]] loop_v34_Type loop_v34;
                for (std::size_t v35 = 0; v35 < 3; ++v35)
                {
                    const bool v36 = (v35 == 0);
                    float v44;
                    if (v36)
                    {
                        const float v37 = (0x800000p-22f * v3.m_unisonGain);
                        const float v38 = (v37 * v3.m_unisonGain);
                        const float v39 = (v38 / 0xc00000p-22f);
                        const float v40 = (1.0f - v39);
                        const float v41 = std::sqrt(v40);
                        v44 = v41;
                    }
                    else
                    {
                        const float v42 = std::sqrt(0xc00000p-22f);
                        const float v43 = (v3.m_unisonGain / v42);
                        v44 = v43;
                    }

                    struct loop_v45_Type
                    {
                    };

                    [[maybe_unused]] loop_v45_Type loop_v45;
                    for (std::size_t v46 = 0; v46 < 2047; ++v46)
                    {
                        [[maybe_unused]] const auto v47 = state.SynthesisMagnitude(v2, v46);
                        [[maybe_unused]] const auto v48 = state.Phase(v2, v35, v46);
                        [[maybe_unused]] const auto v49 = state.Coefficient(v2, v46);
                        const float v50 = (1.0f - v33);
                        const bool v51 = (0.0f <= v50);
                        float v53;
                        if (v51)
                        {
                            const float v52 = (1.0f - v33);
                            v53 = v52;
                        }
                        else
                        {
                            v53 = 0.0f;
                        }

                        const float v54 = (v53 * v44);
                        const float v55 = (v54 * v47);
                        [[maybe_unused]] const auto v56 = state.Polar(v55, v48);
                        const float v57 = (v49.m_real + v56.m_real);
                        const float v58 = (v49.m_imag + v56.m_imag);
                        state.SetCoefficient(v2, v46, {v57, v58});
                    }

                    const std::int64_t v59 = CodegenInteger::FromIndex(1);
                    const bool v60 = (v3.m_shift.m_numerator == v59);
                    if (v60)
                    {
                        const std::int64_t v61 = CodegenInteger::FromIndex(1);
                        const bool v62 = (v3.m_shift.m_denominator == v61);
                        if (v62)
                        {
                            struct loop_v63_Type
                            {
                            };

                            [[maybe_unused]] loop_v63_Type loop_v63;
                            for (std::size_t v64 = 0; v64 < 2047; ++v64)
                            {
                                [[maybe_unused]] const auto v65 = state.SynthesisMagnitude(v2, v64);
                                [[maybe_unused]] const auto v66 = state.Phase(v2, v35, v64);
                                [[maybe_unused]] const auto v67 = state.Coefficient(v2, v64);
                                const bool v68 = (0.0f <= v33);
                                float v69;
                                if (v68)
                                {
                                    v69 = v33;
                                }
                                else
                                {
                                    v69 = 0.0f;
                                }

                                const float v70 = (v69 * v44);
                                const float v71 = (v70 * v65);
                                [[maybe_unused]] const auto v72 = state.Polar(v71, v66);
                                const float v73 = (v67.m_real + v72.m_real);
                                const float v74 = (v67.m_imag + v72.m_imag);
                                state.SetCoefficient(v2, v64, {v73, v74});
                            }

                        }
                        else
                        {
                            [[maybe_unused]] const auto v75 = state.Ratio({v3.m_shift.m_numerator, v3.m_shift.m_denominator});
                            [[maybe_unused]] const auto v76 = state.GraphSize(v2);
                            struct loop_v77_Type
                            {
                            };

                            [[maybe_unused]] loop_v77_Type loop_v77;
                            for (std::size_t v78 = 0; v78 < v76; ++v78)
                            {
                                [[maybe_unused]] const auto v79 = state.VisitAt(v2, v78);
                                const bool v80 = (v79.m_link.m_small == true);
                                if (v80)
                                {
                                }
                                else
                                {
                                    [[maybe_unused]] const auto v81 = state.Omega(v2, v79.m_link.m_root);
                                    const double v82 = (v81 * v75);
                                    const double v83 = (v82 * 0x10000000000000p-40);
                                    [[maybe_unused]] const auto v84 = state.Round(v83);
                                    const std::int64_t v85 = CodegenInteger::FromIndex(1);
                                    const bool v86 = (v85 <= v84);
                                    std::int64_t v88;
                                    if (v86)
                                    {
                                        v88 = v84;
                                    }
                                    else
                                    {
                                        const std::int64_t v87 = CodegenInteger::FromIndex(1);
                                        v88 = v87;
                                    }

                                    const std::int64_t v89 = CodegenInteger::FromIndex(4096);
                                    const std::int64_t v90 = CodegenInteger::FromIndex(1);
                                    const std::int64_t v91 = CodegenInteger::Subtract(v89, v90);
                                    const bool v92 = (v88 <= v91);
                                    std::int64_t v100;
                                    if (v92)
                                    {
                                        const std::int64_t v93 = CodegenInteger::FromIndex(1);
                                        const bool v94 = (v93 <= v84);
                                        std::int64_t v96;
                                        if (v94)
                                        {
                                            v96 = v84;
                                        }
                                        else
                                        {
                                            const std::int64_t v95 = CodegenInteger::FromIndex(1);
                                            v96 = v95;
                                        }

                                        v100 = v96;
                                    }
                                    else
                                    {
                                        const std::int64_t v97 = CodegenInteger::FromIndex(4096);
                                        const std::int64_t v98 = CodegenInteger::FromIndex(1);
                                        const std::int64_t v99 = CodegenInteger::Subtract(v97, v98);
                                        v100 = v99;
                                    }

                                    [[maybe_unused]] const auto v101 = state.Phase(v2, v35, v79.m_link.m_root);
                                    const double v102 = (v101 * v75);
                                    const bool v103 = (v79.m_link.m_root == v79.m_bin);
                                    const bool v104 = (v103 == true);
                                    std::int64_t v111;
                                    if (v104)
                                    {
                                        v111 = v100;
                                    }
                                    else
                                    {
                                        const size_t v105 = CodegenInteger::Add<size_t>(v79.m_link.m_root, 1);
                                        const std::int64_t v106 = CodegenInteger::FromIndex(v105);
                                        const std::int64_t v107 = CodegenInteger::Subtract(v100, v106);
                                        const size_t v108 = CodegenInteger::Add<size_t>(v79.m_bin, 1);
                                        const std::int64_t v109 = CodegenInteger::FromIndex(v108);
                                        const std::int64_t v110 = CodegenInteger::Add<std::int64_t>(v107, v109);
                                        v111 = v110;
                                    }

                                    const bool v112 = (v79.m_link.m_root == v79.m_bin);
                                    const bool v113 = (v112 == true);
                                    double v115;
                                    if (v113)
                                    {
                                        v115 = v102;
                                    }
                                    else
                                    {
                                        const double v114 = (v102 + v79.m_link.m_delta);
                                        v115 = v114;
                                    }

                                    const std::int64_t v116 = CodegenInteger::FromIndex(0);
                                    const bool v117 = (v116 < v111);
                                    bool v118;
                                    if (v117)
                                    {
                                        v118 = true;
                                    }
                                    else
                                    {
                                        v118 = false;
                                    }

                                    bool v139;
                                    size_t v140;
                                    if (v118)
                                    {
                                        const std::int64_t v119 = CodegenInteger::FromIndex(2048);
                                        const bool v120 = (v111 < v119);
                                        bool v121;
                                        if (v120)
                                        {
                                            v121 = true;
                                        }
                                        else
                                        {
                                            v121 = false;
                                        }

                                        bool v137;
                                        size_t v138;
                                        if (v121)
                                        {
                                            const size_t v122 = CodegenInteger::ToIndex(v111);
                                            const size_t v123 = (v122 < 1 ? 0 : v122 - 1);
                                            const size_t v124 = CodegenInteger::Add<size_t>(v123, 1);
                                            const float v125 = static_cast<float>(v124);
                                            const float v126 = (v125 / 0x800000p-11f);
                                            const double v127 = static_cast<double>(v126);
                                            const double v128 = (v82 - v127);
                                            const double v129 = (0x10000000000000p-53 / 0x10000000000000p-42);
                                            const bool v130 = (v129 < v128);
                                            bool v135;
                                            size_t v136;
                                            if (v130)
                                            {
                                                v135 = false;
                                                v136 = size_t{};
                                            }
                                            else
                                            {
                                                const double v131 = (-v128);
                                                const bool v132 = (v129 < v131);
                                                bool v133;
                                                size_t v134;
                                                if (v132)
                                                {
                                                    v133 = false;
                                                    v134 = size_t{};
                                                }
                                                else
                                                {
                                                    v133 = true;
                                                    v134 = v123;
                                                }

                                                v135 = v133;
                                                v136 = v134;
                                            }

                                            v137 = v135;
                                            v138 = v136;
                                        }
                                        else
                                        {
                                            v137 = false;
                                            v138 = size_t{};
                                        }

                                        v139 = v137;
                                        v140 = v138;
                                    }
                                    else
                                    {
                                        v139 = false;
                                        v140 = size_t{};
                                    }

                                    if (v139)
                                    {
                                        [[maybe_unused]] const auto v141 = state.SynthesisMagnitude(v2, v79.m_bin);
                                        [[maybe_unused]] const auto v142 = state.Coefficient(v2, v140);
                                        const bool v143 = (0.0f <= v33);
                                        float v144;
                                        if (v143)
                                        {
                                            v144 = v33;
                                        }
                                        else
                                        {
                                            v144 = 0.0f;
                                        }

                                        const float v145 = (v144 * v44);
                                        const float v146 = (v145 * v141);
                                        [[maybe_unused]] const auto v147 = state.Polar(v146, v115);
                                        const float v148 = (v142.m_real + v147.m_real);
                                        const float v149 = (v142.m_imag + v147.m_imag);
                                        state.SetCoefficient(v2, v140, {v148, v149});
                                    }
                                    else
                                    {
                                    }

                                }

                            }

                        }

                    }
                    else
                    {
                        [[maybe_unused]] const auto v150 = state.Ratio({v3.m_shift.m_numerator, v3.m_shift.m_denominator});
                        [[maybe_unused]] const auto v151 = state.GraphSize(v2);
                        struct loop_v152_Type
                        {
                        };

                        [[maybe_unused]] loop_v152_Type loop_v152;
                        for (std::size_t v153 = 0; v153 < v151; ++v153)
                        {
                            [[maybe_unused]] const auto v154 = state.VisitAt(v2, v153);
                            const bool v155 = (v154.m_link.m_small == true);
                            if (v155)
                            {
                            }
                            else
                            {
                                [[maybe_unused]] const auto v156 = state.Omega(v2, v154.m_link.m_root);
                                const double v157 = (v156 * v150);
                                const double v158 = (v157 * 0x10000000000000p-40);
                                [[maybe_unused]] const auto v159 = state.Round(v158);
                                const std::int64_t v160 = CodegenInteger::FromIndex(1);
                                const bool v161 = (v160 <= v159);
                                std::int64_t v163;
                                if (v161)
                                {
                                    v163 = v159;
                                }
                                else
                                {
                                    const std::int64_t v162 = CodegenInteger::FromIndex(1);
                                    v163 = v162;
                                }

                                const std::int64_t v164 = CodegenInteger::FromIndex(4096);
                                const std::int64_t v165 = CodegenInteger::FromIndex(1);
                                const std::int64_t v166 = CodegenInteger::Subtract(v164, v165);
                                const bool v167 = (v163 <= v166);
                                std::int64_t v175;
                                if (v167)
                                {
                                    const std::int64_t v168 = CodegenInteger::FromIndex(1);
                                    const bool v169 = (v168 <= v159);
                                    std::int64_t v171;
                                    if (v169)
                                    {
                                        v171 = v159;
                                    }
                                    else
                                    {
                                        const std::int64_t v170 = CodegenInteger::FromIndex(1);
                                        v171 = v170;
                                    }

                                    v175 = v171;
                                }
                                else
                                {
                                    const std::int64_t v172 = CodegenInteger::FromIndex(4096);
                                    const std::int64_t v173 = CodegenInteger::FromIndex(1);
                                    const std::int64_t v174 = CodegenInteger::Subtract(v172, v173);
                                    v175 = v174;
                                }

                                [[maybe_unused]] const auto v176 = state.Phase(v2, v35, v154.m_link.m_root);
                                const double v177 = (v176 * v150);
                                const bool v178 = (v154.m_link.m_root == v154.m_bin);
                                const bool v179 = (v178 == true);
                                std::int64_t v186;
                                if (v179)
                                {
                                    v186 = v175;
                                }
                                else
                                {
                                    const size_t v180 = CodegenInteger::Add<size_t>(v154.m_link.m_root, 1);
                                    const std::int64_t v181 = CodegenInteger::FromIndex(v180);
                                    const std::int64_t v182 = CodegenInteger::Subtract(v175, v181);
                                    const size_t v183 = CodegenInteger::Add<size_t>(v154.m_bin, 1);
                                    const std::int64_t v184 = CodegenInteger::FromIndex(v183);
                                    const std::int64_t v185 = CodegenInteger::Add<std::int64_t>(v182, v184);
                                    v186 = v185;
                                }

                                const bool v187 = (v154.m_link.m_root == v154.m_bin);
                                const bool v188 = (v187 == true);
                                double v190;
                                if (v188)
                                {
                                    v190 = v177;
                                }
                                else
                                {
                                    const double v189 = (v177 + v154.m_link.m_delta);
                                    v190 = v189;
                                }

                                const std::int64_t v191 = CodegenInteger::FromIndex(0);
                                const bool v192 = (v191 < v186);
                                bool v193;
                                if (v192)
                                {
                                    v193 = true;
                                }
                                else
                                {
                                    v193 = false;
                                }

                                bool v214;
                                size_t v215;
                                if (v193)
                                {
                                    const std::int64_t v194 = CodegenInteger::FromIndex(2048);
                                    const bool v195 = (v186 < v194);
                                    bool v196;
                                    if (v195)
                                    {
                                        v196 = true;
                                    }
                                    else
                                    {
                                        v196 = false;
                                    }

                                    bool v212;
                                    size_t v213;
                                    if (v196)
                                    {
                                        const size_t v197 = CodegenInteger::ToIndex(v186);
                                        const size_t v198 = (v197 < 1 ? 0 : v197 - 1);
                                        const size_t v199 = CodegenInteger::Add<size_t>(v198, 1);
                                        const float v200 = static_cast<float>(v199);
                                        const float v201 = (v200 / 0x800000p-11f);
                                        const double v202 = static_cast<double>(v201);
                                        const double v203 = (v157 - v202);
                                        const double v204 = (0x10000000000000p-53 / 0x10000000000000p-42);
                                        const bool v205 = (v204 < v203);
                                        bool v210;
                                        size_t v211;
                                        if (v205)
                                        {
                                            v210 = false;
                                            v211 = size_t{};
                                        }
                                        else
                                        {
                                            const double v206 = (-v203);
                                            const bool v207 = (v204 < v206);
                                            bool v208;
                                            size_t v209;
                                            if (v207)
                                            {
                                                v208 = false;
                                                v209 = size_t{};
                                            }
                                            else
                                            {
                                                v208 = true;
                                                v209 = v198;
                                            }

                                            v210 = v208;
                                            v211 = v209;
                                        }

                                        v212 = v210;
                                        v213 = v211;
                                    }
                                    else
                                    {
                                        v212 = false;
                                        v213 = size_t{};
                                    }

                                    v214 = v212;
                                    v215 = v213;
                                }
                                else
                                {
                                    v214 = false;
                                    v215 = size_t{};
                                }

                                if (v214)
                                {
                                    [[maybe_unused]] const auto v216 = state.SynthesisMagnitude(v2, v154.m_bin);
                                    [[maybe_unused]] const auto v217 = state.Coefficient(v2, v215);
                                    const bool v218 = (0.0f <= v33);
                                    float v219;
                                    if (v218)
                                    {
                                        v219 = v33;
                                    }
                                    else
                                    {
                                        v219 = 0.0f;
                                    }

                                    const float v220 = (v219 * v44);
                                    const float v221 = (v220 * v216);
                                    [[maybe_unused]] const auto v222 = state.Polar(v221, v190);
                                    const float v223 = (v217.m_real + v222.m_real);
                                    const float v224 = (v217.m_imag + v222.m_imag);
                                    state.SetCoefficient(v2, v215, {v223, v224});
                                }
                                else
                                {
                                }

                            }

                        }

                    }

                }

            }

        }
        else
        {
            if (input.m_stage == 1u)
            {
                const size_t v225 = input.m_count;
                struct loop_v226_Type
                {
                };

                [[maybe_unused]] loop_v226_Type loop_v226;
                for (std::size_t v227 = 0; v227 < v225; ++v227)
                {
                    [[maybe_unused]] const auto v228 = state.Drive(v227);
                    [[maybe_unused]] const auto v229 = state.GraphSize(v227);
                    struct loop_v230_Type
                    {
                    };

                    [[maybe_unused]] loop_v230_Type loop_v230;
                    for (std::size_t v231 = 0; v231 < v229; ++v231)
                    {
                        [[maybe_unused]] const auto v232 = state.VisitAt(v227, v231);
                        const bool v233 = (v232.m_link.m_root == v232.m_bin);
                        const bool v234 = (v233 == true);
                        if (v234)
                        {
                            [[maybe_unused]] const auto v235 = state.Phase(v227, input.m_oscillator, v232.m_bin);
                            [[maybe_unused]] const auto v236 = state.AnalysisPhase(v227, v232.m_bin);
                            [[maybe_unused]] const auto v237 = state.Gate(v227, v232.m_bin);
                            bool v239;
                            if (v237 == 0u)
                            {
                                v239 = false;
                            }
                            else
                            {
                                bool v238;
                                if (v237 == 1u)
                                {
                                    v238 = true;
                                }
                                else
                                {
                                    v238 = false;
                                }

                                v239 = v238;
                            }

                            const bool v240 = (v239 == true);
                            double v245;
                            if (v240)
                            {
                                const double v241 = static_cast<double>(v236);
                                v245 = v241;
                            }
                            else
                            {
                                const double v242 = static_cast<double>(v228);
                                const double v243 = (v232.m_link.m_delta * v242);
                                const double v244 = (v235 + v243);
                                v245 = v244;
                            }

                            state.SetPhase(v227, input.m_oscillator, v232.m_bin, v245);
                        }
                        else
                        {
                            [[maybe_unused]] const auto v246 = state.Phase(v227, input.m_oscillator, v232.m_link.m_root);
                            [[maybe_unused]] const auto v247 = state.Omega(v227, v232.m_link.m_root);
                            state.SetOmega(v227, v232.m_bin, v247);
                            const double v248 = (v246 + v232.m_link.m_delta);
                            state.SetPhase(v227, input.m_oscillator, v232.m_bin, v248);
                        }

                    }

                }

            }
            else
            {
                const size_t v249 = input.m_count;
                struct loop_v250_Type
                {
                };

                [[maybe_unused]] loop_v250_Type loop_v250;
                for (std::size_t v251 = 0; v251 < v249; ++v251)
                {
                    [[maybe_unused]] const auto v252 = state.Drive(v251);
                    struct loop_v253_Type
                    {
                    };

                    [[maybe_unused]] loop_v253_Type loop_v253;
                    for (std::size_t v254 = 0; v254 < 2047; ++v254)
                    {
                        [[maybe_unused]] const auto v255 = state.SynthesisMagnitude(v251, v254);
                        [[maybe_unused]] const auto v256 = state.Phase(v251, input.m_oscillator, v254);
                        [[maybe_unused]] const auto v257 = state.Coefficient(v251, v254);
                        const float v258 = (v252 * v255);
                        [[maybe_unused]] const auto v259 = state.Polar(v258, v256);
                        const float v260 = (v257.m_real + v259.m_real);
                        const float v261 = (v257.m_imag + v259.m_imag);
                        state.SetCoefficient(v251, v254, {v260, v261});
                    }

                }

            }

        }

    }

};
