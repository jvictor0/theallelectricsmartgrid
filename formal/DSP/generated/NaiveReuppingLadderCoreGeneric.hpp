#pragma once

// Generated from DSP.NaiveReuppingLadder.step. Do not edit.
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
struct NaiveReuppingLadderCoreGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_alpha;
        struct
        {
            Vector m_gain;
            Vector m_tanhGain;
        } m_saturation;

        Vector m_cutoff;
        Vector m_feedback;
        Vector m_stage1;
        Vector m_stage2;
        Vector m_stage3;
        Vector m_stage4;
        Vector m_input;
        Vector m_output;
        Vector m_kEff;
        struct
        {
            struct
            {
                Vector m_k;
                Vector m_a1;
                Vector m_a2;
                Vector m_a3;
                Vector m_ic1;
                Vector m_ic2;
                Vector m_lowPass;
                Vector m_bandPass;
                Vector m_highPass;
                Vector m_notch;
                Vector m_allPass;
            } m_stage1;

            struct
            {
                Vector m_k;
                Vector m_a1;
                Vector m_a2;
                Vector m_a3;
                Vector m_ic1;
                Vector m_ic2;
                Vector m_lowPass;
                Vector m_bandPass;
                Vector m_highPass;
                Vector m_notch;
                Vector m_allPass;
            } m_stage2;

            Vector m_power;
            Vector m_powerAlpha;
        } m_inputEnvelope;

        struct
        {
            struct
            {
                Vector m_k;
                Vector m_a1;
                Vector m_a2;
                Vector m_a3;
                Vector m_ic1;
                Vector m_ic2;
                Vector m_lowPass;
                Vector m_bandPass;
                Vector m_highPass;
                Vector m_notch;
                Vector m_allPass;
            } m_stage1;

            struct
            {
                Vector m_k;
                Vector m_a1;
                Vector m_a2;
                Vector m_a3;
                Vector m_ic1;
                Vector m_ic2;
                Vector m_lowPass;
                Vector m_bandPass;
                Vector m_highPass;
                Vector m_notch;
                Vector m_allPass;
            } m_stage2;

            Vector m_power;
            Vector m_powerAlpha;
        } m_feedbackEnvelope;

        Vector m_criticalFeedback;
        Vector m_kSafe;
        Vector m_baseFeedback;
        Vector m_referencePowerGain;
        Vector m_dampingScale;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (m_state.m_referencePowerGain * m_state.m_inputEnvelope.m_power);
        const Vector v1 = (m_state.m_feedbackEnvelope.m_power - v0);
        const Vector v2 = (m_state.m_referencePowerGain * m_state.m_inputEnvelope.m_power);
        const Vector v3 = (m_state.m_feedbackEnvelope.m_power - v2);
        const Vector v4 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v1, v3, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v5 = (m_state.m_inputEnvelope.m_power + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xabcc76p-50f : 0x15798ec0000000p-79)));
        const Vector v6 = (v4 / v5);
        const Vector v7 = (m_state.m_dampingScale * v6);
        const Vector v8 = (v7 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v9 = (m_state.m_baseFeedback / v8);
        const Vector v10 = (v9 * m_state.m_stage4);
        const Vector v11 = (input.m_x - v10);
        const Vector v12 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v13 = (v12 * m_state.m_stage1);
        const Vector v14 = Vector::FusedMultiplyAdd(m_state.m_alpha, v11, v13);
        const Vector v15 = (v14 * m_state.m_saturation.m_gain);
        const Vector v16 = (v15 * v15);
        const Vector v17 = (v15 * (v16 + Vector::Splat(27)));
        const Vector v18 = Vector::FusedMultiplyAdd(v15 * Vector::Splat(9), v15, Vector::Splat(27));
        const Vector v19 = (v17 / v18);
        const Vector v20 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v19));
        const Vector v21 = (v20 / m_state.m_saturation.m_tanhGain);
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v23 = (v22 * m_state.m_stage2);
        const Vector v24 = Vector::FusedMultiplyAdd(m_state.m_alpha, v21, v23);
        const Vector v25 = (v24 * m_state.m_saturation.m_gain);
        const Vector v26 = (v25 * v25);
        const Vector v27 = (v25 * (v26 + Vector::Splat(27)));
        const Vector v28 = Vector::FusedMultiplyAdd(v25 * Vector::Splat(9), v25, Vector::Splat(27));
        const Vector v29 = (v27 / v28);
        const Vector v30 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v29));
        const Vector v31 = (v30 / m_state.m_saturation.m_tanhGain);
        const Vector v32 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v33 = (v32 * m_state.m_stage3);
        const Vector v34 = Vector::FusedMultiplyAdd(m_state.m_alpha, v31, v33);
        const Vector v35 = (v34 * m_state.m_saturation.m_gain);
        const Vector v36 = (v35 * v35);
        const Vector v37 = (v35 * (v36 + Vector::Splat(27)));
        const Vector v38 = Vector::FusedMultiplyAdd(v35 * Vector::Splat(9), v35, Vector::Splat(27));
        const Vector v39 = (v37 / v38);
        const Vector v40 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v39));
        const Vector v41 = (v40 / m_state.m_saturation.m_tanhGain);
        const Vector v42 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v43 = (v42 * m_state.m_stage4);
        const Vector v44 = Vector::FusedMultiplyAdd(m_state.m_alpha, v41, v43);
        const Vector v45 = (v44 * m_state.m_saturation.m_gain);
        const Vector v46 = (v45 * v45);
        const Vector v47 = (v45 * (v46 + Vector::Splat(27)));
        const Vector v48 = Vector::FusedMultiplyAdd(v45 * Vector::Splat(9), v45, Vector::Splat(27));
        const Vector v49 = (v47 / v48);
        const Vector v50 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v49));
        const Vector v51 = (v50 / m_state.m_saturation.m_tanhGain);
        const Vector v52 = (v9 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v53 = (v51 * v52);
        const Vector v54 = (input.m_x - m_state.m_inputEnvelope.m_stage1.m_ic2);
        const Vector v55 = (m_state.m_inputEnvelope.m_stage1.m_a1 * m_state.m_inputEnvelope.m_stage1.m_ic1);
        const Vector v56 = Vector::FusedMultiplyAdd(m_state.m_inputEnvelope.m_stage1.m_a2, v54, v55);
        const Vector v57 = Vector::FusedMultiplyAdd(m_state.m_inputEnvelope.m_stage1.m_a2, m_state.m_inputEnvelope.m_stage1.m_ic1, m_state.m_inputEnvelope.m_stage1.m_ic2);
        const Vector v58 = Vector::FusedMultiplyAdd(m_state.m_inputEnvelope.m_stage1.m_a3, v54, v57);
        const Vector v59 = (-m_state.m_inputEnvelope.m_stage1.m_k);
        const Vector v60 = Vector::FusedMultiplyAdd(v59, v56, input.m_x);
        const Vector v61 = (v60 - v58);
        const Vector v62 = (-m_state.m_inputEnvelope.m_stage1.m_ic1);
        const Vector v63 = Vector::FusedMultiplyAdd(v56, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v62);
        const Vector v64 = (-m_state.m_inputEnvelope.m_stage1.m_ic2);
        const Vector v65 = Vector::FusedMultiplyAdd(v58, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v64);
        const Vector v66 = (v58 + v61);
        const Vector v67 = (-m_state.m_inputEnvelope.m_stage1.m_k);
        const Vector v68 = (v67 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v69 = Vector::FusedMultiplyAdd(v68, v56, input.m_x);
        const Vector v70 = (v56 - m_state.m_inputEnvelope.m_stage2.m_ic2);
        const Vector v71 = (m_state.m_inputEnvelope.m_stage2.m_a1 * m_state.m_inputEnvelope.m_stage2.m_ic1);
        const Vector v72 = Vector::FusedMultiplyAdd(m_state.m_inputEnvelope.m_stage2.m_a2, v70, v71);
        const Vector v73 = Vector::FusedMultiplyAdd(m_state.m_inputEnvelope.m_stage2.m_a2, m_state.m_inputEnvelope.m_stage2.m_ic1, m_state.m_inputEnvelope.m_stage2.m_ic2);
        const Vector v74 = Vector::FusedMultiplyAdd(m_state.m_inputEnvelope.m_stage2.m_a3, v70, v73);
        const Vector v75 = (-m_state.m_inputEnvelope.m_stage2.m_k);
        const Vector v76 = Vector::FusedMultiplyAdd(v75, v72, v56);
        const Vector v77 = (v76 - v74);
        const Vector v78 = (-m_state.m_inputEnvelope.m_stage2.m_ic1);
        const Vector v79 = Vector::FusedMultiplyAdd(v72, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v78);
        const Vector v80 = (-m_state.m_inputEnvelope.m_stage2.m_ic2);
        const Vector v81 = Vector::FusedMultiplyAdd(v74, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v80);
        const Vector v82 = (v74 + v77);
        const Vector v83 = (-m_state.m_inputEnvelope.m_stage2.m_k);
        const Vector v84 = (v83 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v85 = Vector::FusedMultiplyAdd(v84, v72, v56);
        const Vector v86 = (v72 * v72);
        const Vector v87 = (v86 - m_state.m_inputEnvelope.m_power);
        const Vector v88 = (m_state.m_inputEnvelope.m_powerAlpha * v87);
        const Vector v89 = (m_state.m_inputEnvelope.m_power + v88);
        const Vector v90 = (v44 - m_state.m_feedbackEnvelope.m_stage1.m_ic2);
        const Vector v91 = (m_state.m_feedbackEnvelope.m_stage1.m_a1 * m_state.m_feedbackEnvelope.m_stage1.m_ic1);
        const Vector v92 = Vector::FusedMultiplyAdd(m_state.m_feedbackEnvelope.m_stage1.m_a2, v90, v91);
        const Vector v93 = Vector::FusedMultiplyAdd(m_state.m_feedbackEnvelope.m_stage1.m_a2, m_state.m_feedbackEnvelope.m_stage1.m_ic1, m_state.m_feedbackEnvelope.m_stage1.m_ic2);
        const Vector v94 = Vector::FusedMultiplyAdd(m_state.m_feedbackEnvelope.m_stage1.m_a3, v90, v93);
        const Vector v95 = (-m_state.m_feedbackEnvelope.m_stage1.m_k);
        const Vector v96 = Vector::FusedMultiplyAdd(v95, v92, v44);
        const Vector v97 = (v96 - v94);
        const Vector v98 = (-m_state.m_feedbackEnvelope.m_stage1.m_ic1);
        const Vector v99 = Vector::FusedMultiplyAdd(v92, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v98);
        const Vector v100 = (-m_state.m_feedbackEnvelope.m_stage1.m_ic2);
        const Vector v101 = Vector::FusedMultiplyAdd(v94, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v100);
        const Vector v102 = (v94 + v97);
        const Vector v103 = (-m_state.m_feedbackEnvelope.m_stage1.m_k);
        const Vector v104 = (v103 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v105 = Vector::FusedMultiplyAdd(v104, v92, v44);
        const Vector v106 = (v92 - m_state.m_feedbackEnvelope.m_stage2.m_ic2);
        const Vector v107 = (m_state.m_feedbackEnvelope.m_stage2.m_a1 * m_state.m_feedbackEnvelope.m_stage2.m_ic1);
        const Vector v108 = Vector::FusedMultiplyAdd(m_state.m_feedbackEnvelope.m_stage2.m_a2, v106, v107);
        const Vector v109 = Vector::FusedMultiplyAdd(m_state.m_feedbackEnvelope.m_stage2.m_a2, m_state.m_feedbackEnvelope.m_stage2.m_ic1, m_state.m_feedbackEnvelope.m_stage2.m_ic2);
        const Vector v110 = Vector::FusedMultiplyAdd(m_state.m_feedbackEnvelope.m_stage2.m_a3, v106, v109);
        const Vector v111 = (-m_state.m_feedbackEnvelope.m_stage2.m_k);
        const Vector v112 = Vector::FusedMultiplyAdd(v111, v108, v92);
        const Vector v113 = (v112 - v110);
        const Vector v114 = (-m_state.m_feedbackEnvelope.m_stage2.m_ic1);
        const Vector v115 = Vector::FusedMultiplyAdd(v108, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v114);
        const Vector v116 = (-m_state.m_feedbackEnvelope.m_stage2.m_ic2);
        const Vector v117 = Vector::FusedMultiplyAdd(v110, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v116);
        const Vector v118 = (v110 + v113);
        const Vector v119 = (-m_state.m_feedbackEnvelope.m_stage2.m_k);
        const Vector v120 = (v119 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v121 = Vector::FusedMultiplyAdd(v120, v108, v92);
        const Vector v122 = (v108 * v108);
        const Vector v123 = (v122 - m_state.m_feedbackEnvelope.m_power);
        const Vector v124 = (m_state.m_feedbackEnvelope.m_powerAlpha * v123);
        const Vector v125 = (m_state.m_feedbackEnvelope.m_power + v124);
        const Vector v126 = m_state.m_alpha;
        const Vector v127 = m_state.m_saturation.m_gain;
        const Vector v128 = m_state.m_saturation.m_tanhGain;
        const Vector v129 = m_state.m_cutoff;
        const Vector v130 = m_state.m_feedback;
        const Vector v131 = v14;
        const Vector v132 = v24;
        const Vector v133 = v34;
        const Vector v134 = v44;
        const Vector v135 = v11;
        const Vector v136 = v53;
        const Vector v137 = v9;
        const Vector v138 = m_state.m_inputEnvelope.m_stage1.m_k;
        const Vector v139 = m_state.m_inputEnvelope.m_stage1.m_a1;
        const Vector v140 = m_state.m_inputEnvelope.m_stage1.m_a2;
        const Vector v141 = m_state.m_inputEnvelope.m_stage1.m_a3;
        const Vector v142 = v63;
        const Vector v143 = v65;
        const Vector v144 = v58;
        const Vector v145 = v56;
        const Vector v146 = v61;
        const Vector v147 = v66;
        const Vector v148 = v69;
        const Vector v149 = m_state.m_inputEnvelope.m_stage2.m_k;
        const Vector v150 = m_state.m_inputEnvelope.m_stage2.m_a1;
        const Vector v151 = m_state.m_inputEnvelope.m_stage2.m_a2;
        const Vector v152 = m_state.m_inputEnvelope.m_stage2.m_a3;
        const Vector v153 = v79;
        const Vector v154 = v81;
        const Vector v155 = v74;
        const Vector v156 = v72;
        const Vector v157 = v77;
        const Vector v158 = v82;
        const Vector v159 = v85;
        const Vector v160 = v89;
        const Vector v161 = m_state.m_inputEnvelope.m_powerAlpha;
        const Vector v162 = m_state.m_feedbackEnvelope.m_stage1.m_k;
        const Vector v163 = m_state.m_feedbackEnvelope.m_stage1.m_a1;
        const Vector v164 = m_state.m_feedbackEnvelope.m_stage1.m_a2;
        const Vector v165 = m_state.m_feedbackEnvelope.m_stage1.m_a3;
        const Vector v166 = v99;
        const Vector v167 = v101;
        const Vector v168 = v94;
        const Vector v169 = v92;
        const Vector v170 = v97;
        const Vector v171 = v102;
        const Vector v172 = v105;
        const Vector v173 = m_state.m_feedbackEnvelope.m_stage2.m_k;
        const Vector v174 = m_state.m_feedbackEnvelope.m_stage2.m_a1;
        const Vector v175 = m_state.m_feedbackEnvelope.m_stage2.m_a2;
        const Vector v176 = m_state.m_feedbackEnvelope.m_stage2.m_a3;
        const Vector v177 = v115;
        const Vector v178 = v117;
        const Vector v179 = v110;
        const Vector v180 = v108;
        const Vector v181 = v113;
        const Vector v182 = v118;
        const Vector v183 = v121;
        const Vector v184 = v125;
        const Vector v185 = m_state.m_feedbackEnvelope.m_powerAlpha;
        const Vector v186 = m_state.m_criticalFeedback;
        const Vector v187 = m_state.m_kSafe;
        const Vector v188 = m_state.m_baseFeedback;
        const Vector v189 = m_state.m_referencePowerGain;
        const Vector v190 = m_state.m_dampingScale;
        m_state.m_alpha = v126;
        m_state.m_saturation.m_gain = v127;
        m_state.m_saturation.m_tanhGain = v128;
        m_state.m_cutoff = v129;
        m_state.m_feedback = v130;
        m_state.m_stage1 = v131;
        m_state.m_stage2 = v132;
        m_state.m_stage3 = v133;
        m_state.m_stage4 = v134;
        m_state.m_input = v135;
        m_state.m_output = v136;
        m_state.m_kEff = v137;
        m_state.m_inputEnvelope.m_stage1.m_k = v138;
        m_state.m_inputEnvelope.m_stage1.m_a1 = v139;
        m_state.m_inputEnvelope.m_stage1.m_a2 = v140;
        m_state.m_inputEnvelope.m_stage1.m_a3 = v141;
        m_state.m_inputEnvelope.m_stage1.m_ic1 = v142;
        m_state.m_inputEnvelope.m_stage1.m_ic2 = v143;
        m_state.m_inputEnvelope.m_stage1.m_lowPass = v144;
        m_state.m_inputEnvelope.m_stage1.m_bandPass = v145;
        m_state.m_inputEnvelope.m_stage1.m_highPass = v146;
        m_state.m_inputEnvelope.m_stage1.m_notch = v147;
        m_state.m_inputEnvelope.m_stage1.m_allPass = v148;
        m_state.m_inputEnvelope.m_stage2.m_k = v149;
        m_state.m_inputEnvelope.m_stage2.m_a1 = v150;
        m_state.m_inputEnvelope.m_stage2.m_a2 = v151;
        m_state.m_inputEnvelope.m_stage2.m_a3 = v152;
        m_state.m_inputEnvelope.m_stage2.m_ic1 = v153;
        m_state.m_inputEnvelope.m_stage2.m_ic2 = v154;
        m_state.m_inputEnvelope.m_stage2.m_lowPass = v155;
        m_state.m_inputEnvelope.m_stage2.m_bandPass = v156;
        m_state.m_inputEnvelope.m_stage2.m_highPass = v157;
        m_state.m_inputEnvelope.m_stage2.m_notch = v158;
        m_state.m_inputEnvelope.m_stage2.m_allPass = v159;
        m_state.m_inputEnvelope.m_power = v160;
        m_state.m_inputEnvelope.m_powerAlpha = v161;
        m_state.m_feedbackEnvelope.m_stage1.m_k = v162;
        m_state.m_feedbackEnvelope.m_stage1.m_a1 = v163;
        m_state.m_feedbackEnvelope.m_stage1.m_a2 = v164;
        m_state.m_feedbackEnvelope.m_stage1.m_a3 = v165;
        m_state.m_feedbackEnvelope.m_stage1.m_ic1 = v166;
        m_state.m_feedbackEnvelope.m_stage1.m_ic2 = v167;
        m_state.m_feedbackEnvelope.m_stage1.m_lowPass = v168;
        m_state.m_feedbackEnvelope.m_stage1.m_bandPass = v169;
        m_state.m_feedbackEnvelope.m_stage1.m_highPass = v170;
        m_state.m_feedbackEnvelope.m_stage1.m_notch = v171;
        m_state.m_feedbackEnvelope.m_stage1.m_allPass = v172;
        m_state.m_feedbackEnvelope.m_stage2.m_k = v173;
        m_state.m_feedbackEnvelope.m_stage2.m_a1 = v174;
        m_state.m_feedbackEnvelope.m_stage2.m_a2 = v175;
        m_state.m_feedbackEnvelope.m_stage2.m_a3 = v176;
        m_state.m_feedbackEnvelope.m_stage2.m_ic1 = v177;
        m_state.m_feedbackEnvelope.m_stage2.m_ic2 = v178;
        m_state.m_feedbackEnvelope.m_stage2.m_lowPass = v179;
        m_state.m_feedbackEnvelope.m_stage2.m_bandPass = v180;
        m_state.m_feedbackEnvelope.m_stage2.m_highPass = v181;
        m_state.m_feedbackEnvelope.m_stage2.m_notch = v182;
        m_state.m_feedbackEnvelope.m_stage2.m_allPass = v183;
        m_state.m_feedbackEnvelope.m_power = v184;
        m_state.m_feedbackEnvelope.m_powerAlpha = v185;
        m_state.m_criticalFeedback = v186;
        m_state.m_kSafe = v187;
        m_state.m_baseFeedback = v188;
        m_state.m_referencePowerGain = v189;
        m_state.m_dampingScale = v190;
    }

    // Coefficients generated from DSP.NaiveReuppingLadder.computeCoefficients.
    //

    struct LogicalSpec
    {
        Vector m_cutoff;
        Vector m_resonance;
        Vector m_gain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Vector::IfLess(spec.m_cutoff, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)), spec.m_cutoff, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v1 = (v0 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v2 = Math4096::Sin2pi(v1);
        const Vector v3 = (v2 * v2);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v3);
        const Vector v5 = v4.Sqrt();
        const Vector v6 = (v2 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v7 = (v5 - v2);
        const Vector v8 = (v6 * v7);
        const Vector v9 = (v2 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v10 = (v5 + v2);
        const Vector v11 = (v9 * v10);
        const Vector v12 = (v11 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v13 = (v11 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v14 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v12, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v13);
        const Vector v15 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v14);
        const Vector v16 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v14);
        const Vector v17 = (v15 * v16);
        const Vector v18 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x9f2788p-15f : 0x13e4f10125eab4p-44)) * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x9f2788p-15f : 0x13e4f10125eab4p-44)));
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v18);
        const Vector v20 = (v14 * v14);
        const Vector v21 = (v20 * v19);
        const Vector v22 = Vector::IfLess(v17, v21, v21, v17);
        const Vector v23 = v22.Sqrt();
        const Vector v24 = (v14 / v23);
        const Vector v25 = (v11 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v26 = Vector::IfLess(v11, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v11);
        const Vector v27 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) / v26);
        const Vector v28 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v27);
        const Vector v29 = (v28 * v28);
        const Vector v30 = (v25 * v25);
        const Vector v31 = (v29 * v29);
        const Vector v32 = Vector::IfLess(v11, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v30, v31);
        const Vector v33 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xdeb5a0p-25f : 0x1bd6b403814bc9p-54)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc90fdbp-21f : 0x1921fb54442d18p-50)));
        const Vector v34 = (v2 * v33);
        const Vector v35 = (spec.m_gain * spec.m_gain);
        const Vector v36 = (spec.m_gain * (v35 + Vector::Splat(27)));
        const Vector v37 = Vector::FusedMultiplyAdd(spec.m_gain * Vector::Splat(9), spec.m_gain, Vector::Splat(27));
        const Vector v38 = (v36 / v37);
        const Vector v39 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v38));
        const Vector v40 = (spec.m_gain / v39);
        const Vector v41 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) - v8);
        const Vector v42 = (v8 / v41);
        const Vector v43 = (v42 * v40);
        const Vector v44 = (v43 * v43);
        const Vector v45 = (v44 * v44);
        const Vector v46 = (v45 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xabcc77p-50f : 0x15798ee2308c3ap-79)));
        const Vector v47 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xe66666p-24f : 0x1ccccccccccccdp-53)) / v46);
        const Vector v48 = (spec.m_resonance * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xf9999ap-22f : 0x1f333333333333p-51)));
        const Vector v49 = (spec.m_resonance * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xf9999ap-22f : 0x1f333333333333p-51)));
        const Vector v50 = Vector::IfLess(v47, v48, v47, v49);
        const Vector v51 = (v32 - v50);
        const Vector v52 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v51);
        const Vector v53 = (spec.m_gain * spec.m_gain);
        const Vector v54 = (spec.m_gain * (v53 + Vector::Splat(27)));
        const Vector v55 = Vector::FusedMultiplyAdd(spec.m_gain * Vector::Splat(9), spec.m_gain, Vector::Splat(27));
        const Vector v56 = (v54 / v55);
        const Vector v57 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v56));
        const Vector v58 = (spec.m_resonance * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xf9999ap-22f : 0x1f333333333333p-51)));
        const Vector v59 = (v24 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v60 = Vector::FusedMultiplyAdd(v24, v59, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v61 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v60);
        const Vector v62 = (v24 * v61);
        const Vector v63 = (v24 * v62);
        const Vector v64 = (v24 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v65 = Vector::FusedMultiplyAdd(v24, v64, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v66 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v65);
        const Vector v67 = (v24 * v66);
        const Vector v68 = (v24 * v67);
        const Vector v69 = (v24 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v70 = Vector::FusedMultiplyAdd(v24, v69, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v71 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v70);
        const Vector v72 = (v24 * v71);
        const Vector v73 = (v24 * v72);
        const Vector v74 = (v24 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v75 = Vector::FusedMultiplyAdd(v24, v74, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v76 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v75);
        const Vector v77 = (v24 * v76);
        const Vector v78 = (v24 * v77);
        const Vector v79 = (v52 * v52);
        const Vector v80 = (v40 * v40);
        const Vector v81 = (v80 * v40);
        const Vector v82 = (v81 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd1b717p-37f : 0x1a36e2e0000000p-66)));
        const Vector v83 = v8;
        const Vector v84 = spec.m_gain;
        const Vector v85 = v57;
        const Vector v86 = v0;
        const Vector v87 = v58;
        const Vector v88 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0));
        const Vector v89 = v61;
        const Vector v90 = v62;
        const Vector v91 = v63;
        const Vector v92 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0));
        const Vector v93 = v66;
        const Vector v94 = v67;
        const Vector v95 = v68;
        const Vector v96 = v34;
        const Vector v97 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0));
        const Vector v98 = v71;
        const Vector v99 = v72;
        const Vector v100 = v73;
        const Vector v101 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0));
        const Vector v102 = v76;
        const Vector v103 = v77;
        const Vector v104 = v78;
        const Vector v105 = v34;
        const Vector v106 = v32;
        const Vector v107 = v47;
        const Vector v108 = v50;
        const Vector v109 = v79;
        const Vector v110 = v82;
        m_state.m_alpha = v83;
        m_state.m_saturation.m_gain = v84;
        m_state.m_saturation.m_tanhGain = v85;
        m_state.m_cutoff = v86;
        m_state.m_feedback = v87;
        m_state.m_inputEnvelope.m_stage1.m_k = v88;
        m_state.m_inputEnvelope.m_stage1.m_a1 = v89;
        m_state.m_inputEnvelope.m_stage1.m_a2 = v90;
        m_state.m_inputEnvelope.m_stage1.m_a3 = v91;
        m_state.m_inputEnvelope.m_stage2.m_k = v92;
        m_state.m_inputEnvelope.m_stage2.m_a1 = v93;
        m_state.m_inputEnvelope.m_stage2.m_a2 = v94;
        m_state.m_inputEnvelope.m_stage2.m_a3 = v95;
        m_state.m_inputEnvelope.m_powerAlpha = v96;
        m_state.m_feedbackEnvelope.m_stage1.m_k = v97;
        m_state.m_feedbackEnvelope.m_stage1.m_a1 = v98;
        m_state.m_feedbackEnvelope.m_stage1.m_a2 = v99;
        m_state.m_feedbackEnvelope.m_stage1.m_a3 = v100;
        m_state.m_feedbackEnvelope.m_stage2.m_k = v101;
        m_state.m_feedbackEnvelope.m_stage2.m_a1 = v102;
        m_state.m_feedbackEnvelope.m_stage2.m_a2 = v103;
        m_state.m_feedbackEnvelope.m_stage2.m_a3 = v104;
        m_state.m_feedbackEnvelope.m_powerAlpha = v105;
        m_state.m_criticalFeedback = v106;
        m_state.m_kSafe = v107;
        m_state.m_baseFeedback = v108;
        m_state.m_referencePowerGain = v109;
        m_state.m_dampingScale = v110;
    }

    // Reset generated from DSP.NaiveReuppingLadder.reset.
    //

    void Reset()
    {
        const Vector v0 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v1 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v2 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v3 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v4 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v5 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v6 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v7 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v8 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v9 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v10 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v11 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v12 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v13 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v14 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v15 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v16 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v17 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v18 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v19 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v20 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v21 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v22 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v23 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v24 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v25 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v26 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v27 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v28 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v29 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v30 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v31 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v32 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v33 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v34 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v35 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v36 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
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
