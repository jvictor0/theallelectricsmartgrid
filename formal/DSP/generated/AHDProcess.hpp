#pragma once

// Generated from DSP.AHD.processFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct AHDProcess
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Input
    {
        double m_phaseDistance;
        double m_phaseMagnitude;
        double m_phaseRatio;
        double m_period;
        double m_attackIncrement;
        double m_holdLoops;
        double m_decayIncrement;
        double m_basicPhaseIncrement;
        double m_releaseFactor;
        bool m_trigger;
        bool m_reverseTrigger;
        double m_forwardStartOutput;
        bool m_releaseRequested;
    };

    struct Output
    {
        bool m_idle;
        bool m_releasing;
    };

    template<typename State>
    static Output Process(State& state, [[maybe_unused]] const Input& input)
    {
        bool v0;
        if (input.m_trigger)
        {
            v0 = true;
        }
        else
        {
            v0 = input.m_reverseTrigger;
        }

        bool v1;
        if (v0)
        {
            v1 = true;
        }
        else
        {
            v1 = input.m_releaseRequested;
        }

        const bool v2 = (v1 == true);
        bool v642;
        bool v643;
        if (v2)
        {
            const bool v3 = (input.m_trigger == true);
            if (v3)
            {
                const float v4 = static_cast<float>(state.m_rawOutput);
                const double v5 = static_cast<double>(v4);
                const size_t v6 = 1;
                const double v7 = 0.0;
                const bool v8 = false;
                const bool v9 = false;
                state.m_stage = v6;
                state.m_position = v5;
                state.m_previousSamples = v7;
                state.m_startOutput = v5;
                state.m_armed = v8;
                state.m_forwardOnly = v9;
            }
            else
            {
                const bool v10 = (input.m_reverseTrigger == true);
                if (v10)
                {
                    const double v11 = (1.0 - input.m_forwardStartOutput);
                    const double v12 = (v11 / input.m_attackIncrement);
                    const double v13 = (input.m_holdLoops * input.m_period);
                    const double v14 = (v12 + v13);
                    const double v15 = (1.0 / input.m_decayIncrement);
                    const double v16 = (v14 + v15);
                    const size_t v17 = 0;
                    const double v18 = input.m_forwardStartOutput;
                    const double v19 = 0.0;
                    const bool v20 = true;
                    const bool v21 = true;
                    state.m_stage = v17;
                    state.m_startOutput = v18;
                    state.m_rawOutput = v19;
                    state.m_endSamples = v16;
                    state.m_armed = v20;
                    state.m_forwardOnly = v21;
                }
                else
                {
                    const bool v22 = (input.m_releaseRequested == true);
                    if (v22)
                    {
                        const bool v23 = (state.m_stage == 0);
                        bool v24;
                        if (v23)
                        {
                            v24 = true;
                        }
                        else
                        {
                            v24 = false;
                        }

                        const bool v25 = (v24 == true);
                        size_t v26;
                        if (v25)
                        {
                            v26 = 0;
                        }
                        else
                        {
                            v26 = 4;
                        }

                        const bool v27 = false;
                        state.m_stage = v26;
                        state.m_armed = v27;
                    }
                    else
                    {
                    }

                }

            }

            const size_t v28 = state.m_stage;
            const bool v36 = (v28 == 4);
            bool v37;
            if (v36)
            {
                v37 = true;
            }
            else
            {
                v37 = false;
            }

            const bool v38 = (v37 == true);
            bool v333;
            bool v334;
            if (v38)
            {
                const double v43 = state.m_rawOutput;
                const double v47 = (input.m_basicPhaseIncrement * input.m_phaseRatio);
                const double v48 = (v47 * input.m_period);
                const double v49 = (v48 * input.m_releaseFactor);
                const double v50 = (input.m_decayIncrement * v49);
                const double v51 = (v43 - v50);
                const bool v52 = (v51 <= 0.0);
                if (v52)
                {
                    const size_t v53 = 0;
                    const double v54 = 0.0;
                    state.m_stage = v53;
                    state.m_rawOutput = v54;
                }
                else
                {
                    state.m_rawOutput = v51;
                }

                const size_t v55 = state.m_stage;
                const bool v63 = (v55 == 0);
                bool v64;
                if (v63)
                {
                    v64 = true;
                }
                else
                {
                    v64 = false;
                }

                const bool v65 = (v55 == 4);
                bool v66;
                if (v65)
                {
                    v66 = true;
                }
                else
                {
                    v66 = false;
                }

                v333 = v64;
                v334 = v66;
            }
            else
            {
                const size_t v67 = state.m_stage;
                const double v69 = state.m_previousSamples;
                const double v72 = state.m_endSamples;
                const bool v73 = state.m_armed;
                const bool v74 = state.m_forwardOnly;
                const bool v75 = (v67 == 0);
                bool v76;
                if (v75)
                {
                    v76 = true;
                }
                else
                {
                    v76 = false;
                }

                bool v78;
                if (v76)
                {
                    const bool v77 = !v73;
                    v78 = v77;
                }
                else
                {
                    v78 = false;
                }

                const bool v79 = (v78 == true);
                if (v79)
                {
                }
                else
                {
                    const bool v80 = (v74 == true);
                    double v81;
                    if (v80)
                    {
                        v81 = input.m_phaseDistance;
                    }
                    else
                    {
                        v81 = input.m_phaseMagnitude;
                    }

                    const double v82 = (v81 * input.m_phaseRatio);
                    const double v83 = (v82 * input.m_period);
                    bool v86;
                    if (v74)
                    {
                        const bool v84 = (v83 < 0.0);
                        bool v85;
                        if (v84)
                        {
                            v85 = true;
                        }
                        else
                        {
                            v85 = false;
                        }

                        v86 = v85;
                    }
                    else
                    {
                        v86 = false;
                    }

                    const bool v87 = (v86 == true);
                    if (v87)
                    {
                        const size_t v88 = 0;
                        const double v89 = 0.0;
                        const bool v90 = false;
                        state.m_stage = v88;
                        state.m_rawOutput = v89;
                        state.m_armed = v90;
                    }
                    else
                    {
                        const bool v91 = (v67 == 0);
                        bool v92;
                        if (v91)
                        {
                            v92 = true;
                        }
                        else
                        {
                            v92 = false;
                        }

                        const bool v93 = (v92 == true);
                        if (v93)
                        {
                            const double v94 = (v83 - v72);
                            const bool v95 = (v94 < 0.0);
                            if (v95)
                            {
                                const size_t v96 = 3;
                                const double v97 = 1.0;
                                const double v98 = 0.0;
                                const bool v99 = false;
                                state.m_stage = v96;
                                state.m_position = v97;
                                state.m_rawOutput = v98;
                                state.m_armed = v99;
                                [[maybe_unused]] double loop_v100;
                                loop_v100 = v94;
                                for (std::size_t v101 = 0; v101 < 3; ++v101)
                                {
                                    const bool v102 = (loop_v100 <= 0.0);
                                    bool v103;
                                    if (v102)
                                    {
                                        v103 = true;
                                    }
                                    else
                                    {
                                        v103 = false;
                                    }

                                    bool v106;
                                    if (v103)
                                    {
                                        const bool v104 = (0.0 <= loop_v100);
                                        bool v105;
                                        if (v104)
                                        {
                                            v105 = true;
                                        }
                                        else
                                        {
                                            v105 = false;
                                        }

                                        v106 = v105;
                                    }
                                    else
                                    {
                                        v106 = false;
                                    }

                                    const bool v107 = (v106 == true);
                                    bool v193;
                                    double v194;
                                    if (v107)
                                    {
                                        v193 = true;
                                        v194 = 0.0;
                                    }
                                    else
                                    {
                                        const size_t v108 = state.m_stage;
                                        const double v109 = state.m_position;
                                        const double v111 = state.m_startOutput;
                                        const bool v116 = (0.0 < loop_v100);
                                        bool v117;
                                        if (v116)
                                        {
                                            v117 = true;
                                        }
                                        else
                                        {
                                            v117 = false;
                                        }

                                        const double v118 = (input.m_holdLoops * input.m_period);
                                        const bool v119 = (v108 == 2);
                                        bool v120;
                                        if (v119)
                                        {
                                            v120 = true;
                                        }
                                        else
                                        {
                                            v120 = false;
                                        }

                                        bool v123;
                                        if (v120)
                                        {
                                            const bool v121 = (v118 <= 0.0);
                                            bool v122;
                                            if (v121)
                                            {
                                                v122 = true;
                                            }
                                            else
                                            {
                                                v122 = false;
                                            }

                                            v123 = v122;
                                        }
                                        else
                                        {
                                            v123 = false;
                                        }

                                        const bool v124 = (v123 == true);
                                        bool v191;
                                        double v192;
                                        if (v124)
                                        {
                                            const bool v125 = (v117 == true);
                                            size_t v126;
                                            if (v125)
                                            {
                                                v126 = 3;
                                            }
                                            else
                                            {
                                                v126 = 1;
                                            }

                                            const bool v127 = (v117 == true);
                                            double v128;
                                            if (v127)
                                            {
                                                v128 = 0.0;
                                            }
                                            else
                                            {
                                                v128 = 1.0;
                                            }

                                            state.m_stage = v126;
                                            state.m_position = v128;
                                            v191 = false;
                                            v192 = loop_v100;
                                        }
                                        else
                                        {
                                            const bool v129 = (v108 == 1);
                                            bool v130;
                                            if (v129)
                                            {
                                                v130 = true;
                                            }
                                            else
                                            {
                                                v130 = false;
                                            }

                                            const bool v131 = (v130 == true);
                                            double v137;
                                            if (v131)
                                            {
                                                v137 = input.m_attackIncrement;
                                            }
                                            else
                                            {
                                                const bool v132 = (v108 == 2);
                                                bool v133;
                                                if (v132)
                                                {
                                                    v133 = true;
                                                }
                                                else
                                                {
                                                    v133 = false;
                                                }

                                                const bool v134 = (v133 == true);
                                                double v136;
                                                if (v134)
                                                {
                                                    const double v135 = (1.0 / v118);
                                                    v136 = v135;
                                                }
                                                else
                                                {
                                                    v136 = input.m_decayIncrement;
                                                }

                                                v137 = v136;
                                            }

                                            const bool v138 = (v137 <= 0.0);
                                            bool v189;
                                            double v190;
                                            if (v138)
                                            {
                                                v189 = true;
                                                v190 = 0.0;
                                            }
                                            else
                                            {
                                                const double v139 = (loop_v100 * v137);
                                                const double v140 = (v109 + v139);
                                                const bool v141 = !v117;
                                                bool v144;
                                                if (v141)
                                                {
                                                    const bool v142 = (v108 == 1);
                                                    bool v143;
                                                    if (v142)
                                                    {
                                                        v143 = true;
                                                    }
                                                    else
                                                    {
                                                        v143 = false;
                                                    }

                                                    v144 = v143;
                                                }
                                                else
                                                {
                                                    v144 = false;
                                                }

                                                const bool v145 = (v144 == true);
                                                bool v187;
                                                double v188;
                                                if (v145)
                                                {
                                                    const bool v146 = (v140 < v111);
                                                    double v147;
                                                    if (v146)
                                                    {
                                                        v147 = v111;
                                                    }
                                                    else
                                                    {
                                                        v147 = v140;
                                                    }

                                                    state.m_position = v147;
                                                    v187 = true;
                                                    v188 = 0.0;
                                                }
                                                else
                                                {
                                                    const bool v148 = (v117 == true);
                                                    bool v153;
                                                    if (v148)
                                                    {
                                                        const bool v149 = (v140 < 1.0);
                                                        bool v150;
                                                        if (v149)
                                                        {
                                                            v150 = true;
                                                        }
                                                        else
                                                        {
                                                            v150 = false;
                                                        }

                                                        v153 = v150;
                                                    }
                                                    else
                                                    {
                                                        const bool v151 = (0.0 < v140);
                                                        bool v152;
                                                        if (v151)
                                                        {
                                                            v152 = true;
                                                        }
                                                        else
                                                        {
                                                            v152 = false;
                                                        }

                                                        v153 = v152;
                                                    }

                                                    const bool v154 = (v153 == true);
                                                    bool v185;
                                                    double v186;
                                                    if (v154)
                                                    {
                                                        state.m_position = v140;
                                                        v185 = true;
                                                        v186 = 0.0;
                                                    }
                                                    else
                                                    {
                                                        const bool v155 = (v117 == true);
                                                        double v156;
                                                        if (v155)
                                                        {
                                                            v156 = 1.0;
                                                        }
                                                        else
                                                        {
                                                            v156 = 0.0;
                                                        }

                                                        const double v157 = (v156 - v109);
                                                        const double v158 = (v157 / v137);
                                                        const double v159 = (loop_v100 - v158);
                                                        const bool v160 = (v117 == true);
                                                        double v165;
                                                        if (v160)
                                                        {
                                                            const bool v161 = (v159 < 0.0);
                                                            double v162;
                                                            if (v161)
                                                            {
                                                                v162 = 0.0;
                                                            }
                                                            else
                                                            {
                                                                v162 = v159;
                                                            }

                                                            v165 = v162;
                                                        }
                                                        else
                                                        {
                                                            const bool v163 = (0.0 < v159);
                                                            double v164;
                                                            if (v163)
                                                            {
                                                                v164 = 0.0;
                                                            }
                                                            else
                                                            {
                                                                v164 = v159;
                                                            }

                                                            v165 = v164;
                                                        }

                                                        bool v168;
                                                        if (v117)
                                                        {
                                                            const bool v166 = (v108 == 3);
                                                            bool v167;
                                                            if (v166)
                                                            {
                                                                v167 = true;
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

                                                        const bool v169 = (v168 == true);
                                                        bool v183;
                                                        double v184;
                                                        if (v169)
                                                        {
                                                            const double v170 = (v83 - v165);
                                                            const size_t v171 = 0;
                                                            const double v172 = 0.0;
                                                            const double v173 = 0.0;
                                                            const bool v174 = true;
                                                            state.m_stage = v171;
                                                            state.m_position = v172;
                                                            state.m_rawOutput = v173;
                                                            state.m_endSamples = v170;
                                                            state.m_armed = v174;
                                                            v183 = true;
                                                            v184 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            const bool v175 = (v108 == 2);
                                                            bool v176;
                                                            if (v175)
                                                            {
                                                                v176 = true;
                                                            }
                                                            else
                                                            {
                                                                v176 = false;
                                                            }

                                                            const bool v177 = (v176 == true);
                                                            size_t v180;
                                                            if (v177)
                                                            {
                                                                const bool v178 = (v117 == true);
                                                                size_t v179;
                                                                if (v178)
                                                                {
                                                                    v179 = 3;
                                                                }
                                                                else
                                                                {
                                                                    v179 = 1;
                                                                }

                                                                v180 = v179;
                                                            }
                                                            else
                                                            {
                                                                v180 = 2;
                                                            }

                                                            const bool v181 = (v117 == true);
                                                            double v182;
                                                            if (v181)
                                                            {
                                                                v182 = 0.0;
                                                            }
                                                            else
                                                            {
                                                                v182 = 1.0;
                                                            }

                                                            state.m_stage = v180;
                                                            state.m_position = v182;
                                                            v183 = false;
                                                            v184 = v165;
                                                        }

                                                        v185 = v183;
                                                        v186 = v184;
                                                    }

                                                    v187 = v185;
                                                    v188 = v186;
                                                }

                                                v189 = v187;
                                                v190 = v188;
                                            }

                                            v191 = v189;
                                            v192 = v190;
                                        }

                                        v193 = v191;
                                        v194 = v192;
                                    }

                                    loop_v100 = v194;
                                    if (v193)
                                    {
                                        break;
                                    }
                                }

                                const bool v195 = (state.m_stage == 1);
                                bool v196;
                                if (v195)
                                {
                                    v196 = true;
                                }
                                else
                                {
                                    v196 = false;
                                }

                                const bool v197 = (v196 == true);
                                double v207;
                                if (v197)
                                {
                                    v207 = state.m_position;
                                }
                                else
                                {
                                    const bool v198 = (state.m_stage == 2);
                                    bool v199;
                                    if (v198)
                                    {
                                        v199 = true;
                                    }
                                    else
                                    {
                                        v199 = false;
                                    }

                                    const bool v200 = (v199 == true);
                                    double v206;
                                    if (v200)
                                    {
                                        v206 = 1.0;
                                    }
                                    else
                                    {
                                        const bool v201 = (state.m_stage == 3);
                                        bool v202;
                                        if (v201)
                                        {
                                            v202 = true;
                                        }
                                        else
                                        {
                                            v202 = false;
                                        }

                                        const bool v203 = (v202 == true);
                                        double v205;
                                        if (v203)
                                        {
                                            const double v204 = (1.0 - state.m_position);
                                            v205 = v204;
                                        }
                                        else
                                        {
                                            v205 = 0.0;
                                        }

                                        v206 = v205;
                                    }

                                    v207 = v206;
                                }

                                const float v208 = static_cast<float>(v207);
                                const double v209 = static_cast<double>(v208);
                                state.m_previousSamples = v83;
                                state.m_rawOutput = v209;
                            }
                            else
                            {
                            }

                        }
                        else
                        {
                            const double v210 = (v83 - v69);
                            [[maybe_unused]] double loop_v211;
                            loop_v211 = v210;
                            for (std::size_t v212 = 0; v212 < 3; ++v212)
                            {
                                const bool v213 = (loop_v211 <= 0.0);
                                bool v214;
                                if (v213)
                                {
                                    v214 = true;
                                }
                                else
                                {
                                    v214 = false;
                                }

                                bool v217;
                                if (v214)
                                {
                                    const bool v215 = (0.0 <= loop_v211);
                                    bool v216;
                                    if (v215)
                                    {
                                        v216 = true;
                                    }
                                    else
                                    {
                                        v216 = false;
                                    }

                                    v217 = v216;
                                }
                                else
                                {
                                    v217 = false;
                                }

                                const bool v218 = (v217 == true);
                                bool v304;
                                double v305;
                                if (v218)
                                {
                                    v304 = true;
                                    v305 = 0.0;
                                }
                                else
                                {
                                    const size_t v219 = state.m_stage;
                                    const double v220 = state.m_position;
                                    const double v222 = state.m_startOutput;
                                    const bool v227 = (0.0 < loop_v211);
                                    bool v228;
                                    if (v227)
                                    {
                                        v228 = true;
                                    }
                                    else
                                    {
                                        v228 = false;
                                    }

                                    const double v229 = (input.m_holdLoops * input.m_period);
                                    const bool v230 = (v219 == 2);
                                    bool v231;
                                    if (v230)
                                    {
                                        v231 = true;
                                    }
                                    else
                                    {
                                        v231 = false;
                                    }

                                    bool v234;
                                    if (v231)
                                    {
                                        const bool v232 = (v229 <= 0.0);
                                        bool v233;
                                        if (v232)
                                        {
                                            v233 = true;
                                        }
                                        else
                                        {
                                            v233 = false;
                                        }

                                        v234 = v233;
                                    }
                                    else
                                    {
                                        v234 = false;
                                    }

                                    const bool v235 = (v234 == true);
                                    bool v302;
                                    double v303;
                                    if (v235)
                                    {
                                        const bool v236 = (v228 == true);
                                        size_t v237;
                                        if (v236)
                                        {
                                            v237 = 3;
                                        }
                                        else
                                        {
                                            v237 = 1;
                                        }

                                        const bool v238 = (v228 == true);
                                        double v239;
                                        if (v238)
                                        {
                                            v239 = 0.0;
                                        }
                                        else
                                        {
                                            v239 = 1.0;
                                        }

                                        state.m_stage = v237;
                                        state.m_position = v239;
                                        v302 = false;
                                        v303 = loop_v211;
                                    }
                                    else
                                    {
                                        const bool v240 = (v219 == 1);
                                        bool v241;
                                        if (v240)
                                        {
                                            v241 = true;
                                        }
                                        else
                                        {
                                            v241 = false;
                                        }

                                        const bool v242 = (v241 == true);
                                        double v248;
                                        if (v242)
                                        {
                                            v248 = input.m_attackIncrement;
                                        }
                                        else
                                        {
                                            const bool v243 = (v219 == 2);
                                            bool v244;
                                            if (v243)
                                            {
                                                v244 = true;
                                            }
                                            else
                                            {
                                                v244 = false;
                                            }

                                            const bool v245 = (v244 == true);
                                            double v247;
                                            if (v245)
                                            {
                                                const double v246 = (1.0 / v229);
                                                v247 = v246;
                                            }
                                            else
                                            {
                                                v247 = input.m_decayIncrement;
                                            }

                                            v248 = v247;
                                        }

                                        const bool v249 = (v248 <= 0.0);
                                        bool v300;
                                        double v301;
                                        if (v249)
                                        {
                                            v300 = true;
                                            v301 = 0.0;
                                        }
                                        else
                                        {
                                            const double v250 = (loop_v211 * v248);
                                            const double v251 = (v220 + v250);
                                            const bool v252 = !v228;
                                            bool v255;
                                            if (v252)
                                            {
                                                const bool v253 = (v219 == 1);
                                                bool v254;
                                                if (v253)
                                                {
                                                    v254 = true;
                                                }
                                                else
                                                {
                                                    v254 = false;
                                                }

                                                v255 = v254;
                                            }
                                            else
                                            {
                                                v255 = false;
                                            }

                                            const bool v256 = (v255 == true);
                                            bool v298;
                                            double v299;
                                            if (v256)
                                            {
                                                const bool v257 = (v251 < v222);
                                                double v258;
                                                if (v257)
                                                {
                                                    v258 = v222;
                                                }
                                                else
                                                {
                                                    v258 = v251;
                                                }

                                                state.m_position = v258;
                                                v298 = true;
                                                v299 = 0.0;
                                            }
                                            else
                                            {
                                                const bool v259 = (v228 == true);
                                                bool v264;
                                                if (v259)
                                                {
                                                    const bool v260 = (v251 < 1.0);
                                                    bool v261;
                                                    if (v260)
                                                    {
                                                        v261 = true;
                                                    }
                                                    else
                                                    {
                                                        v261 = false;
                                                    }

                                                    v264 = v261;
                                                }
                                                else
                                                {
                                                    const bool v262 = (0.0 < v251);
                                                    bool v263;
                                                    if (v262)
                                                    {
                                                        v263 = true;
                                                    }
                                                    else
                                                    {
                                                        v263 = false;
                                                    }

                                                    v264 = v263;
                                                }

                                                const bool v265 = (v264 == true);
                                                bool v296;
                                                double v297;
                                                if (v265)
                                                {
                                                    state.m_position = v251;
                                                    v296 = true;
                                                    v297 = 0.0;
                                                }
                                                else
                                                {
                                                    const bool v266 = (v228 == true);
                                                    double v267;
                                                    if (v266)
                                                    {
                                                        v267 = 1.0;
                                                    }
                                                    else
                                                    {
                                                        v267 = 0.0;
                                                    }

                                                    const double v268 = (v267 - v220);
                                                    const double v269 = (v268 / v248);
                                                    const double v270 = (loop_v211 - v269);
                                                    const bool v271 = (v228 == true);
                                                    double v276;
                                                    if (v271)
                                                    {
                                                        const bool v272 = (v270 < 0.0);
                                                        double v273;
                                                        if (v272)
                                                        {
                                                            v273 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            v273 = v270;
                                                        }

                                                        v276 = v273;
                                                    }
                                                    else
                                                    {
                                                        const bool v274 = (0.0 < v270);
                                                        double v275;
                                                        if (v274)
                                                        {
                                                            v275 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            v275 = v270;
                                                        }

                                                        v276 = v275;
                                                    }

                                                    bool v279;
                                                    if (v228)
                                                    {
                                                        const bool v277 = (v219 == 3);
                                                        bool v278;
                                                        if (v277)
                                                        {
                                                            v278 = true;
                                                        }
                                                        else
                                                        {
                                                            v278 = false;
                                                        }

                                                        v279 = v278;
                                                    }
                                                    else
                                                    {
                                                        v279 = false;
                                                    }

                                                    const bool v280 = (v279 == true);
                                                    bool v294;
                                                    double v295;
                                                    if (v280)
                                                    {
                                                        const double v281 = (v83 - v276);
                                                        const size_t v282 = 0;
                                                        const double v283 = 0.0;
                                                        const double v284 = 0.0;
                                                        const bool v285 = true;
                                                        state.m_stage = v282;
                                                        state.m_position = v283;
                                                        state.m_rawOutput = v284;
                                                        state.m_endSamples = v281;
                                                        state.m_armed = v285;
                                                        v294 = true;
                                                        v295 = 0.0;
                                                    }
                                                    else
                                                    {
                                                        const bool v286 = (v219 == 2);
                                                        bool v287;
                                                        if (v286)
                                                        {
                                                            v287 = true;
                                                        }
                                                        else
                                                        {
                                                            v287 = false;
                                                        }

                                                        const bool v288 = (v287 == true);
                                                        size_t v291;
                                                        if (v288)
                                                        {
                                                            const bool v289 = (v228 == true);
                                                            size_t v290;
                                                            if (v289)
                                                            {
                                                                v290 = 3;
                                                            }
                                                            else
                                                            {
                                                                v290 = 1;
                                                            }

                                                            v291 = v290;
                                                        }
                                                        else
                                                        {
                                                            v291 = 2;
                                                        }

                                                        const bool v292 = (v228 == true);
                                                        double v293;
                                                        if (v292)
                                                        {
                                                            v293 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            v293 = 1.0;
                                                        }

                                                        state.m_stage = v291;
                                                        state.m_position = v293;
                                                        v294 = false;
                                                        v295 = v276;
                                                    }

                                                    v296 = v294;
                                                    v297 = v295;
                                                }

                                                v298 = v296;
                                                v299 = v297;
                                            }

                                            v300 = v298;
                                            v301 = v299;
                                        }

                                        v302 = v300;
                                        v303 = v301;
                                    }

                                    v304 = v302;
                                    v305 = v303;
                                }

                                loop_v211 = v305;
                                if (v304)
                                {
                                    break;
                                }
                            }

                            const bool v306 = (state.m_stage == 1);
                            bool v307;
                            if (v306)
                            {
                                v307 = true;
                            }
                            else
                            {
                                v307 = false;
                            }

                            const bool v308 = (v307 == true);
                            double v318;
                            if (v308)
                            {
                                v318 = state.m_position;
                            }
                            else
                            {
                                const bool v309 = (state.m_stage == 2);
                                bool v310;
                                if (v309)
                                {
                                    v310 = true;
                                }
                                else
                                {
                                    v310 = false;
                                }

                                const bool v311 = (v310 == true);
                                double v317;
                                if (v311)
                                {
                                    v317 = 1.0;
                                }
                                else
                                {
                                    const bool v312 = (state.m_stage == 3);
                                    bool v313;
                                    if (v312)
                                    {
                                        v313 = true;
                                    }
                                    else
                                    {
                                        v313 = false;
                                    }

                                    const bool v314 = (v313 == true);
                                    double v316;
                                    if (v314)
                                    {
                                        const double v315 = (1.0 - state.m_position);
                                        v316 = v315;
                                    }
                                    else
                                    {
                                        v316 = 0.0;
                                    }

                                    v317 = v316;
                                }

                                v318 = v317;
                            }

                            const float v319 = static_cast<float>(v318);
                            const double v320 = static_cast<double>(v319);
                            state.m_previousSamples = v83;
                            state.m_rawOutput = v320;
                        }

                    }

                }

                const size_t v321 = state.m_stage;
                const bool v329 = (v321 == 0);
                bool v330;
                if (v329)
                {
                    v330 = true;
                }
                else
                {
                    v330 = false;
                }

                const bool v331 = (v321 == 4);
                bool v332;
                if (v331)
                {
                    v332 = true;
                }
                else
                {
                    v332 = false;
                }

                v333 = v330;
                v334 = v332;
            }

            v642 = v333;
            v643 = v334;
        }
        else
        {
            const size_t v335 = state.m_stage;
            const bool v343 = (v335 == 4);
            bool v344;
            if (v343)
            {
                v344 = true;
            }
            else
            {
                v344 = false;
            }

            const bool v345 = (v344 == true);
            bool v640;
            bool v641;
            if (v345)
            {
                const double v350 = state.m_rawOutput;
                const double v354 = (input.m_basicPhaseIncrement * input.m_phaseRatio);
                const double v355 = (v354 * input.m_period);
                const double v356 = (v355 * input.m_releaseFactor);
                const double v357 = (input.m_decayIncrement * v356);
                const double v358 = (v350 - v357);
                const bool v359 = (v358 <= 0.0);
                if (v359)
                {
                    const size_t v360 = 0;
                    const double v361 = 0.0;
                    state.m_stage = v360;
                    state.m_rawOutput = v361;
                }
                else
                {
                    state.m_rawOutput = v358;
                }

                const size_t v362 = state.m_stage;
                const bool v370 = (v362 == 0);
                bool v371;
                if (v370)
                {
                    v371 = true;
                }
                else
                {
                    v371 = false;
                }

                const bool v372 = (v362 == 4);
                bool v373;
                if (v372)
                {
                    v373 = true;
                }
                else
                {
                    v373 = false;
                }

                v640 = v371;
                v641 = v373;
            }
            else
            {
                const size_t v374 = state.m_stage;
                const double v376 = state.m_previousSamples;
                const double v379 = state.m_endSamples;
                const bool v380 = state.m_armed;
                const bool v381 = state.m_forwardOnly;
                const bool v382 = (v374 == 0);
                bool v383;
                if (v382)
                {
                    v383 = true;
                }
                else
                {
                    v383 = false;
                }

                bool v385;
                if (v383)
                {
                    const bool v384 = !v380;
                    v385 = v384;
                }
                else
                {
                    v385 = false;
                }

                const bool v386 = (v385 == true);
                if (v386)
                {
                }
                else
                {
                    const bool v387 = (v381 == true);
                    double v388;
                    if (v387)
                    {
                        v388 = input.m_phaseDistance;
                    }
                    else
                    {
                        v388 = input.m_phaseMagnitude;
                    }

                    const double v389 = (v388 * input.m_phaseRatio);
                    const double v390 = (v389 * input.m_period);
                    bool v393;
                    if (v381)
                    {
                        const bool v391 = (v390 < 0.0);
                        bool v392;
                        if (v391)
                        {
                            v392 = true;
                        }
                        else
                        {
                            v392 = false;
                        }

                        v393 = v392;
                    }
                    else
                    {
                        v393 = false;
                    }

                    const bool v394 = (v393 == true);
                    if (v394)
                    {
                        const size_t v395 = 0;
                        const double v396 = 0.0;
                        const bool v397 = false;
                        state.m_stage = v395;
                        state.m_rawOutput = v396;
                        state.m_armed = v397;
                    }
                    else
                    {
                        const bool v398 = (v374 == 0);
                        bool v399;
                        if (v398)
                        {
                            v399 = true;
                        }
                        else
                        {
                            v399 = false;
                        }

                        const bool v400 = (v399 == true);
                        if (v400)
                        {
                            const double v401 = (v390 - v379);
                            const bool v402 = (v401 < 0.0);
                            if (v402)
                            {
                                const size_t v403 = 3;
                                const double v404 = 1.0;
                                const double v405 = 0.0;
                                const bool v406 = false;
                                state.m_stage = v403;
                                state.m_position = v404;
                                state.m_rawOutput = v405;
                                state.m_armed = v406;
                                [[maybe_unused]] double loop_v407;
                                loop_v407 = v401;
                                for (std::size_t v408 = 0; v408 < 3; ++v408)
                                {
                                    const bool v409 = (loop_v407 <= 0.0);
                                    bool v410;
                                    if (v409)
                                    {
                                        v410 = true;
                                    }
                                    else
                                    {
                                        v410 = false;
                                    }

                                    bool v413;
                                    if (v410)
                                    {
                                        const bool v411 = (0.0 <= loop_v407);
                                        bool v412;
                                        if (v411)
                                        {
                                            v412 = true;
                                        }
                                        else
                                        {
                                            v412 = false;
                                        }

                                        v413 = v412;
                                    }
                                    else
                                    {
                                        v413 = false;
                                    }

                                    const bool v414 = (v413 == true);
                                    bool v500;
                                    double v501;
                                    if (v414)
                                    {
                                        v500 = true;
                                        v501 = 0.0;
                                    }
                                    else
                                    {
                                        const size_t v415 = state.m_stage;
                                        const double v416 = state.m_position;
                                        const double v418 = state.m_startOutput;
                                        const bool v423 = (0.0 < loop_v407);
                                        bool v424;
                                        if (v423)
                                        {
                                            v424 = true;
                                        }
                                        else
                                        {
                                            v424 = false;
                                        }

                                        const double v425 = (input.m_holdLoops * input.m_period);
                                        const bool v426 = (v415 == 2);
                                        bool v427;
                                        if (v426)
                                        {
                                            v427 = true;
                                        }
                                        else
                                        {
                                            v427 = false;
                                        }

                                        bool v430;
                                        if (v427)
                                        {
                                            const bool v428 = (v425 <= 0.0);
                                            bool v429;
                                            if (v428)
                                            {
                                                v429 = true;
                                            }
                                            else
                                            {
                                                v429 = false;
                                            }

                                            v430 = v429;
                                        }
                                        else
                                        {
                                            v430 = false;
                                        }

                                        const bool v431 = (v430 == true);
                                        bool v498;
                                        double v499;
                                        if (v431)
                                        {
                                            const bool v432 = (v424 == true);
                                            size_t v433;
                                            if (v432)
                                            {
                                                v433 = 3;
                                            }
                                            else
                                            {
                                                v433 = 1;
                                            }

                                            const bool v434 = (v424 == true);
                                            double v435;
                                            if (v434)
                                            {
                                                v435 = 0.0;
                                            }
                                            else
                                            {
                                                v435 = 1.0;
                                            }

                                            state.m_stage = v433;
                                            state.m_position = v435;
                                            v498 = false;
                                            v499 = loop_v407;
                                        }
                                        else
                                        {
                                            const bool v436 = (v415 == 1);
                                            bool v437;
                                            if (v436)
                                            {
                                                v437 = true;
                                            }
                                            else
                                            {
                                                v437 = false;
                                            }

                                            const bool v438 = (v437 == true);
                                            double v444;
                                            if (v438)
                                            {
                                                v444 = input.m_attackIncrement;
                                            }
                                            else
                                            {
                                                const bool v439 = (v415 == 2);
                                                bool v440;
                                                if (v439)
                                                {
                                                    v440 = true;
                                                }
                                                else
                                                {
                                                    v440 = false;
                                                }

                                                const bool v441 = (v440 == true);
                                                double v443;
                                                if (v441)
                                                {
                                                    const double v442 = (1.0 / v425);
                                                    v443 = v442;
                                                }
                                                else
                                                {
                                                    v443 = input.m_decayIncrement;
                                                }

                                                v444 = v443;
                                            }

                                            const bool v445 = (v444 <= 0.0);
                                            bool v496;
                                            double v497;
                                            if (v445)
                                            {
                                                v496 = true;
                                                v497 = 0.0;
                                            }
                                            else
                                            {
                                                const double v446 = (loop_v407 * v444);
                                                const double v447 = (v416 + v446);
                                                const bool v448 = !v424;
                                                bool v451;
                                                if (v448)
                                                {
                                                    const bool v449 = (v415 == 1);
                                                    bool v450;
                                                    if (v449)
                                                    {
                                                        v450 = true;
                                                    }
                                                    else
                                                    {
                                                        v450 = false;
                                                    }

                                                    v451 = v450;
                                                }
                                                else
                                                {
                                                    v451 = false;
                                                }

                                                const bool v452 = (v451 == true);
                                                bool v494;
                                                double v495;
                                                if (v452)
                                                {
                                                    const bool v453 = (v447 < v418);
                                                    double v454;
                                                    if (v453)
                                                    {
                                                        v454 = v418;
                                                    }
                                                    else
                                                    {
                                                        v454 = v447;
                                                    }

                                                    state.m_position = v454;
                                                    v494 = true;
                                                    v495 = 0.0;
                                                }
                                                else
                                                {
                                                    const bool v455 = (v424 == true);
                                                    bool v460;
                                                    if (v455)
                                                    {
                                                        const bool v456 = (v447 < 1.0);
                                                        bool v457;
                                                        if (v456)
                                                        {
                                                            v457 = true;
                                                        }
                                                        else
                                                        {
                                                            v457 = false;
                                                        }

                                                        v460 = v457;
                                                    }
                                                    else
                                                    {
                                                        const bool v458 = (0.0 < v447);
                                                        bool v459;
                                                        if (v458)
                                                        {
                                                            v459 = true;
                                                        }
                                                        else
                                                        {
                                                            v459 = false;
                                                        }

                                                        v460 = v459;
                                                    }

                                                    const bool v461 = (v460 == true);
                                                    bool v492;
                                                    double v493;
                                                    if (v461)
                                                    {
                                                        state.m_position = v447;
                                                        v492 = true;
                                                        v493 = 0.0;
                                                    }
                                                    else
                                                    {
                                                        const bool v462 = (v424 == true);
                                                        double v463;
                                                        if (v462)
                                                        {
                                                            v463 = 1.0;
                                                        }
                                                        else
                                                        {
                                                            v463 = 0.0;
                                                        }

                                                        const double v464 = (v463 - v416);
                                                        const double v465 = (v464 / v444);
                                                        const double v466 = (loop_v407 - v465);
                                                        const bool v467 = (v424 == true);
                                                        double v472;
                                                        if (v467)
                                                        {
                                                            const bool v468 = (v466 < 0.0);
                                                            double v469;
                                                            if (v468)
                                                            {
                                                                v469 = 0.0;
                                                            }
                                                            else
                                                            {
                                                                v469 = v466;
                                                            }

                                                            v472 = v469;
                                                        }
                                                        else
                                                        {
                                                            const bool v470 = (0.0 < v466);
                                                            double v471;
                                                            if (v470)
                                                            {
                                                                v471 = 0.0;
                                                            }
                                                            else
                                                            {
                                                                v471 = v466;
                                                            }

                                                            v472 = v471;
                                                        }

                                                        bool v475;
                                                        if (v424)
                                                        {
                                                            const bool v473 = (v415 == 3);
                                                            bool v474;
                                                            if (v473)
                                                            {
                                                                v474 = true;
                                                            }
                                                            else
                                                            {
                                                                v474 = false;
                                                            }

                                                            v475 = v474;
                                                        }
                                                        else
                                                        {
                                                            v475 = false;
                                                        }

                                                        const bool v476 = (v475 == true);
                                                        bool v490;
                                                        double v491;
                                                        if (v476)
                                                        {
                                                            const double v477 = (v390 - v472);
                                                            const size_t v478 = 0;
                                                            const double v479 = 0.0;
                                                            const double v480 = 0.0;
                                                            const bool v481 = true;
                                                            state.m_stage = v478;
                                                            state.m_position = v479;
                                                            state.m_rawOutput = v480;
                                                            state.m_endSamples = v477;
                                                            state.m_armed = v481;
                                                            v490 = true;
                                                            v491 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            const bool v482 = (v415 == 2);
                                                            bool v483;
                                                            if (v482)
                                                            {
                                                                v483 = true;
                                                            }
                                                            else
                                                            {
                                                                v483 = false;
                                                            }

                                                            const bool v484 = (v483 == true);
                                                            size_t v487;
                                                            if (v484)
                                                            {
                                                                const bool v485 = (v424 == true);
                                                                size_t v486;
                                                                if (v485)
                                                                {
                                                                    v486 = 3;
                                                                }
                                                                else
                                                                {
                                                                    v486 = 1;
                                                                }

                                                                v487 = v486;
                                                            }
                                                            else
                                                            {
                                                                v487 = 2;
                                                            }

                                                            const bool v488 = (v424 == true);
                                                            double v489;
                                                            if (v488)
                                                            {
                                                                v489 = 0.0;
                                                            }
                                                            else
                                                            {
                                                                v489 = 1.0;
                                                            }

                                                            state.m_stage = v487;
                                                            state.m_position = v489;
                                                            v490 = false;
                                                            v491 = v472;
                                                        }

                                                        v492 = v490;
                                                        v493 = v491;
                                                    }

                                                    v494 = v492;
                                                    v495 = v493;
                                                }

                                                v496 = v494;
                                                v497 = v495;
                                            }

                                            v498 = v496;
                                            v499 = v497;
                                        }

                                        v500 = v498;
                                        v501 = v499;
                                    }

                                    loop_v407 = v501;
                                    if (v500)
                                    {
                                        break;
                                    }
                                }

                                const bool v502 = (state.m_stage == 1);
                                bool v503;
                                if (v502)
                                {
                                    v503 = true;
                                }
                                else
                                {
                                    v503 = false;
                                }

                                const bool v504 = (v503 == true);
                                double v514;
                                if (v504)
                                {
                                    v514 = state.m_position;
                                }
                                else
                                {
                                    const bool v505 = (state.m_stage == 2);
                                    bool v506;
                                    if (v505)
                                    {
                                        v506 = true;
                                    }
                                    else
                                    {
                                        v506 = false;
                                    }

                                    const bool v507 = (v506 == true);
                                    double v513;
                                    if (v507)
                                    {
                                        v513 = 1.0;
                                    }
                                    else
                                    {
                                        const bool v508 = (state.m_stage == 3);
                                        bool v509;
                                        if (v508)
                                        {
                                            v509 = true;
                                        }
                                        else
                                        {
                                            v509 = false;
                                        }

                                        const bool v510 = (v509 == true);
                                        double v512;
                                        if (v510)
                                        {
                                            const double v511 = (1.0 - state.m_position);
                                            v512 = v511;
                                        }
                                        else
                                        {
                                            v512 = 0.0;
                                        }

                                        v513 = v512;
                                    }

                                    v514 = v513;
                                }

                                const float v515 = static_cast<float>(v514);
                                const double v516 = static_cast<double>(v515);
                                state.m_previousSamples = v390;
                                state.m_rawOutput = v516;
                            }
                            else
                            {
                            }

                        }
                        else
                        {
                            const double v517 = (v390 - v376);
                            [[maybe_unused]] double loop_v518;
                            loop_v518 = v517;
                            for (std::size_t v519 = 0; v519 < 3; ++v519)
                            {
                                const bool v520 = (loop_v518 <= 0.0);
                                bool v521;
                                if (v520)
                                {
                                    v521 = true;
                                }
                                else
                                {
                                    v521 = false;
                                }

                                bool v524;
                                if (v521)
                                {
                                    const bool v522 = (0.0 <= loop_v518);
                                    bool v523;
                                    if (v522)
                                    {
                                        v523 = true;
                                    }
                                    else
                                    {
                                        v523 = false;
                                    }

                                    v524 = v523;
                                }
                                else
                                {
                                    v524 = false;
                                }

                                const bool v525 = (v524 == true);
                                bool v611;
                                double v612;
                                if (v525)
                                {
                                    v611 = true;
                                    v612 = 0.0;
                                }
                                else
                                {
                                    const size_t v526 = state.m_stage;
                                    const double v527 = state.m_position;
                                    const double v529 = state.m_startOutput;
                                    const bool v534 = (0.0 < loop_v518);
                                    bool v535;
                                    if (v534)
                                    {
                                        v535 = true;
                                    }
                                    else
                                    {
                                        v535 = false;
                                    }

                                    const double v536 = (input.m_holdLoops * input.m_period);
                                    const bool v537 = (v526 == 2);
                                    bool v538;
                                    if (v537)
                                    {
                                        v538 = true;
                                    }
                                    else
                                    {
                                        v538 = false;
                                    }

                                    bool v541;
                                    if (v538)
                                    {
                                        const bool v539 = (v536 <= 0.0);
                                        bool v540;
                                        if (v539)
                                        {
                                            v540 = true;
                                        }
                                        else
                                        {
                                            v540 = false;
                                        }

                                        v541 = v540;
                                    }
                                    else
                                    {
                                        v541 = false;
                                    }

                                    const bool v542 = (v541 == true);
                                    bool v609;
                                    double v610;
                                    if (v542)
                                    {
                                        const bool v543 = (v535 == true);
                                        size_t v544;
                                        if (v543)
                                        {
                                            v544 = 3;
                                        }
                                        else
                                        {
                                            v544 = 1;
                                        }

                                        const bool v545 = (v535 == true);
                                        double v546;
                                        if (v545)
                                        {
                                            v546 = 0.0;
                                        }
                                        else
                                        {
                                            v546 = 1.0;
                                        }

                                        state.m_stage = v544;
                                        state.m_position = v546;
                                        v609 = false;
                                        v610 = loop_v518;
                                    }
                                    else
                                    {
                                        const bool v547 = (v526 == 1);
                                        bool v548;
                                        if (v547)
                                        {
                                            v548 = true;
                                        }
                                        else
                                        {
                                            v548 = false;
                                        }

                                        const bool v549 = (v548 == true);
                                        double v555;
                                        if (v549)
                                        {
                                            v555 = input.m_attackIncrement;
                                        }
                                        else
                                        {
                                            const bool v550 = (v526 == 2);
                                            bool v551;
                                            if (v550)
                                            {
                                                v551 = true;
                                            }
                                            else
                                            {
                                                v551 = false;
                                            }

                                            const bool v552 = (v551 == true);
                                            double v554;
                                            if (v552)
                                            {
                                                const double v553 = (1.0 / v536);
                                                v554 = v553;
                                            }
                                            else
                                            {
                                                v554 = input.m_decayIncrement;
                                            }

                                            v555 = v554;
                                        }

                                        const bool v556 = (v555 <= 0.0);
                                        bool v607;
                                        double v608;
                                        if (v556)
                                        {
                                            v607 = true;
                                            v608 = 0.0;
                                        }
                                        else
                                        {
                                            const double v557 = (loop_v518 * v555);
                                            const double v558 = (v527 + v557);
                                            const bool v559 = !v535;
                                            bool v562;
                                            if (v559)
                                            {
                                                const bool v560 = (v526 == 1);
                                                bool v561;
                                                if (v560)
                                                {
                                                    v561 = true;
                                                }
                                                else
                                                {
                                                    v561 = false;
                                                }

                                                v562 = v561;
                                            }
                                            else
                                            {
                                                v562 = false;
                                            }

                                            const bool v563 = (v562 == true);
                                            bool v605;
                                            double v606;
                                            if (v563)
                                            {
                                                const bool v564 = (v558 < v529);
                                                double v565;
                                                if (v564)
                                                {
                                                    v565 = v529;
                                                }
                                                else
                                                {
                                                    v565 = v558;
                                                }

                                                state.m_position = v565;
                                                v605 = true;
                                                v606 = 0.0;
                                            }
                                            else
                                            {
                                                const bool v566 = (v535 == true);
                                                bool v571;
                                                if (v566)
                                                {
                                                    const bool v567 = (v558 < 1.0);
                                                    bool v568;
                                                    if (v567)
                                                    {
                                                        v568 = true;
                                                    }
                                                    else
                                                    {
                                                        v568 = false;
                                                    }

                                                    v571 = v568;
                                                }
                                                else
                                                {
                                                    const bool v569 = (0.0 < v558);
                                                    bool v570;
                                                    if (v569)
                                                    {
                                                        v570 = true;
                                                    }
                                                    else
                                                    {
                                                        v570 = false;
                                                    }

                                                    v571 = v570;
                                                }

                                                const bool v572 = (v571 == true);
                                                bool v603;
                                                double v604;
                                                if (v572)
                                                {
                                                    state.m_position = v558;
                                                    v603 = true;
                                                    v604 = 0.0;
                                                }
                                                else
                                                {
                                                    const bool v573 = (v535 == true);
                                                    double v574;
                                                    if (v573)
                                                    {
                                                        v574 = 1.0;
                                                    }
                                                    else
                                                    {
                                                        v574 = 0.0;
                                                    }

                                                    const double v575 = (v574 - v527);
                                                    const double v576 = (v575 / v555);
                                                    const double v577 = (loop_v518 - v576);
                                                    const bool v578 = (v535 == true);
                                                    double v583;
                                                    if (v578)
                                                    {
                                                        const bool v579 = (v577 < 0.0);
                                                        double v580;
                                                        if (v579)
                                                        {
                                                            v580 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            v580 = v577;
                                                        }

                                                        v583 = v580;
                                                    }
                                                    else
                                                    {
                                                        const bool v581 = (0.0 < v577);
                                                        double v582;
                                                        if (v581)
                                                        {
                                                            v582 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            v582 = v577;
                                                        }

                                                        v583 = v582;
                                                    }

                                                    bool v586;
                                                    if (v535)
                                                    {
                                                        const bool v584 = (v526 == 3);
                                                        bool v585;
                                                        if (v584)
                                                        {
                                                            v585 = true;
                                                        }
                                                        else
                                                        {
                                                            v585 = false;
                                                        }

                                                        v586 = v585;
                                                    }
                                                    else
                                                    {
                                                        v586 = false;
                                                    }

                                                    const bool v587 = (v586 == true);
                                                    bool v601;
                                                    double v602;
                                                    if (v587)
                                                    {
                                                        const double v588 = (v390 - v583);
                                                        const size_t v589 = 0;
                                                        const double v590 = 0.0;
                                                        const double v591 = 0.0;
                                                        const bool v592 = true;
                                                        state.m_stage = v589;
                                                        state.m_position = v590;
                                                        state.m_rawOutput = v591;
                                                        state.m_endSamples = v588;
                                                        state.m_armed = v592;
                                                        v601 = true;
                                                        v602 = 0.0;
                                                    }
                                                    else
                                                    {
                                                        const bool v593 = (v526 == 2);
                                                        bool v594;
                                                        if (v593)
                                                        {
                                                            v594 = true;
                                                        }
                                                        else
                                                        {
                                                            v594 = false;
                                                        }

                                                        const bool v595 = (v594 == true);
                                                        size_t v598;
                                                        if (v595)
                                                        {
                                                            const bool v596 = (v535 == true);
                                                            size_t v597;
                                                            if (v596)
                                                            {
                                                                v597 = 3;
                                                            }
                                                            else
                                                            {
                                                                v597 = 1;
                                                            }

                                                            v598 = v597;
                                                        }
                                                        else
                                                        {
                                                            v598 = 2;
                                                        }

                                                        const bool v599 = (v535 == true);
                                                        double v600;
                                                        if (v599)
                                                        {
                                                            v600 = 0.0;
                                                        }
                                                        else
                                                        {
                                                            v600 = 1.0;
                                                        }

                                                        state.m_stage = v598;
                                                        state.m_position = v600;
                                                        v601 = false;
                                                        v602 = v583;
                                                    }

                                                    v603 = v601;
                                                    v604 = v602;
                                                }

                                                v605 = v603;
                                                v606 = v604;
                                            }

                                            v607 = v605;
                                            v608 = v606;
                                        }

                                        v609 = v607;
                                        v610 = v608;
                                    }

                                    v611 = v609;
                                    v612 = v610;
                                }

                                loop_v518 = v612;
                                if (v611)
                                {
                                    break;
                                }
                            }

                            const bool v613 = (state.m_stage == 1);
                            bool v614;
                            if (v613)
                            {
                                v614 = true;
                            }
                            else
                            {
                                v614 = false;
                            }

                            const bool v615 = (v614 == true);
                            double v625;
                            if (v615)
                            {
                                v625 = state.m_position;
                            }
                            else
                            {
                                const bool v616 = (state.m_stage == 2);
                                bool v617;
                                if (v616)
                                {
                                    v617 = true;
                                }
                                else
                                {
                                    v617 = false;
                                }

                                const bool v618 = (v617 == true);
                                double v624;
                                if (v618)
                                {
                                    v624 = 1.0;
                                }
                                else
                                {
                                    const bool v619 = (state.m_stage == 3);
                                    bool v620;
                                    if (v619)
                                    {
                                        v620 = true;
                                    }
                                    else
                                    {
                                        v620 = false;
                                    }

                                    const bool v621 = (v620 == true);
                                    double v623;
                                    if (v621)
                                    {
                                        const double v622 = (1.0 - state.m_position);
                                        v623 = v622;
                                    }
                                    else
                                    {
                                        v623 = 0.0;
                                    }

                                    v624 = v623;
                                }

                                v625 = v624;
                            }

                            const float v626 = static_cast<float>(v625);
                            const double v627 = static_cast<double>(v626);
                            state.m_previousSamples = v390;
                            state.m_rawOutput = v627;
                        }

                    }

                }

                const size_t v628 = state.m_stage;
                const bool v636 = (v628 == 0);
                bool v637;
                if (v636)
                {
                    v637 = true;
                }
                else
                {
                    v637 = false;
                }

                const bool v638 = (v628 == 4);
                bool v639;
                if (v638)
                {
                    v639 = true;
                }
                else
                {
                    v639 = false;
                }

                v640 = v637;
                v641 = v639;
            }

            v642 = v640;
            v643 = v641;
        }

        Output output;
        output.m_idle = v642;
        output.m_releasing = v643;
        return output;
    }

};
