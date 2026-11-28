#pragma once

// Generated from DSP.PolyXFader.processTrio. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "MultiChannelNumber.hpp"
#include "PolyXFaderTables.hpp"
struct PolyXFaderTrio
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        size_t m_size;
        MultiChannelNumber<float, 3> m_slope;
        MultiChannelNumber<float, 3> m_center;
        MultiChannelNumber<float, 3> m_totalWeight;
        std::array<MultiChannelNumber<float, 3>, 6> m_weights;
        std::array<MultiChannelNumber<float, 3>, 6> m_values;
        MultiChannelNumber<double, 3> m_mixOutput;
        float m_holdAlpha;
        MultiChannelNumber<float, 3> m_heldValue;
        MultiChannelNumber<float, 3> m_heldOutput;
        MultiChannelNumber<float, 3> m_rawOutput;
        MultiChannelNumber<float, 3> m_output;
    };

    struct Input
    {
        size_t m_size;
        double m_masterPhase;
        double m_phaseTravel;
        double m_maxSlewCycles;
        size_t m_slewDoublings;
        double m_slewStepScale;
        std::array<double, 6> m_cycleRatio;
        MultiChannelNumber<float, 3> m_phaseShift;
        MultiChannelNumber<float, 3> m_mult;
        float m_minWaveMultLimit;
        std::array<float, 6> m_waveMultLimits;
        MultiChannelNumber<float, 3> m_attackFrac;
        MultiChannelNumber<float, 3> m_shape;
        MultiChannelNumber<float, 3> m_slope;
        MultiChannelNumber<float, 3> m_center;
        std::array<MultiChannelNumber<float, 3>, 6> m_externalWeights;
        MultiChannelNumber<float, 3> m_trig;
        MultiChannelNumber<float, 3> m_shFade;
        MultiChannelNumber<float, 3> m_amplitude;
    };

    template<typename State>
    static void Process(State& state, [[maybe_unused]] const Input& input)
    {
        const size_t v0 = state.m_size;
        const MultiChannelNumber<float, 3> v1 = state.m_slope;
        const MultiChannelNumber<float, 3> v2 = state.m_center;
        const bool v12 = (input.m_size == v0);
        const bool v13 = !v12;
        bool v15;
        if (v13)
        {
            v15 = true;
        }
        else
        {
            const bool v14 = (input.m_slope != v1);
            v15 = v14;
        }

        bool v17;
        if (v15)
        {
            v17 = true;
        }
        else
        {
            const bool v16 = (input.m_center != v2);
            v17 = v16;
        }

        const bool v18 = (v17 == true);
        if (v18)
        {
            const float v19 = static_cast<float>(input.m_size);
            const MultiChannelNumber<float, 3> v20 = MultiChannelNumber<float, 3>::Splat(v19);
            const MultiChannelNumber<float, 3> v21 = (input.m_center * v20);
            const MultiChannelNumber<float, 3> v22 = (input.m_slope * v20);
            const MultiChannelNumber<float, 3> v23 = MultiChannelNumber<float, 3>::Splat(0x800000p-24f);
            const MultiChannelNumber<float, 3> v24 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
            const MultiChannelNumber<float, 3> v25 = MultiChannelNumber<float, 3>::Splat(1.0f);
            const MultiChannelNumber<float, 3> v26 = MultiChannelNumber<float, 3>::Splat(0x800000p-24f);
            const MultiChannelNumber<float, 3> v27 = MultiChannelNumber<float, 3>::Splat(0x800000p-24f);
            const MultiChannelNumber<float, 3> v28 = MultiChannelNumber<float, 3>::IfLess(v22, v26, v27, v22);
            const MultiChannelNumber<float, 3> v29 = (v25 / v28);
            const MultiChannelNumber<float, 3> v30 = MultiChannelNumber<float, 3>::IfLess(v22, v23, v24, v29);
            const size_t v31 = input.m_size;
            const MultiChannelNumber<float, 3> v32 = MultiChannelNumber<float, 3>::Splat(0.0f);
            [[maybe_unused]] MultiChannelNumber<float, 3> loop_v33;
            loop_v33 = v32;
            for (std::size_t v34 = 0; v34 < v31; ++v34)
            {
                const float v35 = static_cast<float>(v34);
                const MultiChannelNumber<float, 3> v36 = MultiChannelNumber<float, 3>::Splat(v35);
                const MultiChannelNumber<float, 3> v37 = (v36 - v21);
                const MultiChannelNumber<float, 3> v38 = v37.Abs();
                const MultiChannelNumber<float, 3> v39 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v40 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v41 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v42 = MultiChannelNumber<float, 3>::Splat(0x800000p-25f);
                const MultiChannelNumber<float, 3> v43 = (v38 - v42);
                const MultiChannelNumber<float, 3> v44 = MultiChannelNumber<float, 3>::Splat(0x800000p-25f);
                const MultiChannelNumber<float, 3> v45 = (v38 - v44);
                const MultiChannelNumber<float, 3> v46 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v47 = MultiChannelNumber<float, 3>::IfLess(v41, v43, v45, v46);
                const MultiChannelNumber<float, 3> v48 = (v47 * v30);
                const MultiChannelNumber<float, 3> v49 = (v40 - v48);
                const MultiChannelNumber<float, 3> v50 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v51 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v52 = MultiChannelNumber<float, 3>::Splat(0x800000p-25f);
                const MultiChannelNumber<float, 3> v53 = (v38 - v52);
                const MultiChannelNumber<float, 3> v54 = MultiChannelNumber<float, 3>::Splat(0x800000p-25f);
                const MultiChannelNumber<float, 3> v55 = (v38 - v54);
                const MultiChannelNumber<float, 3> v56 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v57 = MultiChannelNumber<float, 3>::IfLess(v51, v53, v55, v56);
                const MultiChannelNumber<float, 3> v58 = (v57 * v30);
                const MultiChannelNumber<float, 3> v59 = (v50 - v58);
                const MultiChannelNumber<float, 3> v60 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v61 = MultiChannelNumber<float, 3>::IfLess(v39, v49, v59, v60);
                const bool v62 = (v34 == 0);
                const bool v63 = (v62 == true);
                MultiChannelNumber<float, 3> v67;
                if (v63)
                {
                    const MultiChannelNumber<float, 3> v64 = MultiChannelNumber<float, 3>::Splat(0x800000p-25f);
                    const MultiChannelNumber<float, 3> v65 = MultiChannelNumber<float, 3>::Splat(1.0f);
                    const MultiChannelNumber<float, 3> v66 = MultiChannelNumber<float, 3>::IfLess(v21, v64, v65, v61);
                    v67 = v66;
                }
                else
                {
                    v67 = v61;
                }

                const size_t v68 = (input.m_size < 1 ? 0 : input.m_size - 1);
                const bool v69 = (v34 == v68);
                const bool v70 = (v69 == true);
                MultiChannelNumber<float, 3> v77;
                if (v70)
                {
                    const float v71 = static_cast<float>(input.m_size);
                    const MultiChannelNumber<float, 3> v72 = MultiChannelNumber<float, 3>::Splat(v71);
                    const MultiChannelNumber<float, 3> v73 = MultiChannelNumber<float, 3>::Splat(0x800000p-25f);
                    const MultiChannelNumber<float, 3> v74 = (v72 - v73);
                    const MultiChannelNumber<float, 3> v75 = MultiChannelNumber<float, 3>::Splat(1.0f);
                    const MultiChannelNumber<float, 3> v76 = MultiChannelNumber<float, 3>::IfLess(v21, v74, v67, v75);
                    v77 = v76;
                }
                else
                {
                    v77 = v67;
                }

                state.m_weights[v34] = v77;
                const MultiChannelNumber<float, 3> v78 = (loop_v33 + v77);
                loop_v33 = v78;
            }

            const size_t v79 = input.m_size;
            const MultiChannelNumber<float, 3> v80 = input.m_slope;
            const MultiChannelNumber<float, 3> v81 = input.m_center;
            const MultiChannelNumber<float, 3> v82 = loop_v33;
            state.m_size = v79;
            state.m_slope = v80;
            state.m_center = v81;
            state.m_totalWeight = v82;
        }
        else
        {
        }

        const bool v83 = (input.m_size == 0);
        const bool v84 = !v83;
        const bool v85 = (v84 == true);
        if (v85)
        {
            const MultiChannelNumber<float, 3> v86 = MultiChannelNumber<float, 3>::Splat(input.m_minWaveMultLimit);
            const MultiChannelNumber<float, 3> v87 = MultiChannelNumber<float, 3>::Splat(input.m_minWaveMultLimit);
            const MultiChannelNumber<float, 3> v88 = MultiChannelNumber<float, 3>::IfLess(input.m_mult, v86, input.m_mult, v87);
            const bool v89 = (v88 != input.m_mult);
            const bool v90 = (v89 == true);
            if (v90)
            {
                const MultiChannelNumber<float, 3> v91 = MultiChannelNumber<float, 3>::Splat(0x83126fp-33f);
                const MultiChannelNumber<float, 3> v92 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v93 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v94 = MultiChannelNumber<float, 3>::IfLess(v92, input.m_attackFrac, v93, input.m_attackFrac);
                const MultiChannelNumber<float, 3> v95 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v96 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v97 = MultiChannelNumber<float, 3>::IfLess(v95, input.m_attackFrac, v96, input.m_attackFrac);
                const MultiChannelNumber<float, 3> v98 = MultiChannelNumber<float, 3>::Splat(0x83126fp-33f);
                const MultiChannelNumber<float, 3> v99 = MultiChannelNumber<float, 3>::IfLess(v91, v94, v97, v98);
                const MultiChannelNumber<double, 3> v100 = input.m_phaseShift.template Convert<double>();
                const MultiChannelNumber<float, 3> v101 = input.m_mult.Floor();
                const MultiChannelNumber<float, 3> v102 = (input.m_mult - v101);
                const MultiChannelNumber<float, 3> v103 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v104 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v105 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v106 = MultiChannelNumber<float, 3>::IfLess(v104, v102, v102, v105);
                const MultiChannelNumber<float, 3> v107 = (v103 / v106);
                const MultiChannelNumber<float, 3> v108 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v109 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v110 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v111 = (input.m_shape * v110);
                const MultiChannelNumber<float, 3> v112 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v113 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v114 = (input.m_shape * v113);
                const MultiChannelNumber<float, 3> v115 = MultiChannelNumber<float, 3>::IfLess(v109, v111, v112, v114);
                const MultiChannelNumber<float, 3> v116 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v117 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v118 = (input.m_shape * v117);
                const MultiChannelNumber<float, 3> v119 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v120 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v121 = (input.m_shape * v120);
                const MultiChannelNumber<float, 3> v122 = MultiChannelNumber<float, 3>::IfLess(v116, v118, v119, v121);
                const MultiChannelNumber<float, 3> v123 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v124 = MultiChannelNumber<float, 3>::IfLess(v108, v115, v122, v123);
                const MultiChannelNumber<float, 3> v125 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v126 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v127 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v128 = (input.m_shape * v127);
                const MultiChannelNumber<float, 3> v129 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v130 = (v128 - v129);
                const MultiChannelNumber<float, 3> v131 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v132 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v133 = (input.m_shape * v132);
                const MultiChannelNumber<float, 3> v134 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v135 = (v133 - v134);
                const MultiChannelNumber<float, 3> v136 = MultiChannelNumber<float, 3>::IfLess(v126, v130, v131, v135);
                const MultiChannelNumber<float, 3> v137 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v138 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v139 = (input.m_shape * v138);
                const MultiChannelNumber<float, 3> v140 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v141 = (v139 - v140);
                const MultiChannelNumber<float, 3> v142 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v143 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v144 = (input.m_shape * v143);
                const MultiChannelNumber<float, 3> v145 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v146 = (v144 - v145);
                const MultiChannelNumber<float, 3> v147 = MultiChannelNumber<float, 3>::IfLess(v137, v141, v142, v146);
                const MultiChannelNumber<float, 3> v148 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v149 = MultiChannelNumber<float, 3>::IfLess(v125, v136, v147, v148);
                const MultiChannelNumber<float, 3> v150 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v151 = (v150 - v124);
                const MultiChannelNumber<float, 3> v152 = (v124 - v149);
                const MultiChannelNumber<float, 3> v153 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v154 = (v153 / v99);
                const MultiChannelNumber<float, 3> v155 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v156 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v157 = (v156 - v99);
                const MultiChannelNumber<float, 3> v158 = (v155 / v157);
                const size_t v159 = input.m_size;
                const MultiChannelNumber<float, 3> v160 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<double, 3> v161 = MultiChannelNumber<double, 3>::Splat(0.0);
                struct loop_v162_Type
                {
                    MultiChannelNumber<float, 3> m_signal;
                    MultiChannelNumber<double, 3> m_maxSpeed;
                };

                [[maybe_unused]] loop_v162_Type loop_v162;
                loop_v162.m_signal = v160;
                loop_v162.m_maxSpeed = v161;
                for (std::size_t v163 = 0; v163 < v159; ++v163)
                {
                    const MultiChannelNumber<float, 3> v176 = state.m_weights[v163];
                    const MultiChannelNumber<float, 3> v177 = MultiChannelNumber<float, 3>::Splat(0.0f);
                    const bool v178 = (v176 != v177);
                    const bool v179 = !v178;
                    const bool v180 = (v179 == true);
                    MultiChannelNumber<float, 3> v240;
                    MultiChannelNumber<double, 3> v241;
                    if (v180)
                    {
                        v240 = loop_v162.m_signal;
                        v241 = loop_v162.m_maxSpeed;
                    }
                    else
                    {
                        const MultiChannelNumber<float, 3> v181 = MultiChannelNumber<float, 3>::Splat(input.m_waveMultLimits[v163]);
                        const MultiChannelNumber<float, 3> v182 = MultiChannelNumber<float, 3>::Splat(input.m_waveMultLimits[v163]);
                        const MultiChannelNumber<float, 3> v183 = MultiChannelNumber<float, 3>::IfLess(input.m_mult, v181, input.m_mult, v182);
                        const bool v184 = (v183 != input.m_mult);
                        const bool v185 = (v184 == true);
                        MultiChannelNumber<float, 3> v193;
                        MultiChannelNumber<float, 3> v194;
                        MultiChannelNumber<float, 3> v195;
                        MultiChannelNumber<float, 3> v196;
                        if (v185)
                        {
                            const MultiChannelNumber<float, 3> v186 = v183.Floor();
                            const MultiChannelNumber<float, 3> v187 = (v183 - v186);
                            const MultiChannelNumber<float, 3> v188 = MultiChannelNumber<float, 3>::Splat(1.0f);
                            const MultiChannelNumber<float, 3> v189 = MultiChannelNumber<float, 3>::Splat(0.0f);
                            const MultiChannelNumber<float, 3> v190 = MultiChannelNumber<float, 3>::Splat(1.0f);
                            const MultiChannelNumber<float, 3> v191 = MultiChannelNumber<float, 3>::IfLess(v189, v187, v187, v190);
                            const MultiChannelNumber<float, 3> v192 = (v188 / v191);
                            v193 = v183;
                            v194 = v186;
                            v195 = v187;
                            v196 = v192;
                        }
                        else
                        {
                            v193 = input.m_mult;
                            v194 = v101;
                            v195 = v102;
                            v196 = v107;
                        }

                        const double v197 = (input.m_masterPhase * input.m_cycleRatio[v163]);
                        const MultiChannelNumber<double, 3> v198 = MultiChannelNumber<double, 3>::Splat(v197);
                        const MultiChannelNumber<double, 3> v199 = (v198 + v100);
                        const MultiChannelNumber<double, 3> v200 = MultiChannelNumber<double, 3>::Splat(0x18000000000000p-53);
                        const MultiChannelNumber<double, 3> v201 = (v199 + v200);
                        const MultiChannelNumber<double, 3> v202 = (v198 + v100);
                        const MultiChannelNumber<double, 3> v203 = MultiChannelNumber<double, 3>::Splat(0x18000000000000p-53);
                        const MultiChannelNumber<double, 3> v204 = (v202 + v203);
                        const MultiChannelNumber<double, 3> v205 = v204.Floor();
                        const MultiChannelNumber<double, 3> v206 = (v201 - v205);
                        const MultiChannelNumber<float, 3> v207 = v206.template Convert<float>();
                        const MultiChannelNumber<float, 3> v208 = (v207 * v193);
                        const MultiChannelNumber<float, 3> v209 = (v208 - v194);
                        const MultiChannelNumber<float, 3> v210 = (v209 * v196);
                        const MultiChannelNumber<float, 3> v211 = v208.Floor();
                        const MultiChannelNumber<float, 3> v212 = (v208 - v211);
                        const MultiChannelNumber<float, 3> v213 = MultiChannelNumber<float, 3>::IfLess(v194, v208, v210, v212);
                        const MultiChannelNumber<float, 3> v214 = MultiChannelNumber<float, 3>::Splat(1.0f);
                        const MultiChannelNumber<float, 3> v215 = MultiChannelNumber<float, 3>::IfLess(v194, v208, v195, v214);
                        const MultiChannelNumber<float, 3> v216 = (v213 * v154);
                        const MultiChannelNumber<float, 3> v217 = MultiChannelNumber<float, 3>::Splat(1.0f);
                        const MultiChannelNumber<float, 3> v218 = (v213 - v99);
                        const MultiChannelNumber<float, 3> v219 = (v218 * v158);
                        const MultiChannelNumber<float, 3> v220 = (v217 - v219);
                        const MultiChannelNumber<float, 3> v221 = MultiChannelNumber<float, 3>::IfLess(v99, v213, v220, v216);
                        const auto v222 = PolyXFaderTables::Evaluate(v221);
                        const MultiChannelNumber<float, 3> v223 = MultiChannelNumber<float, 3>::IfLess(v99, v213, v220, v216);
                        const MultiChannelNumber<float, 3> v224 = (v223 * v151);
                        const MultiChannelNumber<float, 3> v225 = (v222.first * v152);
                        const MultiChannelNumber<float, 3> v226 = (v224 + v225);
                        const MultiChannelNumber<float, 3> v227 = (v222.second * v149);
                        const MultiChannelNumber<float, 3> v228 = (v226 + v227);
                        const MultiChannelNumber<float, 3> v229 = (v215 * v228);
                        const MultiChannelNumber<double, 3> v230 = MultiChannelNumber<double, 3>::Splat(input.m_cycleRatio[v163]);
                        const MultiChannelNumber<double, 3> v231 = v183.template Convert<double>();
                        const MultiChannelNumber<double, 3> v232 = (v230 * v231);
                        state.m_values[v163] = v229;
                        const MultiChannelNumber<float, 3> v233 = (v229 * v176);
                        const MultiChannelNumber<float, 3> v234 = (v233 * input.m_externalWeights[v163]);
                        const MultiChannelNumber<float, 3> v235 = (loop_v162.m_signal + v234);
                        const MultiChannelNumber<double, 3> v236 = MultiChannelNumber<double, 3>::Splat(0.0);
                        const MultiChannelNumber<double, 3> v237 = v176.template Convert<double>();
                        const MultiChannelNumber<double, 3> v238 = MultiChannelNumber<double, 3>::IfLess(loop_v162.m_maxSpeed, v232, v232, loop_v162.m_maxSpeed);
                        const MultiChannelNumber<double, 3> v239 = MultiChannelNumber<double, 3>::IfLess(v236, v237, v238, loop_v162.m_maxSpeed);
                        v240 = v235;
                        v241 = v239;
                    }

                    loop_v162.m_signal = v240;
                    loop_v162.m_maxSpeed = v241;
                }

                const MultiChannelNumber<float, 3> v245 = state.m_totalWeight;
                const MultiChannelNumber<float, 3> v254 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v255 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v256 = MultiChannelNumber<float, 3>::IfLess(v254, v245, v245, v255);
                const MultiChannelNumber<float, 3> v257 = (loop_v162.m_signal / v256);
                const MultiChannelNumber<double, 3> v264 = state.m_mixOutput;
                const float v265 = state.m_holdAlpha;
                const MultiChannelNumber<float, 3> v266 = state.m_heldValue;
                const MultiChannelNumber<float, 3> v267 = state.m_heldOutput;
                const MultiChannelNumber<double, 3> v270 = MultiChannelNumber<double, 3>::Splat(0x10000000000000p-49);
                const MultiChannelNumber<double, 3> v271 = (v270 * loop_v162.m_maxSpeed);
                const MultiChannelNumber<double, 3> v272 = MultiChannelNumber<double, 3>::Splat(input.m_phaseTravel);
                const MultiChannelNumber<double, 3> v273 = (v271 * v272);
                const MultiChannelNumber<double, 3> v274 = MultiChannelNumber<double, 3>::Splat(input.m_maxSlewCycles);
                const MultiChannelNumber<double, 3> v275 = MultiChannelNumber<double, 3>::Splat(0x10000000000000p-49);
                const MultiChannelNumber<double, 3> v276 = (v275 * loop_v162.m_maxSpeed);
                const MultiChannelNumber<double, 3> v277 = MultiChannelNumber<double, 3>::Splat(input.m_phaseTravel);
                const MultiChannelNumber<double, 3> v278 = (v276 * v277);
                const MultiChannelNumber<double, 3> v279 = MultiChannelNumber<double, 3>::Splat(input.m_maxSlewCycles);
                const MultiChannelNumber<double, 3> v280 = MultiChannelNumber<double, 3>::IfLess(v273, v274, v278, v279);
                const MultiChannelNumber<double, 3> v281 = MultiChannelNumber<double, 3>::Splat(input.m_slewStepScale);
                const MultiChannelNumber<double, 3> v282 = (v280 * v281);
                const MultiChannelNumber<float, 3> v283 = v282.template Convert<float>();
                const MultiChannelNumber<float, 3> v284 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v285 = MultiChannelNumber<float, 3>::Splat(0xb40000p-14f);
                const MultiChannelNumber<float, 3> v286 = (v284 / v285);
                const MultiChannelNumber<float, 3> v287 = (-v286);
                const MultiChannelNumber<float, 3> v288 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v289 = (v283 * v288);
                const MultiChannelNumber<float, 3> v290 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v291 = MultiChannelNumber<float, 3>::Splat(0xf00000p-17f);
                const MultiChannelNumber<float, 3> v292 = (v290 / v291);
                const MultiChannelNumber<float, 3> v293 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v287, v289, v292);
                const MultiChannelNumber<float, 3> v294 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v295 = (v283 * v294);
                const MultiChannelNumber<float, 3> v296 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v297 = MultiChannelNumber<float, 3>::Splat(0xc00000p-19f);
                const MultiChannelNumber<float, 3> v298 = (v296 / v297);
                const MultiChannelNumber<float, 3> v299 = (-v298);
                const MultiChannelNumber<float, 3> v300 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v293, v295, v299);
                const MultiChannelNumber<float, 3> v301 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v302 = (v283 * v301);
                const MultiChannelNumber<float, 3> v303 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v304 = MultiChannelNumber<float, 3>::Splat(0xc00000p-21f);
                const MultiChannelNumber<float, 3> v305 = (v303 / v304);
                const MultiChannelNumber<float, 3> v306 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v300, v302, v305);
                const MultiChannelNumber<float, 3> v307 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v308 = (v283 * v307);
                const MultiChannelNumber<float, 3> v309 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v310 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v311 = (v309 / v310);
                const MultiChannelNumber<float, 3> v312 = (-v311);
                const MultiChannelNumber<float, 3> v313 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v306, v308, v312);
                const MultiChannelNumber<float, 3> v314 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v315 = (v283 * v314);
                const MultiChannelNumber<float, 3> v316 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v317 = (v283 * v316);
                const MultiChannelNumber<float, 3> v318 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v319 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v313, v317, v318);
                const MultiChannelNumber<float, 3> v320 = (v315 * v319);
                const size_t v321 = input.m_slewDoublings;
                [[maybe_unused]] MultiChannelNumber<float, 3> loop_v322;
                loop_v322 = v320;
                for (std::size_t v323 = 0; v323 < v321; ++v323)
                {
                    const MultiChannelNumber<float, 3> v324 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                    const MultiChannelNumber<float, 3> v325 = (v324 - loop_v322);
                    const MultiChannelNumber<float, 3> v326 = (loop_v322 * v325);
                    loop_v322 = v326;
                }

                const MultiChannelNumber<double, 3> v327 = loop_v322.template Convert<double>();
                const MultiChannelNumber<double, 3> v328 = v257.template Convert<double>();
                const MultiChannelNumber<double, 3> v329 = MultiChannelNumber<double, 3>::Splat(1.0);
                const MultiChannelNumber<double, 3> v330 = (v329 - v327);
                const MultiChannelNumber<double, 3> v331 = (v330 * v264);
                const MultiChannelNumber<double, 3> v332 = MultiChannelNumber<double, 3>::FusedMultiplyAdd(v327, v328, v331);
                const MultiChannelNumber<float, 3> v333 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v334 = v332.template Convert<float>();
                const MultiChannelNumber<float, 3> v335 = MultiChannelNumber<float, 3>::IfLess(v333, input.m_trig, v334, v266);
                const MultiChannelNumber<float, 3> v336 = MultiChannelNumber<float, 3>::Splat(v265);
                const MultiChannelNumber<float, 3> v337 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v338 = MultiChannelNumber<float, 3>::Splat(v265);
                const MultiChannelNumber<float, 3> v339 = (v337 - v338);
                const MultiChannelNumber<float, 3> v340 = (v339 * v267);
                const MultiChannelNumber<float, 3> v341 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v336, v335, v340);
                const MultiChannelNumber<float, 3> v342 = (v341 * input.m_shFade);
                const MultiChannelNumber<float, 3> v343 = v332.template Convert<float>();
                const MultiChannelNumber<float, 3> v344 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v345 = (v344 - input.m_shFade);
                const MultiChannelNumber<float, 3> v346 = (v343 * v345);
                const MultiChannelNumber<float, 3> v347 = (v342 + v346);
                const MultiChannelNumber<float, 3> v348 = (v347 * input.m_amplitude);
                const MultiChannelNumber<float, 3> v349 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v350 = (v349 - input.m_amplitude);
                const MultiChannelNumber<float, 3> v351 = MultiChannelNumber<float, 3>::Splat(0x800000p-24f);
                const MultiChannelNumber<float, 3> v352 = (v350 * v351);
                const MultiChannelNumber<float, 3> v353 = (v348 + v352);
                state.m_mixOutput = v332;
                state.m_heldValue = v335;
                state.m_heldOutput = v341;
                state.m_rawOutput = v347;
                state.m_output = v353;
            }
            else
            {
                const MultiChannelNumber<float, 3> v354 = MultiChannelNumber<float, 3>::Splat(0x83126fp-33f);
                const MultiChannelNumber<float, 3> v355 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v356 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v357 = MultiChannelNumber<float, 3>::IfLess(v355, input.m_attackFrac, v356, input.m_attackFrac);
                const MultiChannelNumber<float, 3> v358 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v359 = MultiChannelNumber<float, 3>::Splat(0xffbe77p-24f);
                const MultiChannelNumber<float, 3> v360 = MultiChannelNumber<float, 3>::IfLess(v358, input.m_attackFrac, v359, input.m_attackFrac);
                const MultiChannelNumber<float, 3> v361 = MultiChannelNumber<float, 3>::Splat(0x83126fp-33f);
                const MultiChannelNumber<float, 3> v362 = MultiChannelNumber<float, 3>::IfLess(v354, v357, v360, v361);
                const MultiChannelNumber<double, 3> v363 = input.m_phaseShift.template Convert<double>();
                const MultiChannelNumber<float, 3> v364 = input.m_mult.Floor();
                const MultiChannelNumber<float, 3> v365 = (input.m_mult - v364);
                const MultiChannelNumber<float, 3> v366 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v367 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v368 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v369 = MultiChannelNumber<float, 3>::IfLess(v367, v365, v365, v368);
                const MultiChannelNumber<float, 3> v370 = (v366 / v369);
                const MultiChannelNumber<float, 3> v371 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v372 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v373 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v374 = (input.m_shape * v373);
                const MultiChannelNumber<float, 3> v375 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v376 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v377 = (input.m_shape * v376);
                const MultiChannelNumber<float, 3> v378 = MultiChannelNumber<float, 3>::IfLess(v372, v374, v375, v377);
                const MultiChannelNumber<float, 3> v379 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v380 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v381 = (input.m_shape * v380);
                const MultiChannelNumber<float, 3> v382 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v383 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v384 = (input.m_shape * v383);
                const MultiChannelNumber<float, 3> v385 = MultiChannelNumber<float, 3>::IfLess(v379, v381, v382, v384);
                const MultiChannelNumber<float, 3> v386 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v387 = MultiChannelNumber<float, 3>::IfLess(v371, v378, v385, v386);
                const MultiChannelNumber<float, 3> v388 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v389 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v390 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v391 = (input.m_shape * v390);
                const MultiChannelNumber<float, 3> v392 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v393 = (v391 - v392);
                const MultiChannelNumber<float, 3> v394 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v395 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v396 = (input.m_shape * v395);
                const MultiChannelNumber<float, 3> v397 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v398 = (v396 - v397);
                const MultiChannelNumber<float, 3> v399 = MultiChannelNumber<float, 3>::IfLess(v389, v393, v394, v398);
                const MultiChannelNumber<float, 3> v400 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v401 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v402 = (input.m_shape * v401);
                const MultiChannelNumber<float, 3> v403 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v404 = (v402 - v403);
                const MultiChannelNumber<float, 3> v405 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v406 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v407 = (input.m_shape * v406);
                const MultiChannelNumber<float, 3> v408 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v409 = (v407 - v408);
                const MultiChannelNumber<float, 3> v410 = MultiChannelNumber<float, 3>::IfLess(v400, v404, v405, v409);
                const MultiChannelNumber<float, 3> v411 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v412 = MultiChannelNumber<float, 3>::IfLess(v388, v399, v410, v411);
                const MultiChannelNumber<float, 3> v413 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v414 = (v413 - v387);
                const MultiChannelNumber<float, 3> v415 = (v387 - v412);
                const MultiChannelNumber<float, 3> v416 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v417 = (v416 / v362);
                const MultiChannelNumber<float, 3> v418 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v419 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v420 = (v419 - v362);
                const MultiChannelNumber<float, 3> v421 = (v418 / v420);
                const size_t v422 = input.m_size;
                const MultiChannelNumber<float, 3> v423 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<double, 3> v424 = MultiChannelNumber<double, 3>::Splat(0.0);
                struct loop_v425_Type
                {
                    MultiChannelNumber<float, 3> m_signal;
                    MultiChannelNumber<double, 3> m_maxSpeed;
                };

                [[maybe_unused]] loop_v425_Type loop_v425;
                loop_v425.m_signal = v423;
                loop_v425.m_maxSpeed = v424;
                for (std::size_t v426 = 0; v426 < v422; ++v426)
                {
                    const MultiChannelNumber<float, 3> v439 = state.m_weights[v426];
                    const MultiChannelNumber<float, 3> v440 = MultiChannelNumber<float, 3>::Splat(0.0f);
                    const bool v441 = (v439 != v440);
                    const bool v442 = !v441;
                    const bool v443 = (v442 == true);
                    MultiChannelNumber<float, 3> v485;
                    MultiChannelNumber<double, 3> v486;
                    if (v443)
                    {
                        v485 = loop_v425.m_signal;
                        v486 = loop_v425.m_maxSpeed;
                    }
                    else
                    {
                        const double v444 = (input.m_masterPhase * input.m_cycleRatio[v426]);
                        const MultiChannelNumber<double, 3> v445 = MultiChannelNumber<double, 3>::Splat(v444);
                        const MultiChannelNumber<double, 3> v446 = (v445 + v363);
                        const MultiChannelNumber<double, 3> v447 = MultiChannelNumber<double, 3>::Splat(0x18000000000000p-53);
                        const MultiChannelNumber<double, 3> v448 = (v446 + v447);
                        const MultiChannelNumber<double, 3> v449 = (v445 + v363);
                        const MultiChannelNumber<double, 3> v450 = MultiChannelNumber<double, 3>::Splat(0x18000000000000p-53);
                        const MultiChannelNumber<double, 3> v451 = (v449 + v450);
                        const MultiChannelNumber<double, 3> v452 = v451.Floor();
                        const MultiChannelNumber<double, 3> v453 = (v448 - v452);
                        const MultiChannelNumber<float, 3> v454 = v453.template Convert<float>();
                        const MultiChannelNumber<float, 3> v455 = (v454 * input.m_mult);
                        const MultiChannelNumber<float, 3> v456 = (v455 - v364);
                        const MultiChannelNumber<float, 3> v457 = (v456 * v370);
                        const MultiChannelNumber<float, 3> v458 = v455.Floor();
                        const MultiChannelNumber<float, 3> v459 = (v455 - v458);
                        const MultiChannelNumber<float, 3> v460 = MultiChannelNumber<float, 3>::IfLess(v364, v455, v457, v459);
                        const MultiChannelNumber<float, 3> v461 = MultiChannelNumber<float, 3>::Splat(1.0f);
                        const MultiChannelNumber<float, 3> v462 = MultiChannelNumber<float, 3>::IfLess(v364, v455, v365, v461);
                        const MultiChannelNumber<float, 3> v463 = (v460 * v417);
                        const MultiChannelNumber<float, 3> v464 = MultiChannelNumber<float, 3>::Splat(1.0f);
                        const MultiChannelNumber<float, 3> v465 = (v460 - v362);
                        const MultiChannelNumber<float, 3> v466 = (v465 * v421);
                        const MultiChannelNumber<float, 3> v467 = (v464 - v466);
                        const MultiChannelNumber<float, 3> v468 = MultiChannelNumber<float, 3>::IfLess(v362, v460, v467, v463);
                        const auto v469 = PolyXFaderTables::Evaluate(v468);
                        const MultiChannelNumber<float, 3> v470 = MultiChannelNumber<float, 3>::IfLess(v362, v460, v467, v463);
                        const MultiChannelNumber<float, 3> v471 = (v470 * v414);
                        const MultiChannelNumber<float, 3> v472 = (v469.first * v415);
                        const MultiChannelNumber<float, 3> v473 = (v471 + v472);
                        const MultiChannelNumber<float, 3> v474 = (v469.second * v412);
                        const MultiChannelNumber<float, 3> v475 = (v473 + v474);
                        const MultiChannelNumber<float, 3> v476 = (v462 * v475);
                        const MultiChannelNumber<double, 3> v477 = MultiChannelNumber<double, 3>::Splat(input.m_cycleRatio[v426]);
                        state.m_values[v426] = v476;
                        const MultiChannelNumber<float, 3> v478 = (v476 * v439);
                        const MultiChannelNumber<float, 3> v479 = (v478 * input.m_externalWeights[v426]);
                        const MultiChannelNumber<float, 3> v480 = (loop_v425.m_signal + v479);
                        const MultiChannelNumber<double, 3> v481 = MultiChannelNumber<double, 3>::Splat(0.0);
                        const MultiChannelNumber<double, 3> v482 = v439.template Convert<double>();
                        const MultiChannelNumber<double, 3> v483 = MultiChannelNumber<double, 3>::IfLess(loop_v425.m_maxSpeed, v477, v477, loop_v425.m_maxSpeed);
                        const MultiChannelNumber<double, 3> v484 = MultiChannelNumber<double, 3>::IfLess(v481, v482, v483, loop_v425.m_maxSpeed);
                        v485 = v480;
                        v486 = v484;
                    }

                    loop_v425.m_signal = v485;
                    loop_v425.m_maxSpeed = v486;
                }

                const MultiChannelNumber<float, 3> v490 = state.m_totalWeight;
                const MultiChannelNumber<float, 3> v499 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v500 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v501 = MultiChannelNumber<float, 3>::IfLess(v499, v490, v490, v500);
                const MultiChannelNumber<double, 3> v502 = input.m_mult.template Convert<double>();
                const MultiChannelNumber<double, 3> v503 = (loop_v425.m_maxSpeed * v502);
                const MultiChannelNumber<float, 3> v504 = (loop_v425.m_signal / v501);
                const MultiChannelNumber<double, 3> v511 = state.m_mixOutput;
                const float v512 = state.m_holdAlpha;
                const MultiChannelNumber<float, 3> v513 = state.m_heldValue;
                const MultiChannelNumber<float, 3> v514 = state.m_heldOutput;
                const MultiChannelNumber<double, 3> v517 = MultiChannelNumber<double, 3>::Splat(0x10000000000000p-49);
                const MultiChannelNumber<double, 3> v518 = (v517 * v503);
                const MultiChannelNumber<double, 3> v519 = MultiChannelNumber<double, 3>::Splat(input.m_phaseTravel);
                const MultiChannelNumber<double, 3> v520 = (v518 * v519);
                const MultiChannelNumber<double, 3> v521 = MultiChannelNumber<double, 3>::Splat(input.m_maxSlewCycles);
                const MultiChannelNumber<double, 3> v522 = MultiChannelNumber<double, 3>::Splat(0x10000000000000p-49);
                const MultiChannelNumber<double, 3> v523 = (v522 * v503);
                const MultiChannelNumber<double, 3> v524 = MultiChannelNumber<double, 3>::Splat(input.m_phaseTravel);
                const MultiChannelNumber<double, 3> v525 = (v523 * v524);
                const MultiChannelNumber<double, 3> v526 = MultiChannelNumber<double, 3>::Splat(input.m_maxSlewCycles);
                const MultiChannelNumber<double, 3> v527 = MultiChannelNumber<double, 3>::IfLess(v520, v521, v525, v526);
                const MultiChannelNumber<double, 3> v528 = MultiChannelNumber<double, 3>::Splat(input.m_slewStepScale);
                const MultiChannelNumber<double, 3> v529 = (v527 * v528);
                const MultiChannelNumber<float, 3> v530 = v529.template Convert<float>();
                const MultiChannelNumber<float, 3> v531 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v532 = MultiChannelNumber<float, 3>::Splat(0xb40000p-14f);
                const MultiChannelNumber<float, 3> v533 = (v531 / v532);
                const MultiChannelNumber<float, 3> v534 = (-v533);
                const MultiChannelNumber<float, 3> v535 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v536 = (v530 * v535);
                const MultiChannelNumber<float, 3> v537 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v538 = MultiChannelNumber<float, 3>::Splat(0xf00000p-17f);
                const MultiChannelNumber<float, 3> v539 = (v537 / v538);
                const MultiChannelNumber<float, 3> v540 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v534, v536, v539);
                const MultiChannelNumber<float, 3> v541 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v542 = (v530 * v541);
                const MultiChannelNumber<float, 3> v543 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v544 = MultiChannelNumber<float, 3>::Splat(0xc00000p-19f);
                const MultiChannelNumber<float, 3> v545 = (v543 / v544);
                const MultiChannelNumber<float, 3> v546 = (-v545);
                const MultiChannelNumber<float, 3> v547 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v540, v542, v546);
                const MultiChannelNumber<float, 3> v548 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v549 = (v530 * v548);
                const MultiChannelNumber<float, 3> v550 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v551 = MultiChannelNumber<float, 3>::Splat(0xc00000p-21f);
                const MultiChannelNumber<float, 3> v552 = (v550 / v551);
                const MultiChannelNumber<float, 3> v553 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v547, v549, v552);
                const MultiChannelNumber<float, 3> v554 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v555 = (v530 * v554);
                const MultiChannelNumber<float, 3> v556 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v557 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                const MultiChannelNumber<float, 3> v558 = (v556 / v557);
                const MultiChannelNumber<float, 3> v559 = (-v558);
                const MultiChannelNumber<float, 3> v560 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v553, v555, v559);
                const MultiChannelNumber<float, 3> v561 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v562 = (v530 * v561);
                const MultiChannelNumber<float, 3> v563 = MultiChannelNumber<float, 3>::Splat(0xc90fdbp-21f);
                const MultiChannelNumber<float, 3> v564 = (v530 * v563);
                const MultiChannelNumber<float, 3> v565 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v566 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v560, v564, v565);
                const MultiChannelNumber<float, 3> v567 = (v562 * v566);
                const size_t v568 = input.m_slewDoublings;
                [[maybe_unused]] MultiChannelNumber<float, 3> loop_v569;
                loop_v569 = v567;
                for (std::size_t v570 = 0; v570 < v568; ++v570)
                {
                    const MultiChannelNumber<float, 3> v571 = MultiChannelNumber<float, 3>::Splat(0x800000p-22f);
                    const MultiChannelNumber<float, 3> v572 = (v571 - loop_v569);
                    const MultiChannelNumber<float, 3> v573 = (loop_v569 * v572);
                    loop_v569 = v573;
                }

                const MultiChannelNumber<double, 3> v574 = loop_v569.template Convert<double>();
                const MultiChannelNumber<double, 3> v575 = v504.template Convert<double>();
                const MultiChannelNumber<double, 3> v576 = MultiChannelNumber<double, 3>::Splat(1.0);
                const MultiChannelNumber<double, 3> v577 = (v576 - v574);
                const MultiChannelNumber<double, 3> v578 = (v577 * v511);
                const MultiChannelNumber<double, 3> v579 = MultiChannelNumber<double, 3>::FusedMultiplyAdd(v574, v575, v578);
                const MultiChannelNumber<float, 3> v580 = MultiChannelNumber<float, 3>::Splat(0.0f);
                const MultiChannelNumber<float, 3> v581 = v579.template Convert<float>();
                const MultiChannelNumber<float, 3> v582 = MultiChannelNumber<float, 3>::IfLess(v580, input.m_trig, v581, v513);
                const MultiChannelNumber<float, 3> v583 = MultiChannelNumber<float, 3>::Splat(v512);
                const MultiChannelNumber<float, 3> v584 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v585 = MultiChannelNumber<float, 3>::Splat(v512);
                const MultiChannelNumber<float, 3> v586 = (v584 - v585);
                const MultiChannelNumber<float, 3> v587 = (v586 * v514);
                const MultiChannelNumber<float, 3> v588 = MultiChannelNumber<float, 3>::FusedMultiplyAdd(v583, v582, v587);
                const MultiChannelNumber<float, 3> v589 = (v588 * input.m_shFade);
                const MultiChannelNumber<float, 3> v590 = v579.template Convert<float>();
                const MultiChannelNumber<float, 3> v591 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v592 = (v591 - input.m_shFade);
                const MultiChannelNumber<float, 3> v593 = (v590 * v592);
                const MultiChannelNumber<float, 3> v594 = (v589 + v593);
                const MultiChannelNumber<float, 3> v595 = (v594 * input.m_amplitude);
                const MultiChannelNumber<float, 3> v596 = MultiChannelNumber<float, 3>::Splat(1.0f);
                const MultiChannelNumber<float, 3> v597 = (v596 - input.m_amplitude);
                const MultiChannelNumber<float, 3> v598 = MultiChannelNumber<float, 3>::Splat(0x800000p-24f);
                const MultiChannelNumber<float, 3> v599 = (v597 * v598);
                const MultiChannelNumber<float, 3> v600 = (v595 + v599);
                state.m_mixOutput = v579;
                state.m_heldValue = v582;
                state.m_heldOutput = v588;
                state.m_rawOutput = v594;
                state.m_output = v600;
            }

        }
        else
        {
        }

    }

};
