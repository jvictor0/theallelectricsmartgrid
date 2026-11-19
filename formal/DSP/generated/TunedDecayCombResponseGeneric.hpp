#pragma once

// Generated from DSP.DelayKernels.response. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct TunedDecayCombResponseGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            Vector m_delay;
            Vector m_a;
            Vector m_b;
            Vector m_r;
            Vector m_delayGain;
            Vector m_g;
            Vector m_k;
            Vector m_h;
            Vector m_gain;
            Vector m_sign;
            Vector m_bypass;
        } m_first;

        struct
        {
            Vector m_delay;
            Vector m_a;
            Vector m_b;
            Vector m_r;
            Vector m_delayGain;
            Vector m_g;
            Vector m_k;
            Vector m_h;
            Vector m_gain;
            Vector m_sign;
            Vector m_bypass;
        } m_second;

        Vector m_mix;
    };

    struct Input
    {
        Vector m_cyclesPerSample;
        Vector m_sinHalf;
        Vector m_cosHalf;
        struct
        {
            Vector m_real;
            Vector m_imag;
        } m_zInverse;

    };

    struct Output
    {
        struct
        {
            Vector m_real;
            Vector m_imag;
        } m_numerator;

        struct
        {
            Vector m_real;
            Vector m_imag;
        } m_denominator;

    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (m_state.m_first.m_r * input.m_zInverse.m_real);
        const Vector v1 = (m_state.m_first.m_r * input.m_zInverse.m_imag);
        const Vector v2 = (m_state.m_first.m_g * m_state.m_first.m_g);
        const Vector v3 = (m_state.m_first.m_k * m_state.m_first.m_g);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v0);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v1);
        const Vector v6 = (m_state.m_first.m_gain * v2);
        const Vector v7 = (v4 * v4);
        const Vector v8 = (v5 * v5);
        const Vector v9 = (v7 - v8);
        const Vector v10 = (v6 * v9);
        const Vector v11 = (m_state.m_first.m_gain * v2);
        const Vector v12 = (v4 * v5);
        const Vector v13 = (v5 * v4);
        const Vector v14 = (v12 + v13);
        const Vector v15 = (v11 * v14);
        const Vector v16 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v3);
        const Vector v17 = (v16 + v2);
        const Vector v18 = (v2 - Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v18);
        const Vector v20 = (v19 * v0);
        const Vector v21 = (v17 + v20);
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v3);
        const Vector v23 = (v22 + v2);
        const Vector v24 = (v0 * v0);
        const Vector v25 = (v1 * v1);
        const Vector v26 = (v24 - v25);
        const Vector v27 = (v23 * v26);
        const Vector v28 = (v21 + v27);
        const Vector v29 = (v2 - Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v30 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v29);
        const Vector v31 = (v30 * v1);
        const Vector v32 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v31);
        const Vector v33 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v3);
        const Vector v34 = (v33 + v2);
        const Vector v35 = (v0 * v1);
        const Vector v36 = (v1 * v0);
        const Vector v37 = (v35 + v36);
        const Vector v38 = (v34 * v37);
        const Vector v39 = (v32 + v38);
        const Vector v40 = (v28 * v28);
        const Vector v41 = (v39 * v39);
        const Vector v42 = (v40 + v41);
        const Vector v43 = (v10 * v28);
        const Vector v44 = (v15 * v39);
        const Vector v45 = (v43 + v44);
        const Vector v46 = (v45 / v42);
        const Vector v47 = (v15 * v28);
        const Vector v48 = (v10 * v39);
        const Vector v49 = (v47 - v48);
        const Vector v50 = (v49 / v42);
        const Vector v51 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v52 = Vector::IfLess(m_state.m_first.m_bypass, v51, v46, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v53 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v54 = Vector::IfLess(m_state.m_first.m_bypass, v53, v50, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v55 = (m_state.m_second.m_r * input.m_zInverse.m_real);
        const Vector v56 = (m_state.m_second.m_r * input.m_zInverse.m_imag);
        const Vector v57 = (m_state.m_second.m_g * m_state.m_second.m_g);
        const Vector v58 = (m_state.m_second.m_k * m_state.m_second.m_g);
        const Vector v59 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v55);
        const Vector v60 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v56);
        const Vector v61 = (m_state.m_second.m_gain * v57);
        const Vector v62 = (v59 * v59);
        const Vector v63 = (v60 * v60);
        const Vector v64 = (v62 - v63);
        const Vector v65 = (v61 * v64);
        const Vector v66 = (m_state.m_second.m_gain * v57);
        const Vector v67 = (v59 * v60);
        const Vector v68 = (v60 * v59);
        const Vector v69 = (v67 + v68);
        const Vector v70 = (v66 * v69);
        const Vector v71 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v58);
        const Vector v72 = (v71 + v57);
        const Vector v73 = (v57 - Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v74 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v73);
        const Vector v75 = (v74 * v55);
        const Vector v76 = (v72 + v75);
        const Vector v77 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v58);
        const Vector v78 = (v77 + v57);
        const Vector v79 = (v55 * v55);
        const Vector v80 = (v56 * v56);
        const Vector v81 = (v79 - v80);
        const Vector v82 = (v78 * v81);
        const Vector v83 = (v76 + v82);
        const Vector v84 = (v57 - Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v85 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v84);
        const Vector v86 = (v85 * v56);
        const Vector v87 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v86);
        const Vector v88 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v58);
        const Vector v89 = (v88 + v57);
        const Vector v90 = (v55 * v56);
        const Vector v91 = (v56 * v55);
        const Vector v92 = (v90 + v91);
        const Vector v93 = (v89 * v92);
        const Vector v94 = (v87 + v93);
        const Vector v95 = (v83 * v83);
        const Vector v96 = (v94 * v94);
        const Vector v97 = (v95 + v96);
        const Vector v98 = (v65 * v83);
        const Vector v99 = (v70 * v94);
        const Vector v100 = (v98 + v99);
        const Vector v101 = (v100 / v97);
        const Vector v102 = (v70 * v83);
        const Vector v103 = (v65 * v94);
        const Vector v104 = (v102 - v103);
        const Vector v105 = (v104 / v97);
        const Vector v106 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v107 = Vector::IfLess(m_state.m_second.m_bypass, v106, v101, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v108 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v109 = Vector::IfLess(m_state.m_second.m_bypass, v108, v105, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v110 = (m_state.m_first.m_r * input.m_zInverse.m_real);
        const Vector v111 = (m_state.m_first.m_r * input.m_zInverse.m_imag);
        const Vector v112 = (m_state.m_first.m_a * v110);
        const Vector v113 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v112);
        const Vector v114 = (m_state.m_first.m_a * v110);
        const Vector v115 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v114);
        const Vector v116 = (v113 * v115);
        const Vector v117 = (m_state.m_first.m_a * v111);
        const Vector v118 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v117);
        const Vector v119 = (m_state.m_first.m_a * v111);
        const Vector v120 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v119);
        const Vector v121 = (v118 * v120);
        const Vector v122 = (v116 + v121);
        const Vector v123 = (m_state.m_first.m_a + v110);
        const Vector v124 = (m_state.m_first.m_a * v110);
        const Vector v125 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v124);
        const Vector v126 = (v123 * v125);
        const Vector v127 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v111);
        const Vector v128 = (m_state.m_first.m_a * v111);
        const Vector v129 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v128);
        const Vector v130 = (v127 * v129);
        const Vector v131 = (v126 + v130);
        const Vector v132 = (v131 / v122);
        const Vector v133 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v111);
        const Vector v134 = (m_state.m_first.m_a * v110);
        const Vector v135 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v134);
        const Vector v136 = (v133 * v135);
        const Vector v137 = (m_state.m_first.m_a + v110);
        const Vector v138 = (m_state.m_first.m_a * v111);
        const Vector v139 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v138);
        const Vector v140 = (v137 * v139);
        const Vector v141 = (v136 - v140);
        const Vector v142 = (v141 / v122);
        const Vector v143 = (m_state.m_first.m_delay * input.m_cyclesPerSample);
        const Vector v144 = (-v143);
        const Vector v145 = Math4096::Cos2pi(v144);
        const Vector v146 = Math4096::Sin2pi(v144);
        const Vector v147 = (m_state.m_first.m_sign * v52);
        const Vector v148 = (v147 * v132);
        const Vector v149 = (m_state.m_first.m_sign * v54);
        const Vector v150 = (v149 * v142);
        const Vector v151 = (v148 - v150);
        const Vector v152 = (m_state.m_first.m_delayGain * v151);
        const Vector v153 = (v152 * v145);
        const Vector v154 = (m_state.m_first.m_sign * v52);
        const Vector v155 = (v154 * v142);
        const Vector v156 = (m_state.m_first.m_sign * v54);
        const Vector v157 = (v156 * v132);
        const Vector v158 = (v155 + v157);
        const Vector v159 = (m_state.m_first.m_delayGain * v158);
        const Vector v160 = (v159 * v146);
        const Vector v161 = (v153 - v160);
        const Vector v162 = (m_state.m_first.m_sign * v52);
        const Vector v163 = (v162 * v132);
        const Vector v164 = (m_state.m_first.m_sign * v54);
        const Vector v165 = (v164 * v142);
        const Vector v166 = (v163 - v165);
        const Vector v167 = (m_state.m_first.m_delayGain * v166);
        const Vector v168 = (v167 * v146);
        const Vector v169 = (m_state.m_first.m_sign * v52);
        const Vector v170 = (v169 * v142);
        const Vector v171 = (m_state.m_first.m_sign * v54);
        const Vector v172 = (v171 * v132);
        const Vector v173 = (v170 + v172);
        const Vector v174 = (m_state.m_first.m_delayGain * v173);
        const Vector v175 = (v174 * v145);
        const Vector v176 = (v168 + v175);
        const Vector v177 = (m_state.m_second.m_r * input.m_zInverse.m_real);
        const Vector v178 = (m_state.m_second.m_r * input.m_zInverse.m_imag);
        const Vector v179 = (m_state.m_second.m_a * v177);
        const Vector v180 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v179);
        const Vector v181 = (m_state.m_second.m_a * v177);
        const Vector v182 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v181);
        const Vector v183 = (v180 * v182);
        const Vector v184 = (m_state.m_second.m_a * v178);
        const Vector v185 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v184);
        const Vector v186 = (m_state.m_second.m_a * v178);
        const Vector v187 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v186);
        const Vector v188 = (v185 * v187);
        const Vector v189 = (v183 + v188);
        const Vector v190 = (m_state.m_second.m_a + v177);
        const Vector v191 = (m_state.m_second.m_a * v177);
        const Vector v192 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v191);
        const Vector v193 = (v190 * v192);
        const Vector v194 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v178);
        const Vector v195 = (m_state.m_second.m_a * v178);
        const Vector v196 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v195);
        const Vector v197 = (v194 * v196);
        const Vector v198 = (v193 + v197);
        const Vector v199 = (v198 / v189);
        const Vector v200 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v178);
        const Vector v201 = (m_state.m_second.m_a * v177);
        const Vector v202 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v201);
        const Vector v203 = (v200 * v202);
        const Vector v204 = (m_state.m_second.m_a + v177);
        const Vector v205 = (m_state.m_second.m_a * v178);
        const Vector v206 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v205);
        const Vector v207 = (v204 * v206);
        const Vector v208 = (v203 - v207);
        const Vector v209 = (v208 / v189);
        const Vector v210 = (m_state.m_second.m_delay * input.m_cyclesPerSample);
        const Vector v211 = (-v210);
        const Vector v212 = Math4096::Cos2pi(v211);
        const Vector v213 = Math4096::Sin2pi(v211);
        const Vector v214 = (m_state.m_second.m_sign * v107);
        const Vector v215 = (v214 * v199);
        const Vector v216 = (m_state.m_second.m_sign * v109);
        const Vector v217 = (v216 * v209);
        const Vector v218 = (v215 - v217);
        const Vector v219 = (m_state.m_second.m_delayGain * v218);
        const Vector v220 = (v219 * v212);
        const Vector v221 = (m_state.m_second.m_sign * v107);
        const Vector v222 = (v221 * v209);
        const Vector v223 = (m_state.m_second.m_sign * v109);
        const Vector v224 = (v223 * v199);
        const Vector v225 = (v222 + v224);
        const Vector v226 = (m_state.m_second.m_delayGain * v225);
        const Vector v227 = (v226 * v213);
        const Vector v228 = (v220 - v227);
        const Vector v229 = (m_state.m_second.m_sign * v107);
        const Vector v230 = (v229 * v199);
        const Vector v231 = (m_state.m_second.m_sign * v109);
        const Vector v232 = (v231 * v209);
        const Vector v233 = (v230 - v232);
        const Vector v234 = (m_state.m_second.m_delayGain * v233);
        const Vector v235 = (v234 * v213);
        const Vector v236 = (m_state.m_second.m_sign * v107);
        const Vector v237 = (v236 * v209);
        const Vector v238 = (m_state.m_second.m_sign * v109);
        const Vector v239 = (v238 * v199);
        const Vector v240 = (v237 + v239);
        const Vector v241 = (m_state.m_second.m_delayGain * v240);
        const Vector v242 = (v241 * v212);
        const Vector v243 = (v235 + v242);
        const Vector v244 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_mix);
        const Vector v245 = (v244 * v52);
        const Vector v246 = (m_state.m_mix * v107);
        const Vector v247 = (v245 + v246);
        m_output.m_numerator.m_real = v247;
        const Vector v248 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_mix);
        const Vector v249 = (v248 * v54);
        const Vector v250 = (m_state.m_mix * v109);
        const Vector v251 = (v249 + v250);
        m_output.m_numerator.m_imag = v251;
        const Vector v252 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_mix);
        const Vector v253 = (v252 * v161);
        const Vector v254 = (m_state.m_mix * v228);
        const Vector v255 = (v253 + v254);
        const Vector v256 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v255);
        m_output.m_denominator.m_real = v256;
        const Vector v257 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_mix);
        const Vector v258 = (v257 * v176);
        const Vector v259 = (m_state.m_mix * v243);
        const Vector v260 = (v258 + v259);
        const Vector v261 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) - v260);
        m_output.m_denominator.m_imag = v261;
    }

};
