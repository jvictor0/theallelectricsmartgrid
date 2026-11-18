#pragma once

// Generated from DSP.NaiveReuppingLadder.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"
#include "MultiChannelNumber.hpp"

struct NaiveReuppingLadderCore
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        double m_alpha;
        struct
        {
            double m_gain;
            double m_tanhGain;
        } m_saturation;

        double m_cutoff;
        double m_feedback;
        double m_stage1;
        double m_stage2;
        double m_stage3;
        double m_stage4;
        double m_input;
        double m_output;
        double m_kEff;
        struct
        {
            struct
            {
                double m_k;
                double m_a1;
                double m_a2;
                double m_a3;
                double m_ic1;
                double m_ic2;
                double m_lowPass;
                double m_bandPass;
                double m_highPass;
                double m_notch;
                double m_allPass;
            } m_stage1;

            struct
            {
                double m_k;
                double m_a1;
                double m_a2;
                double m_a3;
                double m_ic1;
                double m_ic2;
                double m_lowPass;
                double m_bandPass;
                double m_highPass;
                double m_notch;
                double m_allPass;
            } m_stage2;

            double m_power;
            double m_powerAlpha;
        } m_inputEnvelope;

        struct
        {
            struct
            {
                double m_k;
                double m_a1;
                double m_a2;
                double m_a3;
                double m_ic1;
                double m_ic2;
                double m_lowPass;
                double m_bandPass;
                double m_highPass;
                double m_notch;
                double m_allPass;
            } m_stage1;

            struct
            {
                double m_k;
                double m_a1;
                double m_a2;
                double m_a3;
                double m_ic1;
                double m_ic2;
                double m_lowPass;
                double m_bandPass;
                double m_highPass;
                double m_notch;
                double m_allPass;
            } m_stage2;

            double m_power;
            double m_powerAlpha;
        } m_feedbackEnvelope;

        double m_criticalFeedback;
        double m_kSafe;
        double m_baseFeedback;
        double m_referencePowerGain;
        double m_dampingScale;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (m_state.m_referencePowerGain * m_state.m_inputEnvelope.m_power);
        const double v1 = (m_state.m_feedbackEnvelope.m_power - v0);
        const bool v2 = (0.0 < v1);
        double v5;
        if (v2)
        {
            const double v3 = (m_state.m_referencePowerGain * m_state.m_inputEnvelope.m_power);
            const double v4 = (m_state.m_feedbackEnvelope.m_power - v3);
            v5 = v4;
        }
        else
        {
            v5 = 0.0;
        }

        const double v6 = (m_state.m_inputEnvelope.m_power + 0x15798ec0000000p-79);
        const double v7 = (v5 / v6);
        const double v8 = (m_state.m_dampingScale * v7);
        const double v9 = (v8 + 1.0);
        const double v10 = (m_state.m_baseFeedback / v9);
        const double v11 = (v10 * m_state.m_stage4);
        const double v12 = (input.m_x - v11);
        const double v13 = (1.0 - m_state.m_alpha);
        const double v14 = (v13 * m_state.m_stage1);
        const double v15 = std::fma(m_state.m_alpha, v12, v14);
        const double v16 = (v15 * m_state.m_saturation.m_gain);
        const double v17 = (v15 * m_state.m_saturation.m_gain);
        const double v18 = (v16 * v17);
        const double v19 = (v15 * m_state.m_saturation.m_gain);
        const double v20 = (v18 + 0x1b000000000000p-48);
        const double v21 = (v19 * v20);
        const double v22 = (v15 * m_state.m_saturation.m_gain);
        const double v23 = (v22 * 0x12000000000000p-49);
        const double v24 = (v15 * m_state.m_saturation.m_gain);
        const double v25 = std::fma(v23, v24, 0x1b000000000000p-48);
        const double v26 = (v21 / v25);
        const double v27 = (-1.0);
        const bool v28 = (v27 < v26);
        double v30;
        if (v28)
        {
            v30 = v26;
        }
        else
        {
            const double v29 = (-1.0);
            v30 = v29;
        }

        const bool v31 = (v30 < 1.0);
        double v36;
        if (v31)
        {
            const double v32 = (-1.0);
            const bool v33 = (v32 < v26);
            double v35;
            if (v33)
            {
                v35 = v26;
            }
            else
            {
                const double v34 = (-1.0);
                v35 = v34;
            }

            v36 = v35;
        }
        else
        {
            v36 = 1.0;
        }

        const double v37 = (v36 / m_state.m_saturation.m_tanhGain);
        const double v38 = (1.0 - m_state.m_alpha);
        const double v39 = (v38 * m_state.m_stage2);
        const double v40 = std::fma(m_state.m_alpha, v37, v39);
        const double v41 = (v40 * m_state.m_saturation.m_gain);
        const double v42 = (v40 * m_state.m_saturation.m_gain);
        const double v43 = (v41 * v42);
        const double v44 = (v40 * m_state.m_saturation.m_gain);
        const double v45 = (v43 + 0x1b000000000000p-48);
        const double v46 = (v44 * v45);
        const double v47 = (v40 * m_state.m_saturation.m_gain);
        const double v48 = (v47 * 0x12000000000000p-49);
        const double v49 = (v40 * m_state.m_saturation.m_gain);
        const double v50 = std::fma(v48, v49, 0x1b000000000000p-48);
        const double v51 = (v46 / v50);
        const double v52 = (-1.0);
        const bool v53 = (v52 < v51);
        double v55;
        if (v53)
        {
            v55 = v51;
        }
        else
        {
            const double v54 = (-1.0);
            v55 = v54;
        }

        const bool v56 = (v55 < 1.0);
        double v61;
        if (v56)
        {
            const double v57 = (-1.0);
            const bool v58 = (v57 < v51);
            double v60;
            if (v58)
            {
                v60 = v51;
            }
            else
            {
                const double v59 = (-1.0);
                v60 = v59;
            }

            v61 = v60;
        }
        else
        {
            v61 = 1.0;
        }

        const double v62 = (v61 / m_state.m_saturation.m_tanhGain);
        const double v63 = (1.0 - m_state.m_alpha);
        const double v64 = (v63 * m_state.m_stage3);
        const double v65 = std::fma(m_state.m_alpha, v62, v64);
        const double v66 = (v65 * m_state.m_saturation.m_gain);
        const double v67 = (v65 * m_state.m_saturation.m_gain);
        const double v68 = (v66 * v67);
        const double v69 = (v65 * m_state.m_saturation.m_gain);
        const double v70 = (v68 + 0x1b000000000000p-48);
        const double v71 = (v69 * v70);
        const double v72 = (v65 * m_state.m_saturation.m_gain);
        const double v73 = (v72 * 0x12000000000000p-49);
        const double v74 = (v65 * m_state.m_saturation.m_gain);
        const double v75 = std::fma(v73, v74, 0x1b000000000000p-48);
        const double v76 = (v71 / v75);
        const double v77 = (-1.0);
        const bool v78 = (v77 < v76);
        double v80;
        if (v78)
        {
            v80 = v76;
        }
        else
        {
            const double v79 = (-1.0);
            v80 = v79;
        }

        const bool v81 = (v80 < 1.0);
        double v86;
        if (v81)
        {
            const double v82 = (-1.0);
            const bool v83 = (v82 < v76);
            double v85;
            if (v83)
            {
                v85 = v76;
            }
            else
            {
                const double v84 = (-1.0);
                v85 = v84;
            }

            v86 = v85;
        }
        else
        {
            v86 = 1.0;
        }

        const double v87 = (v86 / m_state.m_saturation.m_tanhGain);
        const double v88 = (1.0 - m_state.m_alpha);
        const double v89 = (v88 * m_state.m_stage4);
        const double v90 = std::fma(m_state.m_alpha, v87, v89);
        const double v91 = (v90 * m_state.m_saturation.m_gain);
        const double v92 = (v90 * m_state.m_saturation.m_gain);
        const double v93 = (v91 * v92);
        const double v94 = (v90 * m_state.m_saturation.m_gain);
        const double v95 = (v93 + 0x1b000000000000p-48);
        const double v96 = (v94 * v95);
        const double v97 = (v90 * m_state.m_saturation.m_gain);
        const double v98 = (v97 * 0x12000000000000p-49);
        const double v99 = (v90 * m_state.m_saturation.m_gain);
        const double v100 = std::fma(v98, v99, 0x1b000000000000p-48);
        const double v101 = (v96 / v100);
        const double v102 = (-1.0);
        const bool v103 = (v102 < v101);
        double v105;
        if (v103)
        {
            v105 = v101;
        }
        else
        {
            const double v104 = (-1.0);
            v105 = v104;
        }

        const bool v106 = (v105 < 1.0);
        double v111;
        if (v106)
        {
            const double v107 = (-1.0);
            const bool v108 = (v107 < v101);
            double v110;
            if (v108)
            {
                v110 = v101;
            }
            else
            {
                const double v109 = (-1.0);
                v110 = v109;
            }

            v111 = v110;
        }
        else
        {
            v111 = 1.0;
        }

        const double v112 = (v111 / m_state.m_saturation.m_tanhGain);
        const double v113 = (v10 + 1.0);
        const double v114 = (v112 * v113);
        const double v115 = (input.m_x - m_state.m_inputEnvelope.m_stage1.m_ic2);
        const double v116 = (m_state.m_inputEnvelope.m_stage1.m_a1 * m_state.m_inputEnvelope.m_stage1.m_ic1);
        const double v117 = std::fma(m_state.m_inputEnvelope.m_stage1.m_a2, v115, v116);
        const double v118 = std::fma(m_state.m_inputEnvelope.m_stage1.m_a2, m_state.m_inputEnvelope.m_stage1.m_ic1, m_state.m_inputEnvelope.m_stage1.m_ic2);
        const double v119 = std::fma(m_state.m_inputEnvelope.m_stage1.m_a3, v115, v118);
        const double v120 = (-m_state.m_inputEnvelope.m_stage1.m_k);
        const double v121 = std::fma(v120, v117, input.m_x);
        const double v122 = (v121 - v119);
        const double v123 = (-m_state.m_inputEnvelope.m_stage1.m_ic1);
        const double v124 = std::fma(v117, 0x10000000000000p-51, v123);
        const double v125 = (-m_state.m_inputEnvelope.m_stage1.m_ic2);
        const double v126 = std::fma(v119, 0x10000000000000p-51, v125);
        const double v127 = (v119 + v122);
        const double v128 = (-m_state.m_inputEnvelope.m_stage1.m_k);
        const double v129 = (v128 * 0x10000000000000p-51);
        const double v130 = std::fma(v129, v117, input.m_x);
        const double v131 = (v117 - m_state.m_inputEnvelope.m_stage2.m_ic2);
        const double v132 = (m_state.m_inputEnvelope.m_stage2.m_a1 * m_state.m_inputEnvelope.m_stage2.m_ic1);
        const double v133 = std::fma(m_state.m_inputEnvelope.m_stage2.m_a2, v131, v132);
        const double v134 = std::fma(m_state.m_inputEnvelope.m_stage2.m_a2, m_state.m_inputEnvelope.m_stage2.m_ic1, m_state.m_inputEnvelope.m_stage2.m_ic2);
        const double v135 = std::fma(m_state.m_inputEnvelope.m_stage2.m_a3, v131, v134);
        const double v136 = (-m_state.m_inputEnvelope.m_stage2.m_k);
        const double v137 = std::fma(v136, v133, v117);
        const double v138 = (v137 - v135);
        const double v139 = (-m_state.m_inputEnvelope.m_stage2.m_ic1);
        const double v140 = std::fma(v133, 0x10000000000000p-51, v139);
        const double v141 = (-m_state.m_inputEnvelope.m_stage2.m_ic2);
        const double v142 = std::fma(v135, 0x10000000000000p-51, v141);
        const double v143 = (v135 + v138);
        const double v144 = (-m_state.m_inputEnvelope.m_stage2.m_k);
        const double v145 = (v144 * 0x10000000000000p-51);
        const double v146 = std::fma(v145, v133, v117);
        const double v147 = (v133 * v133);
        const double v148 = (v147 - m_state.m_inputEnvelope.m_power);
        const double v149 = (m_state.m_inputEnvelope.m_powerAlpha * v148);
        const double v150 = (m_state.m_inputEnvelope.m_power + v149);
        const double v151 = (v90 - m_state.m_feedbackEnvelope.m_stage1.m_ic2);
        const double v152 = (m_state.m_feedbackEnvelope.m_stage1.m_a1 * m_state.m_feedbackEnvelope.m_stage1.m_ic1);
        const double v153 = std::fma(m_state.m_feedbackEnvelope.m_stage1.m_a2, v151, v152);
        const double v154 = std::fma(m_state.m_feedbackEnvelope.m_stage1.m_a2, m_state.m_feedbackEnvelope.m_stage1.m_ic1, m_state.m_feedbackEnvelope.m_stage1.m_ic2);
        const double v155 = std::fma(m_state.m_feedbackEnvelope.m_stage1.m_a3, v151, v154);
        const double v156 = (-m_state.m_feedbackEnvelope.m_stage1.m_k);
        const double v157 = std::fma(v156, v153, v90);
        const double v158 = (v157 - v155);
        const double v159 = (-m_state.m_feedbackEnvelope.m_stage1.m_ic1);
        const double v160 = std::fma(v153, 0x10000000000000p-51, v159);
        const double v161 = (-m_state.m_feedbackEnvelope.m_stage1.m_ic2);
        const double v162 = std::fma(v155, 0x10000000000000p-51, v161);
        const double v163 = (v155 + v158);
        const double v164 = (-m_state.m_feedbackEnvelope.m_stage1.m_k);
        const double v165 = (v164 * 0x10000000000000p-51);
        const double v166 = std::fma(v165, v153, v90);
        const double v167 = (v153 - m_state.m_feedbackEnvelope.m_stage2.m_ic2);
        const double v168 = (m_state.m_feedbackEnvelope.m_stage2.m_a1 * m_state.m_feedbackEnvelope.m_stage2.m_ic1);
        const double v169 = std::fma(m_state.m_feedbackEnvelope.m_stage2.m_a2, v167, v168);
        const double v170 = std::fma(m_state.m_feedbackEnvelope.m_stage2.m_a2, m_state.m_feedbackEnvelope.m_stage2.m_ic1, m_state.m_feedbackEnvelope.m_stage2.m_ic2);
        const double v171 = std::fma(m_state.m_feedbackEnvelope.m_stage2.m_a3, v167, v170);
        const double v172 = (-m_state.m_feedbackEnvelope.m_stage2.m_k);
        const double v173 = std::fma(v172, v169, v153);
        const double v174 = (v173 - v171);
        const double v175 = (-m_state.m_feedbackEnvelope.m_stage2.m_ic1);
        const double v176 = std::fma(v169, 0x10000000000000p-51, v175);
        const double v177 = (-m_state.m_feedbackEnvelope.m_stage2.m_ic2);
        const double v178 = std::fma(v171, 0x10000000000000p-51, v177);
        const double v179 = (v171 + v174);
        const double v180 = (-m_state.m_feedbackEnvelope.m_stage2.m_k);
        const double v181 = (v180 * 0x10000000000000p-51);
        const double v182 = std::fma(v181, v169, v153);
        const double v183 = (v169 * v169);
        const double v184 = (v183 - m_state.m_feedbackEnvelope.m_power);
        const double v185 = (m_state.m_feedbackEnvelope.m_powerAlpha * v184);
        const double v186 = (m_state.m_feedbackEnvelope.m_power + v185);
        const double v187 = m_state.m_alpha;
        const double v188 = m_state.m_saturation.m_gain;
        const double v189 = m_state.m_saturation.m_tanhGain;
        const double v190 = m_state.m_cutoff;
        const double v191 = m_state.m_feedback;
        const double v192 = v15;
        const double v193 = v40;
        const double v194 = v65;
        const double v195 = v90;
        const double v196 = v12;
        const double v197 = v114;
        const double v198 = v10;
        const double v199 = m_state.m_inputEnvelope.m_stage1.m_k;
        const double v200 = m_state.m_inputEnvelope.m_stage1.m_a1;
        const double v201 = m_state.m_inputEnvelope.m_stage1.m_a2;
        const double v202 = m_state.m_inputEnvelope.m_stage1.m_a3;
        const double v203 = v124;
        const double v204 = v126;
        const double v205 = v119;
        const double v206 = v117;
        const double v207 = v122;
        const double v208 = v127;
        const double v209 = v130;
        const double v210 = m_state.m_inputEnvelope.m_stage2.m_k;
        const double v211 = m_state.m_inputEnvelope.m_stage2.m_a1;
        const double v212 = m_state.m_inputEnvelope.m_stage2.m_a2;
        const double v213 = m_state.m_inputEnvelope.m_stage2.m_a3;
        const double v214 = v140;
        const double v215 = v142;
        const double v216 = v135;
        const double v217 = v133;
        const double v218 = v138;
        const double v219 = v143;
        const double v220 = v146;
        const double v221 = v150;
        const double v222 = m_state.m_inputEnvelope.m_powerAlpha;
        const double v223 = m_state.m_feedbackEnvelope.m_stage1.m_k;
        const double v224 = m_state.m_feedbackEnvelope.m_stage1.m_a1;
        const double v225 = m_state.m_feedbackEnvelope.m_stage1.m_a2;
        const double v226 = m_state.m_feedbackEnvelope.m_stage1.m_a3;
        const double v227 = v160;
        const double v228 = v162;
        const double v229 = v155;
        const double v230 = v153;
        const double v231 = v158;
        const double v232 = v163;
        const double v233 = v166;
        const double v234 = m_state.m_feedbackEnvelope.m_stage2.m_k;
        const double v235 = m_state.m_feedbackEnvelope.m_stage2.m_a1;
        const double v236 = m_state.m_feedbackEnvelope.m_stage2.m_a2;
        const double v237 = m_state.m_feedbackEnvelope.m_stage2.m_a3;
        const double v238 = v176;
        const double v239 = v178;
        const double v240 = v171;
        const double v241 = v169;
        const double v242 = v174;
        const double v243 = v179;
        const double v244 = v182;
        const double v245 = v186;
        const double v246 = m_state.m_feedbackEnvelope.m_powerAlpha;
        const double v247 = m_state.m_criticalFeedback;
        const double v248 = m_state.m_kSafe;
        const double v249 = m_state.m_baseFeedback;
        const double v250 = m_state.m_referencePowerGain;
        const double v251 = m_state.m_dampingScale;
        m_state.m_alpha = v187;
        m_state.m_saturation.m_gain = v188;
        m_state.m_saturation.m_tanhGain = v189;
        m_state.m_cutoff = v190;
        m_state.m_feedback = v191;
        m_state.m_stage1 = v192;
        m_state.m_stage2 = v193;
        m_state.m_stage3 = v194;
        m_state.m_stage4 = v195;
        m_state.m_input = v196;
        m_state.m_output = v197;
        m_state.m_kEff = v198;
        m_state.m_inputEnvelope.m_stage1.m_k = v199;
        m_state.m_inputEnvelope.m_stage1.m_a1 = v200;
        m_state.m_inputEnvelope.m_stage1.m_a2 = v201;
        m_state.m_inputEnvelope.m_stage1.m_a3 = v202;
        m_state.m_inputEnvelope.m_stage1.m_ic1 = v203;
        m_state.m_inputEnvelope.m_stage1.m_ic2 = v204;
        m_state.m_inputEnvelope.m_stage1.m_lowPass = v205;
        m_state.m_inputEnvelope.m_stage1.m_bandPass = v206;
        m_state.m_inputEnvelope.m_stage1.m_highPass = v207;
        m_state.m_inputEnvelope.m_stage1.m_notch = v208;
        m_state.m_inputEnvelope.m_stage1.m_allPass = v209;
        m_state.m_inputEnvelope.m_stage2.m_k = v210;
        m_state.m_inputEnvelope.m_stage2.m_a1 = v211;
        m_state.m_inputEnvelope.m_stage2.m_a2 = v212;
        m_state.m_inputEnvelope.m_stage2.m_a3 = v213;
        m_state.m_inputEnvelope.m_stage2.m_ic1 = v214;
        m_state.m_inputEnvelope.m_stage2.m_ic2 = v215;
        m_state.m_inputEnvelope.m_stage2.m_lowPass = v216;
        m_state.m_inputEnvelope.m_stage2.m_bandPass = v217;
        m_state.m_inputEnvelope.m_stage2.m_highPass = v218;
        m_state.m_inputEnvelope.m_stage2.m_notch = v219;
        m_state.m_inputEnvelope.m_stage2.m_allPass = v220;
        m_state.m_inputEnvelope.m_power = v221;
        m_state.m_inputEnvelope.m_powerAlpha = v222;
        m_state.m_feedbackEnvelope.m_stage1.m_k = v223;
        m_state.m_feedbackEnvelope.m_stage1.m_a1 = v224;
        m_state.m_feedbackEnvelope.m_stage1.m_a2 = v225;
        m_state.m_feedbackEnvelope.m_stage1.m_a3 = v226;
        m_state.m_feedbackEnvelope.m_stage1.m_ic1 = v227;
        m_state.m_feedbackEnvelope.m_stage1.m_ic2 = v228;
        m_state.m_feedbackEnvelope.m_stage1.m_lowPass = v229;
        m_state.m_feedbackEnvelope.m_stage1.m_bandPass = v230;
        m_state.m_feedbackEnvelope.m_stage1.m_highPass = v231;
        m_state.m_feedbackEnvelope.m_stage1.m_notch = v232;
        m_state.m_feedbackEnvelope.m_stage1.m_allPass = v233;
        m_state.m_feedbackEnvelope.m_stage2.m_k = v234;
        m_state.m_feedbackEnvelope.m_stage2.m_a1 = v235;
        m_state.m_feedbackEnvelope.m_stage2.m_a2 = v236;
        m_state.m_feedbackEnvelope.m_stage2.m_a3 = v237;
        m_state.m_feedbackEnvelope.m_stage2.m_ic1 = v238;
        m_state.m_feedbackEnvelope.m_stage2.m_ic2 = v239;
        m_state.m_feedbackEnvelope.m_stage2.m_lowPass = v240;
        m_state.m_feedbackEnvelope.m_stage2.m_bandPass = v241;
        m_state.m_feedbackEnvelope.m_stage2.m_highPass = v242;
        m_state.m_feedbackEnvelope.m_stage2.m_notch = v243;
        m_state.m_feedbackEnvelope.m_stage2.m_allPass = v244;
        m_state.m_feedbackEnvelope.m_power = v245;
        m_state.m_feedbackEnvelope.m_powerAlpha = v246;
        m_state.m_criticalFeedback = v247;
        m_state.m_kSafe = v248;
        m_state.m_baseFeedback = v249;
        m_state.m_referencePowerGain = v250;
        m_state.m_dampingScale = v251;
    }

    // Coefficients generated from DSP.NaiveReuppingLadder.computeCoefficientsFloat.
    //

    struct LogicalSpec
    {
        double m_cutoff;
        double m_resonance;
        double m_gain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const bool v0 = (spec.m_cutoff < 0x10000000000000p-53);
        double v1;
        if (v0)
        {
            v1 = spec.m_cutoff;
        }
        else
        {
            v1 = 0x10000000000000p-53;
        }

        const double v2 = (v1 * 0x10000000000000p-53);
        const double v3 = Math4096::Sin2pi(MultiChannelNumber<double, 1>::Splat(v2))[0];
        const double v4 = (v3 * v3);
        const double v5 = (1.0 + v4);
        const double v6 = std::sqrt(v5);
        const double v7 = (v3 * 0x10000000000000p-51);
        const double v8 = (v6 - v3);
        const double v9 = (v7 * v8);
        const double v10 = (v3 * 0x10000000000000p-51);
        const double v11 = (v6 + v3);
        const double v12 = (v10 * v11);
        const double v13 = (v12 * 0x10000000000000p-53);
        const bool v14 = (1.0 < v13);
        double v16;
        if (v14)
        {
            v16 = 1.0;
        }
        else
        {
            const double v15 = (v12 * 0x10000000000000p-53);
            v16 = v15;
        }

        const double v17 = (1.0 - v16);
        const double v18 = (1.0 + v16);
        const double v19 = (v17 * v18);
        const double v20 = (0x13e4f10125eab4p-44 * 0x13e4f10125eab4p-44);
        const double v21 = (1.0 / v20);
        const double v22 = (v16 * v16);
        const double v23 = (v22 * v21);
        const bool v24 = (v19 < v23);
        double v25;
        if (v24)
        {
            v25 = v23;
        }
        else
        {
            v25 = v19;
        }

        const double v26 = std::sqrt(v25);
        const double v27 = (v16 / v26);
        const double v28 = (v12 + 0x10000000000000p-51);
        const bool v29 = (v12 < 0x10000000000000p-51);
        double v30;
        if (v29)
        {
            v30 = 0x10000000000000p-51;
        }
        else
        {
            v30 = v12;
        }

        const double v31 = (0x10000000000000p-51 / v30);
        const double v32 = (1.0 + v31);
        const double v33 = (v32 * v32);
        const bool v34 = (v12 < 0x10000000000000p-51);
        double v37;
        if (v34)
        {
            const double v35 = (v28 * v28);
            v37 = v35;
        }
        else
        {
            const double v36 = (v33 * v33);
            v37 = v36;
        }

        const double v38 = (0x1bd6b403814bc9p-54 / 0x1921fb54442d18p-50);
        const double v39 = (v3 * v38);
        const double v40 = (spec.m_gain * spec.m_gain);
        const double v41 = (v40 + 0x1b000000000000p-48);
        const double v42 = (spec.m_gain * v41);
        const double v43 = (spec.m_gain * 0x12000000000000p-49);
        const double v44 = std::fma(v43, spec.m_gain, 0x1b000000000000p-48);
        const double v45 = (v42 / v44);
        const double v46 = (-1.0);
        const bool v47 = (v46 < v45);
        double v49;
        if (v47)
        {
            v49 = v45;
        }
        else
        {
            const double v48 = (-1.0);
            v49 = v48;
        }

        const bool v50 = (v49 < 1.0);
        double v55;
        if (v50)
        {
            const double v51 = (-1.0);
            const bool v52 = (v51 < v45);
            double v54;
            if (v52)
            {
                v54 = v45;
            }
            else
            {
                const double v53 = (-1.0);
                v54 = v53;
            }

            v55 = v54;
        }
        else
        {
            v55 = 1.0;
        }

        const double v56 = (spec.m_gain / v55);
        const double v57 = (0x10000000000000p-51 - v9);
        const double v58 = (v9 / v57);
        const double v59 = (v58 * v56);
        const double v60 = (v59 * v59);
        const double v61 = (v60 * v60);
        const double v62 = (v61 + 0x15798ee2308c3ap-79);
        const double v63 = (0x1ccccccccccccdp-53 / v62);
        const double v64 = (spec.m_resonance * 0x1f333333333333p-51);
        const bool v65 = (v63 < v64);
        double v67;
        if (v65)
        {
            v67 = v63;
        }
        else
        {
            const double v66 = (spec.m_resonance * 0x1f333333333333p-51);
            v67 = v66;
        }

        const double v68 = (v37 - v67);
        const double v69 = (1.0 / v68);
        const double v70 = (spec.m_gain * spec.m_gain);
        const double v71 = (v70 + 0x1b000000000000p-48);
        const double v72 = (spec.m_gain * v71);
        const double v73 = (spec.m_gain * 0x12000000000000p-49);
        const double v74 = std::fma(v73, spec.m_gain, 0x1b000000000000p-48);
        const double v75 = (v72 / v74);
        const double v76 = (-1.0);
        const bool v77 = (v76 < v75);
        double v79;
        if (v77)
        {
            v79 = v75;
        }
        else
        {
            const double v78 = (-1.0);
            v79 = v78;
        }

        const bool v80 = (v79 < 1.0);
        double v85;
        if (v80)
        {
            const double v81 = (-1.0);
            const bool v82 = (v81 < v75);
            double v84;
            if (v82)
            {
                v84 = v75;
            }
            else
            {
                const double v83 = (-1.0);
                v84 = v83;
            }

            v85 = v84;
        }
        else
        {
            v85 = 1.0;
        }

        const double v86 = (spec.m_resonance * 0x1f333333333333p-51);
        const double v87 = (v27 + 1.0);
        const double v88 = std::fma(v27, v87, 1.0);
        const double v89 = (1.0 / v88);
        const double v90 = (v27 * v89);
        const double v91 = (v27 * v90);
        const double v92 = (v27 + 1.0);
        const double v93 = std::fma(v27, v92, 1.0);
        const double v94 = (1.0 / v93);
        const double v95 = (v27 * v94);
        const double v96 = (v27 * v95);
        const double v97 = (v27 + 1.0);
        const double v98 = std::fma(v27, v97, 1.0);
        const double v99 = (1.0 / v98);
        const double v100 = (v27 * v99);
        const double v101 = (v27 * v100);
        const double v102 = (v27 + 1.0);
        const double v103 = std::fma(v27, v102, 1.0);
        const double v104 = (1.0 / v103);
        const double v105 = (v27 * v104);
        const double v106 = (v27 * v105);
        const double v107 = (v69 * v69);
        const double v108 = (v56 * v56);
        const double v109 = (v108 * v56);
        const double v110 = (v109 * 0x1a36e2e0000000p-66);
        const double v111 = v9;
        const double v112 = spec.m_gain;
        const double v113 = v85;
        const double v114 = v1;
        const double v115 = v86;
        const double v116 = 1.0;
        const double v117 = v89;
        const double v118 = v90;
        const double v119 = v91;
        const double v120 = 1.0;
        const double v121 = v94;
        const double v122 = v95;
        const double v123 = v96;
        const double v124 = v39;
        const double v125 = 1.0;
        const double v126 = v99;
        const double v127 = v100;
        const double v128 = v101;
        const double v129 = 1.0;
        const double v130 = v104;
        const double v131 = v105;
        const double v132 = v106;
        const double v133 = v39;
        const double v134 = v37;
        const double v135 = v63;
        const double v136 = v67;
        const double v137 = v107;
        const double v138 = v110;
        m_state.m_alpha = v111;
        m_state.m_saturation.m_gain = v112;
        m_state.m_saturation.m_tanhGain = v113;
        m_state.m_cutoff = v114;
        m_state.m_feedback = v115;
        m_state.m_inputEnvelope.m_stage1.m_k = v116;
        m_state.m_inputEnvelope.m_stage1.m_a1 = v117;
        m_state.m_inputEnvelope.m_stage1.m_a2 = v118;
        m_state.m_inputEnvelope.m_stage1.m_a3 = v119;
        m_state.m_inputEnvelope.m_stage2.m_k = v120;
        m_state.m_inputEnvelope.m_stage2.m_a1 = v121;
        m_state.m_inputEnvelope.m_stage2.m_a2 = v122;
        m_state.m_inputEnvelope.m_stage2.m_a3 = v123;
        m_state.m_inputEnvelope.m_powerAlpha = v124;
        m_state.m_feedbackEnvelope.m_stage1.m_k = v125;
        m_state.m_feedbackEnvelope.m_stage1.m_a1 = v126;
        m_state.m_feedbackEnvelope.m_stage1.m_a2 = v127;
        m_state.m_feedbackEnvelope.m_stage1.m_a3 = v128;
        m_state.m_feedbackEnvelope.m_stage2.m_k = v129;
        m_state.m_feedbackEnvelope.m_stage2.m_a1 = v130;
        m_state.m_feedbackEnvelope.m_stage2.m_a2 = v131;
        m_state.m_feedbackEnvelope.m_stage2.m_a3 = v132;
        m_state.m_feedbackEnvelope.m_powerAlpha = v133;
        m_state.m_criticalFeedback = v134;
        m_state.m_kSafe = v135;
        m_state.m_baseFeedback = v136;
        m_state.m_referencePowerGain = v137;
        m_state.m_dampingScale = v138;
    }

    // Reset generated from DSP.NaiveReuppingLadder.resetFloat.
    //

    void Reset()
    {
        const double v0 = 0.0;
        const double v1 = 0.0;
        const double v2 = 0.0;
        const double v3 = 0.0;
        const double v4 = 0.0;
        const double v5 = 0.0;
        const double v6 = 0.0;
        const double v7 = 0.0;
        const double v8 = 0.0;
        const double v9 = 0.0;
        const double v10 = 0.0;
        const double v11 = 0.0;
        const double v12 = 0.0;
        const double v13 = 0.0;
        const double v14 = 0.0;
        const double v15 = 0.0;
        const double v16 = 0.0;
        const double v17 = 0.0;
        const double v18 = 0.0;
        const double v19 = 0.0;
        const double v20 = 0.0;
        const double v21 = 0.0;
        const double v22 = 0.0;
        const double v23 = 0.0;
        const double v24 = 0.0;
        const double v25 = 0.0;
        const double v26 = 0.0;
        const double v27 = 0.0;
        const double v28 = 0.0;
        const double v29 = 0.0;
        const double v30 = 0.0;
        const double v31 = 0.0;
        const double v32 = 0.0;
        const double v33 = 0.0;
        const double v34 = 0.0;
        const double v35 = 0.0;
        const double v36 = 0.0;
        m_state.m_stage1 = v0;
        m_state.m_stage2 = v1;
        m_state.m_stage3 = v2;
        m_state.m_stage4 = v3;
        m_state.m_input = v4;
        m_state.m_output = v5;
        m_state.m_kEff = v6;
        m_state.m_inputEnvelope.m_stage1.m_ic1 = v7;
        m_state.m_inputEnvelope.m_stage1.m_ic2 = v8;
        m_state.m_inputEnvelope.m_stage1.m_lowPass = v9;
        m_state.m_inputEnvelope.m_stage1.m_bandPass = v10;
        m_state.m_inputEnvelope.m_stage1.m_highPass = v11;
        m_state.m_inputEnvelope.m_stage1.m_notch = v12;
        m_state.m_inputEnvelope.m_stage1.m_allPass = v13;
        m_state.m_inputEnvelope.m_stage2.m_ic1 = v14;
        m_state.m_inputEnvelope.m_stage2.m_ic2 = v15;
        m_state.m_inputEnvelope.m_stage2.m_lowPass = v16;
        m_state.m_inputEnvelope.m_stage2.m_bandPass = v17;
        m_state.m_inputEnvelope.m_stage2.m_highPass = v18;
        m_state.m_inputEnvelope.m_stage2.m_notch = v19;
        m_state.m_inputEnvelope.m_stage2.m_allPass = v20;
        m_state.m_inputEnvelope.m_power = v21;
        m_state.m_feedbackEnvelope.m_stage1.m_ic1 = v22;
        m_state.m_feedbackEnvelope.m_stage1.m_ic2 = v23;
        m_state.m_feedbackEnvelope.m_stage1.m_lowPass = v24;
        m_state.m_feedbackEnvelope.m_stage1.m_bandPass = v25;
        m_state.m_feedbackEnvelope.m_stage1.m_highPass = v26;
        m_state.m_feedbackEnvelope.m_stage1.m_notch = v27;
        m_state.m_feedbackEnvelope.m_stage1.m_allPass = v28;
        m_state.m_feedbackEnvelope.m_stage2.m_ic1 = v29;
        m_state.m_feedbackEnvelope.m_stage2.m_ic2 = v30;
        m_state.m_feedbackEnvelope.m_stage2.m_lowPass = v31;
        m_state.m_feedbackEnvelope.m_stage2.m_bandPass = v32;
        m_state.m_feedbackEnvelope.m_stage2.m_highPass = v33;
        m_state.m_feedbackEnvelope.m_stage2.m_notch = v34;
        m_state.m_feedbackEnvelope.m_stage2.m_allPass = v35;
        m_state.m_feedbackEnvelope.m_power = v36;
    }

};
