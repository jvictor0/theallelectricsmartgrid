#pragma once

// Generated from DSP.Resynthesis.SpectrumStorage.synthesize64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct SynthesizeSpectrum64
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
                            double v25;
                            if (v17)
                            {
                                v25 = v13;
                            }
                            else
                            {
                                const bool v18 = (v5 == 0);
                                double v22;
                                if (v18)
                                {
                                    v22 = 1.0;
                                }
                                else
                                {
                                    const bool v19 = (v5 == 1);
                                    double v21;
                                    if (v19)
                                    {
                                        v21 = v3.m_unisonDetune;
                                    }
                                    else
                                    {
                                        const double v20 = (1.0 / v3.m_unisonDetune);
                                        v21 = v20;
                                    }

                                    v22 = v21;
                                }

                                const double v23 = (v9.m_link.m_delta * v22);
                                const double v24 = (v12 + v23);
                                v25 = v24;
                            }

                            state.SetPhase(v2, v5, v9.m_bin, v25);
                        }
                        else
                        {
                            [[maybe_unused]] const auto v26 = state.Phase(v2, v5, v9.m_link.m_root);
                            [[maybe_unused]] const auto v27 = state.Omega(v2, v9.m_link.m_root);
                            state.SetOmega(v2, v9.m_bin, v27);
                            const double v28 = (v26 + v9.m_link.m_delta);
                            state.SetPhase(v2, v5, v9.m_bin, v28);
                        }

                    }

                }

                const double v29 = (v3.m_fade * v3.m_fade);
                const double v30 = (v29 * v3.m_fade);
                const double v31 = (v30 * v3.m_fade);
                struct loop_v32_Type
                {
                };

                [[maybe_unused]] loop_v32_Type loop_v32;
                for (std::size_t v33 = 0; v33 < 3; ++v33)
                {
                    const bool v34 = (v33 == 0);
                    double v42;
                    if (v34)
                    {
                        const double v35 = (0x10000000000000p-51 * v3.m_unisonGain);
                        const double v36 = (v35 * v3.m_unisonGain);
                        const double v37 = (v36 / 0x18000000000000p-51);
                        const double v38 = (1.0 - v37);
                        const double v39 = std::sqrt(v38);
                        v42 = v39;
                    }
                    else
                    {
                        const double v40 = std::sqrt(0x18000000000000p-51);
                        const double v41 = (v3.m_unisonGain / v40);
                        v42 = v41;
                    }

                    struct loop_v43_Type
                    {
                    };

                    [[maybe_unused]] loop_v43_Type loop_v43;
                    for (std::size_t v44 = 0; v44 < 2047; ++v44)
                    {
                        [[maybe_unused]] const auto v45 = state.SynthesisMagnitude(v2, v44);
                        [[maybe_unused]] const auto v46 = state.Phase(v2, v33, v44);
                        [[maybe_unused]] const auto v47 = state.Coefficient(v2, v44);
                        const double v48 = (1.0 - v31);
                        const bool v49 = (0.0 <= v48);
                        double v51;
                        if (v49)
                        {
                            const double v50 = (1.0 - v31);
                            v51 = v50;
                        }
                        else
                        {
                            v51 = 0.0;
                        }

                        const double v52 = (v51 * v42);
                        const double v53 = (v52 * v45);
                        [[maybe_unused]] const auto v54 = state.Polar(v53, v46);
                        const double v55 = (v47.m_real + v54.m_real);
                        const double v56 = (v47.m_imag + v54.m_imag);
                        state.SetCoefficient(v2, v44, {v55, v56});
                    }

                    const std::int64_t v57 = CodegenInteger::FromIndex(1);
                    const bool v58 = (v3.m_shift.m_numerator == v57);
                    if (v58)
                    {
                        const std::int64_t v59 = CodegenInteger::FromIndex(1);
                        const bool v60 = (v3.m_shift.m_denominator == v59);
                        if (v60)
                        {
                            struct loop_v61_Type
                            {
                            };

                            [[maybe_unused]] loop_v61_Type loop_v61;
                            for (std::size_t v62 = 0; v62 < 2047; ++v62)
                            {
                                [[maybe_unused]] const auto v63 = state.SynthesisMagnitude(v2, v62);
                                [[maybe_unused]] const auto v64 = state.Phase(v2, v33, v62);
                                [[maybe_unused]] const auto v65 = state.Coefficient(v2, v62);
                                const bool v66 = (0.0 <= v31);
                                double v67;
                                if (v66)
                                {
                                    v67 = v31;
                                }
                                else
                                {
                                    v67 = 0.0;
                                }

                                const double v68 = (v67 * v42);
                                const double v69 = (v68 * v63);
                                [[maybe_unused]] const auto v70 = state.Polar(v69, v64);
                                const double v71 = (v65.m_real + v70.m_real);
                                const double v72 = (v65.m_imag + v70.m_imag);
                                state.SetCoefficient(v2, v62, {v71, v72});
                            }

                        }
                        else
                        {
                            [[maybe_unused]] const auto v73 = state.Ratio({v3.m_shift.m_numerator, v3.m_shift.m_denominator});
                            [[maybe_unused]] const auto v74 = state.GraphSize(v2);
                            struct loop_v75_Type
                            {
                            };

                            [[maybe_unused]] loop_v75_Type loop_v75;
                            for (std::size_t v76 = 0; v76 < v74; ++v76)
                            {
                                [[maybe_unused]] const auto v77 = state.VisitAt(v2, v76);
                                const bool v78 = (v77.m_link.m_small == true);
                                if (v78)
                                {
                                }
                                else
                                {
                                    [[maybe_unused]] const auto v79 = state.Omega(v2, v77.m_link.m_root);
                                    const double v80 = (v79 * v73);
                                    const double v81 = (v80 * 0x10000000000000p-40);
                                    [[maybe_unused]] const auto v82 = state.Round(v81);
                                    const std::int64_t v83 = CodegenInteger::FromIndex(1);
                                    const bool v84 = (v83 <= v82);
                                    std::int64_t v86;
                                    if (v84)
                                    {
                                        v86 = v82;
                                    }
                                    else
                                    {
                                        const std::int64_t v85 = CodegenInteger::FromIndex(1);
                                        v86 = v85;
                                    }

                                    const std::int64_t v87 = CodegenInteger::FromIndex(4096);
                                    const std::int64_t v88 = CodegenInteger::FromIndex(1);
                                    const std::int64_t v89 = CodegenInteger::Subtract(v87, v88);
                                    const bool v90 = (v86 <= v89);
                                    std::int64_t v98;
                                    if (v90)
                                    {
                                        const std::int64_t v91 = CodegenInteger::FromIndex(1);
                                        const bool v92 = (v91 <= v82);
                                        std::int64_t v94;
                                        if (v92)
                                        {
                                            v94 = v82;
                                        }
                                        else
                                        {
                                            const std::int64_t v93 = CodegenInteger::FromIndex(1);
                                            v94 = v93;
                                        }

                                        v98 = v94;
                                    }
                                    else
                                    {
                                        const std::int64_t v95 = CodegenInteger::FromIndex(4096);
                                        const std::int64_t v96 = CodegenInteger::FromIndex(1);
                                        const std::int64_t v97 = CodegenInteger::Subtract(v95, v96);
                                        v98 = v97;
                                    }

                                    [[maybe_unused]] const auto v99 = state.Phase(v2, v33, v77.m_link.m_root);
                                    const double v100 = (v99 * v73);
                                    const bool v101 = (v77.m_link.m_root == v77.m_bin);
                                    const bool v102 = (v101 == true);
                                    std::int64_t v109;
                                    if (v102)
                                    {
                                        v109 = v98;
                                    }
                                    else
                                    {
                                        const size_t v103 = CodegenInteger::Add<size_t>(v77.m_link.m_root, 1);
                                        const std::int64_t v104 = CodegenInteger::FromIndex(v103);
                                        const std::int64_t v105 = CodegenInteger::Subtract(v98, v104);
                                        const size_t v106 = CodegenInteger::Add<size_t>(v77.m_bin, 1);
                                        const std::int64_t v107 = CodegenInteger::FromIndex(v106);
                                        const std::int64_t v108 = CodegenInteger::Add<std::int64_t>(v105, v107);
                                        v109 = v108;
                                    }

                                    const bool v110 = (v77.m_link.m_root == v77.m_bin);
                                    const bool v111 = (v110 == true);
                                    double v113;
                                    if (v111)
                                    {
                                        v113 = v100;
                                    }
                                    else
                                    {
                                        const double v112 = (v100 + v77.m_link.m_delta);
                                        v113 = v112;
                                    }

                                    const std::int64_t v114 = CodegenInteger::FromIndex(0);
                                    const bool v115 = (v114 < v109);
                                    bool v116;
                                    if (v115)
                                    {
                                        v116 = true;
                                    }
                                    else
                                    {
                                        v116 = false;
                                    }

                                    bool v136;
                                    size_t v137;
                                    if (v116)
                                    {
                                        const std::int64_t v117 = CodegenInteger::FromIndex(2048);
                                        const bool v118 = (v109 < v117);
                                        bool v119;
                                        if (v118)
                                        {
                                            v119 = true;
                                        }
                                        else
                                        {
                                            v119 = false;
                                        }

                                        bool v134;
                                        size_t v135;
                                        if (v119)
                                        {
                                            const size_t v120 = CodegenInteger::ToIndex(v109);
                                            const size_t v121 = (v120 < 1 ? 0 : v120 - 1);
                                            const size_t v122 = CodegenInteger::Add<size_t>(v121, 1);
                                            const double v123 = static_cast<double>(v122);
                                            const double v124 = (v123 / 0x10000000000000p-40);
                                            const double v125 = (v80 - v124);
                                            const double v126 = (0x10000000000000p-53 / 0x10000000000000p-42);
                                            const bool v127 = (v126 < v125);
                                            bool v132;
                                            size_t v133;
                                            if (v127)
                                            {
                                                v132 = false;
                                                v133 = size_t{};
                                            }
                                            else
                                            {
                                                const double v128 = (-v125);
                                                const bool v129 = (v126 < v128);
                                                bool v130;
                                                size_t v131;
                                                if (v129)
                                                {
                                                    v130 = false;
                                                    v131 = size_t{};
                                                }
                                                else
                                                {
                                                    v130 = true;
                                                    v131 = v121;
                                                }

                                                v132 = v130;
                                                v133 = v131;
                                            }

                                            v134 = v132;
                                            v135 = v133;
                                        }
                                        else
                                        {
                                            v134 = false;
                                            v135 = size_t{};
                                        }

                                        v136 = v134;
                                        v137 = v135;
                                    }
                                    else
                                    {
                                        v136 = false;
                                        v137 = size_t{};
                                    }

                                    if (v136)
                                    {
                                        [[maybe_unused]] const auto v138 = state.SynthesisMagnitude(v2, v77.m_bin);
                                        [[maybe_unused]] const auto v139 = state.Coefficient(v2, v137);
                                        const bool v140 = (0.0 <= v31);
                                        double v141;
                                        if (v140)
                                        {
                                            v141 = v31;
                                        }
                                        else
                                        {
                                            v141 = 0.0;
                                        }

                                        const double v142 = (v141 * v42);
                                        const double v143 = (v142 * v138);
                                        [[maybe_unused]] const auto v144 = state.Polar(v143, v113);
                                        const double v145 = (v139.m_real + v144.m_real);
                                        const double v146 = (v139.m_imag + v144.m_imag);
                                        state.SetCoefficient(v2, v137, {v145, v146});
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
                        [[maybe_unused]] const auto v147 = state.Ratio({v3.m_shift.m_numerator, v3.m_shift.m_denominator});
                        [[maybe_unused]] const auto v148 = state.GraphSize(v2);
                        struct loop_v149_Type
                        {
                        };

                        [[maybe_unused]] loop_v149_Type loop_v149;
                        for (std::size_t v150 = 0; v150 < v148; ++v150)
                        {
                            [[maybe_unused]] const auto v151 = state.VisitAt(v2, v150);
                            const bool v152 = (v151.m_link.m_small == true);
                            if (v152)
                            {
                            }
                            else
                            {
                                [[maybe_unused]] const auto v153 = state.Omega(v2, v151.m_link.m_root);
                                const double v154 = (v153 * v147);
                                const double v155 = (v154 * 0x10000000000000p-40);
                                [[maybe_unused]] const auto v156 = state.Round(v155);
                                const std::int64_t v157 = CodegenInteger::FromIndex(1);
                                const bool v158 = (v157 <= v156);
                                std::int64_t v160;
                                if (v158)
                                {
                                    v160 = v156;
                                }
                                else
                                {
                                    const std::int64_t v159 = CodegenInteger::FromIndex(1);
                                    v160 = v159;
                                }

                                const std::int64_t v161 = CodegenInteger::FromIndex(4096);
                                const std::int64_t v162 = CodegenInteger::FromIndex(1);
                                const std::int64_t v163 = CodegenInteger::Subtract(v161, v162);
                                const bool v164 = (v160 <= v163);
                                std::int64_t v172;
                                if (v164)
                                {
                                    const std::int64_t v165 = CodegenInteger::FromIndex(1);
                                    const bool v166 = (v165 <= v156);
                                    std::int64_t v168;
                                    if (v166)
                                    {
                                        v168 = v156;
                                    }
                                    else
                                    {
                                        const std::int64_t v167 = CodegenInteger::FromIndex(1);
                                        v168 = v167;
                                    }

                                    v172 = v168;
                                }
                                else
                                {
                                    const std::int64_t v169 = CodegenInteger::FromIndex(4096);
                                    const std::int64_t v170 = CodegenInteger::FromIndex(1);
                                    const std::int64_t v171 = CodegenInteger::Subtract(v169, v170);
                                    v172 = v171;
                                }

                                [[maybe_unused]] const auto v173 = state.Phase(v2, v33, v151.m_link.m_root);
                                const double v174 = (v173 * v147);
                                const bool v175 = (v151.m_link.m_root == v151.m_bin);
                                const bool v176 = (v175 == true);
                                std::int64_t v183;
                                if (v176)
                                {
                                    v183 = v172;
                                }
                                else
                                {
                                    const size_t v177 = CodegenInteger::Add<size_t>(v151.m_link.m_root, 1);
                                    const std::int64_t v178 = CodegenInteger::FromIndex(v177);
                                    const std::int64_t v179 = CodegenInteger::Subtract(v172, v178);
                                    const size_t v180 = CodegenInteger::Add<size_t>(v151.m_bin, 1);
                                    const std::int64_t v181 = CodegenInteger::FromIndex(v180);
                                    const std::int64_t v182 = CodegenInteger::Add<std::int64_t>(v179, v181);
                                    v183 = v182;
                                }

                                const bool v184 = (v151.m_link.m_root == v151.m_bin);
                                const bool v185 = (v184 == true);
                                double v187;
                                if (v185)
                                {
                                    v187 = v174;
                                }
                                else
                                {
                                    const double v186 = (v174 + v151.m_link.m_delta);
                                    v187 = v186;
                                }

                                const std::int64_t v188 = CodegenInteger::FromIndex(0);
                                const bool v189 = (v188 < v183);
                                bool v190;
                                if (v189)
                                {
                                    v190 = true;
                                }
                                else
                                {
                                    v190 = false;
                                }

                                bool v210;
                                size_t v211;
                                if (v190)
                                {
                                    const std::int64_t v191 = CodegenInteger::FromIndex(2048);
                                    const bool v192 = (v183 < v191);
                                    bool v193;
                                    if (v192)
                                    {
                                        v193 = true;
                                    }
                                    else
                                    {
                                        v193 = false;
                                    }

                                    bool v208;
                                    size_t v209;
                                    if (v193)
                                    {
                                        const size_t v194 = CodegenInteger::ToIndex(v183);
                                        const size_t v195 = (v194 < 1 ? 0 : v194 - 1);
                                        const size_t v196 = CodegenInteger::Add<size_t>(v195, 1);
                                        const double v197 = static_cast<double>(v196);
                                        const double v198 = (v197 / 0x10000000000000p-40);
                                        const double v199 = (v154 - v198);
                                        const double v200 = (0x10000000000000p-53 / 0x10000000000000p-42);
                                        const bool v201 = (v200 < v199);
                                        bool v206;
                                        size_t v207;
                                        if (v201)
                                        {
                                            v206 = false;
                                            v207 = size_t{};
                                        }
                                        else
                                        {
                                            const double v202 = (-v199);
                                            const bool v203 = (v200 < v202);
                                            bool v204;
                                            size_t v205;
                                            if (v203)
                                            {
                                                v204 = false;
                                                v205 = size_t{};
                                            }
                                            else
                                            {
                                                v204 = true;
                                                v205 = v195;
                                            }

                                            v206 = v204;
                                            v207 = v205;
                                        }

                                        v208 = v206;
                                        v209 = v207;
                                    }
                                    else
                                    {
                                        v208 = false;
                                        v209 = size_t{};
                                    }

                                    v210 = v208;
                                    v211 = v209;
                                }
                                else
                                {
                                    v210 = false;
                                    v211 = size_t{};
                                }

                                if (v210)
                                {
                                    [[maybe_unused]] const auto v212 = state.SynthesisMagnitude(v2, v151.m_bin);
                                    [[maybe_unused]] const auto v213 = state.Coefficient(v2, v211);
                                    const bool v214 = (0.0 <= v31);
                                    double v215;
                                    if (v214)
                                    {
                                        v215 = v31;
                                    }
                                    else
                                    {
                                        v215 = 0.0;
                                    }

                                    const double v216 = (v215 * v42);
                                    const double v217 = (v216 * v212);
                                    [[maybe_unused]] const auto v218 = state.Polar(v217, v187);
                                    const double v219 = (v213.m_real + v218.m_real);
                                    const double v220 = (v213.m_imag + v218.m_imag);
                                    state.SetCoefficient(v2, v211, {v219, v220});
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
                const size_t v221 = input.m_count;
                struct loop_v222_Type
                {
                };

                [[maybe_unused]] loop_v222_Type loop_v222;
                for (std::size_t v223 = 0; v223 < v221; ++v223)
                {
                    [[maybe_unused]] const auto v224 = state.Drive(v223);
                    [[maybe_unused]] const auto v225 = state.GraphSize(v223);
                    struct loop_v226_Type
                    {
                    };

                    [[maybe_unused]] loop_v226_Type loop_v226;
                    for (std::size_t v227 = 0; v227 < v225; ++v227)
                    {
                        [[maybe_unused]] const auto v228 = state.VisitAt(v223, v227);
                        const bool v229 = (v228.m_link.m_root == v228.m_bin);
                        const bool v230 = (v229 == true);
                        if (v230)
                        {
                            [[maybe_unused]] const auto v231 = state.Phase(v223, input.m_oscillator, v228.m_bin);
                            [[maybe_unused]] const auto v232 = state.AnalysisPhase(v223, v228.m_bin);
                            [[maybe_unused]] const auto v233 = state.Gate(v223, v228.m_bin);
                            bool v235;
                            if (v233 == 0u)
                            {
                                v235 = false;
                            }
                            else
                            {
                                bool v234;
                                if (v233 == 1u)
                                {
                                    v234 = true;
                                }
                                else
                                {
                                    v234 = false;
                                }

                                v235 = v234;
                            }

                            const bool v236 = (v235 == true);
                            double v239;
                            if (v236)
                            {
                                v239 = v232;
                            }
                            else
                            {
                                const double v237 = (v228.m_link.m_delta * v224);
                                const double v238 = (v231 + v237);
                                v239 = v238;
                            }

                            state.SetPhase(v223, input.m_oscillator, v228.m_bin, v239);
                        }
                        else
                        {
                            [[maybe_unused]] const auto v240 = state.Phase(v223, input.m_oscillator, v228.m_link.m_root);
                            [[maybe_unused]] const auto v241 = state.Omega(v223, v228.m_link.m_root);
                            state.SetOmega(v223, v228.m_bin, v241);
                            const double v242 = (v240 + v228.m_link.m_delta);
                            state.SetPhase(v223, input.m_oscillator, v228.m_bin, v242);
                        }

                    }

                }

            }
            else
            {
                const size_t v243 = input.m_count;
                struct loop_v244_Type
                {
                };

                [[maybe_unused]] loop_v244_Type loop_v244;
                for (std::size_t v245 = 0; v245 < v243; ++v245)
                {
                    [[maybe_unused]] const auto v246 = state.Drive(v245);
                    struct loop_v247_Type
                    {
                    };

                    [[maybe_unused]] loop_v247_Type loop_v247;
                    for (std::size_t v248 = 0; v248 < 2047; ++v248)
                    {
                        [[maybe_unused]] const auto v249 = state.SynthesisMagnitude(v245, v248);
                        [[maybe_unused]] const auto v250 = state.Phase(v245, input.m_oscillator, v248);
                        [[maybe_unused]] const auto v251 = state.Coefficient(v245, v248);
                        const double v252 = (v246 * v249);
                        [[maybe_unused]] const auto v253 = state.Polar(v252, v250);
                        const double v254 = (v251.m_real + v253.m_real);
                        const double v255 = (v251.m_imag + v253.m_imag);
                        state.SetCoefficient(v245, v248, {v254, v255});
                    }

                }

            }

        }

    }

};
