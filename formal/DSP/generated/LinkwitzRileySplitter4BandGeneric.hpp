#pragma once

// Generated from DSP.LinkwitzRileySplitter.step4Band. Do not edit.
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
struct LinkwitzRileySplitter4BandGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
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
            } m_first;

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
            } m_second;

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
                } m_svf;

                struct
                {
                    Vector m_x;
                } m_low;

                struct
                {
                    Vector m_x;
                } m_high;

            } m_lowPhase;

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
                } m_svf;

                struct
                {
                    Vector m_x;
                } m_low;

                struct
                {
                    Vector m_x;
                } m_high;

            } m_highPhase;

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
                } m_first;

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
                } m_second;

                struct
                {
                    Vector m_x;
                } m_lowPhase;

                struct
                {
                    Vector m_x;
                } m_highPhase;

                struct
                {
                    Vector m_x;
                } m_low;

                struct
                {
                    Vector m_x;
                } m_high;

            } m_low;

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
                } m_first;

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
                } m_second;

                struct
                {
                    Vector m_x;
                } m_lowPhase;

                struct
                {
                    Vector m_x;
                } m_highPhase;

                struct
                {
                    Vector m_x;
                } m_low;

                struct
                {
                    Vector m_x;
                } m_high;

            } m_high;

        } m_tree;

        std::array<Vector, 4> m_bands;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_tree.m_first.m_ic2);
        const Vector v1 = (m_state.m_tree.m_first.m_a1 * m_state.m_tree.m_first.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_tree.m_first.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_tree.m_first.m_a2, m_state.m_tree.m_first.m_ic1, m_state.m_tree.m_first.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_tree.m_first.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_tree.m_first.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_tree.m_first.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_tree.m_first.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (-m_state.m_tree.m_first.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        const Vector v16 = (v4 - m_state.m_tree.m_second.m_ic2);
        const Vector v17 = (m_state.m_tree.m_second.m_a1 * m_state.m_tree.m_second.m_ic1);
        const Vector v18 = Vector::FusedMultiplyAdd(m_state.m_tree.m_second.m_a2, v16, v17);
        const Vector v19 = Vector::FusedMultiplyAdd(m_state.m_tree.m_second.m_a2, m_state.m_tree.m_second.m_ic1, m_state.m_tree.m_second.m_ic2);
        const Vector v20 = Vector::FusedMultiplyAdd(m_state.m_tree.m_second.m_a3, v16, v19);
        const Vector v21 = (-m_state.m_tree.m_second.m_k);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, v18, v4);
        const Vector v23 = (v22 - v20);
        const Vector v24 = (-m_state.m_tree.m_second.m_ic1);
        const Vector v25 = Vector::FusedMultiplyAdd(v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v24);
        const Vector v26 = (-m_state.m_tree.m_second.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        const Vector v28 = (v20 + v23);
        const Vector v29 = (-m_state.m_tree.m_second.m_k);
        const Vector v30 = (v29 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v31 = Vector::FusedMultiplyAdd(v30, v18, v4);
        const Vector v32 = (v20 - m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v33 = (m_state.m_tree.m_lowPhase.m_svf.m_a1 * m_state.m_tree.m_lowPhase.m_svf.m_ic1);
        const Vector v34 = Vector::FusedMultiplyAdd(m_state.m_tree.m_lowPhase.m_svf.m_a2, v32, v33);
        const Vector v35 = Vector::FusedMultiplyAdd(m_state.m_tree.m_lowPhase.m_svf.m_a2, m_state.m_tree.m_lowPhase.m_svf.m_ic1, m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v36 = Vector::FusedMultiplyAdd(m_state.m_tree.m_lowPhase.m_svf.m_a3, v32, v35);
        const Vector v37 = (-m_state.m_tree.m_lowPhase.m_svf.m_k);
        const Vector v38 = Vector::FusedMultiplyAdd(v37, v34, v20);
        const Vector v39 = (v38 - v36);
        const Vector v40 = (-m_state.m_tree.m_lowPhase.m_svf.m_ic1);
        const Vector v41 = Vector::FusedMultiplyAdd(v34, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v40);
        const Vector v42 = (-m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v43 = Vector::FusedMultiplyAdd(v36, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v42);
        const Vector v44 = (v36 + v39);
        const Vector v45 = (-m_state.m_tree.m_lowPhase.m_svf.m_k);
        const Vector v46 = (v45 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v47 = Vector::FusedMultiplyAdd(v46, v34, v20);
        const Vector v48 = (v15 - v20);
        const Vector v49 = (v48 - m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v50 = (m_state.m_tree.m_highPhase.m_svf.m_a1 * m_state.m_tree.m_highPhase.m_svf.m_ic1);
        const Vector v51 = Vector::FusedMultiplyAdd(m_state.m_tree.m_highPhase.m_svf.m_a2, v49, v50);
        const Vector v52 = Vector::FusedMultiplyAdd(m_state.m_tree.m_highPhase.m_svf.m_a2, m_state.m_tree.m_highPhase.m_svf.m_ic1, m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v53 = Vector::FusedMultiplyAdd(m_state.m_tree.m_highPhase.m_svf.m_a3, v49, v52);
        const Vector v54 = (-m_state.m_tree.m_highPhase.m_svf.m_k);
        const Vector v55 = (v15 - v20);
        const Vector v56 = Vector::FusedMultiplyAdd(v54, v51, v55);
        const Vector v57 = (v56 - v53);
        const Vector v58 = (-m_state.m_tree.m_highPhase.m_svf.m_ic1);
        const Vector v59 = Vector::FusedMultiplyAdd(v51, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v58);
        const Vector v60 = (-m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v61 = Vector::FusedMultiplyAdd(v53, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v60);
        const Vector v62 = (v53 + v57);
        const Vector v63 = (-m_state.m_tree.m_highPhase.m_svf.m_k);
        const Vector v64 = (v63 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v65 = (v15 - v20);
        const Vector v66 = Vector::FusedMultiplyAdd(v64, v51, v65);
        const Vector v67 = (v47 - m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v68 = (m_state.m_tree.m_low.m_first.m_a1 * m_state.m_tree.m_low.m_first.m_ic1);
        const Vector v69 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_first.m_a2, v67, v68);
        const Vector v70 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_first.m_a2, m_state.m_tree.m_low.m_first.m_ic1, m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v71 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_first.m_a3, v67, v70);
        const Vector v72 = (-m_state.m_tree.m_low.m_first.m_k);
        const Vector v73 = Vector::FusedMultiplyAdd(v72, v69, v47);
        const Vector v74 = (v73 - v71);
        const Vector v75 = (-m_state.m_tree.m_low.m_first.m_ic1);
        const Vector v76 = Vector::FusedMultiplyAdd(v69, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v75);
        const Vector v77 = (-m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v78 = Vector::FusedMultiplyAdd(v71, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v77);
        const Vector v79 = (v71 + v74);
        const Vector v80 = (-m_state.m_tree.m_low.m_first.m_k);
        const Vector v81 = (v80 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v82 = Vector::FusedMultiplyAdd(v81, v69, v47);
        const Vector v83 = (v71 - m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v84 = (m_state.m_tree.m_low.m_second.m_a1 * m_state.m_tree.m_low.m_second.m_ic1);
        const Vector v85 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_second.m_a2, v83, v84);
        const Vector v86 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_second.m_a2, m_state.m_tree.m_low.m_second.m_ic1, m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v87 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_second.m_a3, v83, v86);
        const Vector v88 = (-m_state.m_tree.m_low.m_second.m_k);
        const Vector v89 = Vector::FusedMultiplyAdd(v88, v85, v71);
        const Vector v90 = (v89 - v87);
        const Vector v91 = (-m_state.m_tree.m_low.m_second.m_ic1);
        const Vector v92 = Vector::FusedMultiplyAdd(v85, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v91);
        const Vector v93 = (-m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v94 = Vector::FusedMultiplyAdd(v87, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v93);
        const Vector v95 = (v87 + v90);
        const Vector v96 = (-m_state.m_tree.m_low.m_second.m_k);
        const Vector v97 = (v96 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v98 = Vector::FusedMultiplyAdd(v97, v85, v71);
        const Vector v99 = (v82 - v87);
        const Vector v100 = (v66 - m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v101 = (m_state.m_tree.m_high.m_first.m_a1 * m_state.m_tree.m_high.m_first.m_ic1);
        const Vector v102 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_first.m_a2, v100, v101);
        const Vector v103 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_first.m_a2, m_state.m_tree.m_high.m_first.m_ic1, m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v104 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_first.m_a3, v100, v103);
        const Vector v105 = (-m_state.m_tree.m_high.m_first.m_k);
        const Vector v106 = Vector::FusedMultiplyAdd(v105, v102, v66);
        const Vector v107 = (v106 - v104);
        const Vector v108 = (-m_state.m_tree.m_high.m_first.m_ic1);
        const Vector v109 = Vector::FusedMultiplyAdd(v102, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v108);
        const Vector v110 = (-m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v111 = Vector::FusedMultiplyAdd(v104, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v110);
        const Vector v112 = (v104 + v107);
        const Vector v113 = (-m_state.m_tree.m_high.m_first.m_k);
        const Vector v114 = (v113 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v115 = Vector::FusedMultiplyAdd(v114, v102, v66);
        const Vector v116 = (v104 - m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v117 = (m_state.m_tree.m_high.m_second.m_a1 * m_state.m_tree.m_high.m_second.m_ic1);
        const Vector v118 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_second.m_a2, v116, v117);
        const Vector v119 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_second.m_a2, m_state.m_tree.m_high.m_second.m_ic1, m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v120 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_second.m_a3, v116, v119);
        const Vector v121 = (-m_state.m_tree.m_high.m_second.m_k);
        const Vector v122 = Vector::FusedMultiplyAdd(v121, v118, v104);
        const Vector v123 = (v122 - v120);
        const Vector v124 = (-m_state.m_tree.m_high.m_second.m_ic1);
        const Vector v125 = Vector::FusedMultiplyAdd(v118, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v124);
        const Vector v126 = (-m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v127 = Vector::FusedMultiplyAdd(v120, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v126);
        const Vector v128 = (v120 + v123);
        const Vector v129 = (-m_state.m_tree.m_high.m_second.m_k);
        const Vector v130 = (v129 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v131 = Vector::FusedMultiplyAdd(v130, v118, v104);
        const Vector v132 = (v115 - v120);
        const Vector v133 = m_state.m_tree.m_first.m_k;
        const Vector v134 = m_state.m_tree.m_first.m_a1;
        const Vector v135 = m_state.m_tree.m_first.m_a2;
        const Vector v136 = m_state.m_tree.m_first.m_a3;
        const Vector v137 = v9;
        const Vector v138 = v11;
        const Vector v139 = v4;
        const Vector v140 = v2;
        const Vector v141 = v7;
        const Vector v142 = v12;
        const Vector v143 = v15;
        const Vector v144 = m_state.m_tree.m_second.m_k;
        const Vector v145 = m_state.m_tree.m_second.m_a1;
        const Vector v146 = m_state.m_tree.m_second.m_a2;
        const Vector v147 = m_state.m_tree.m_second.m_a3;
        const Vector v148 = v25;
        const Vector v149 = v27;
        const Vector v150 = v20;
        const Vector v151 = v18;
        const Vector v152 = v23;
        const Vector v153 = v28;
        const Vector v154 = v31;
        const Vector v155 = m_state.m_tree.m_lowPhase.m_svf.m_k;
        const Vector v156 = m_state.m_tree.m_lowPhase.m_svf.m_a1;
        const Vector v157 = m_state.m_tree.m_lowPhase.m_svf.m_a2;
        const Vector v158 = m_state.m_tree.m_lowPhase.m_svf.m_a3;
        const Vector v159 = v41;
        const Vector v160 = v43;
        const Vector v161 = v36;
        const Vector v162 = v34;
        const Vector v163 = v39;
        const Vector v164 = v44;
        const Vector v165 = v47;
        const Vector v166 = v47;
        const Vector v167 = v47;
        const Vector v168 = m_state.m_tree.m_highPhase.m_svf.m_k;
        const Vector v169 = m_state.m_tree.m_highPhase.m_svf.m_a1;
        const Vector v170 = m_state.m_tree.m_highPhase.m_svf.m_a2;
        const Vector v171 = m_state.m_tree.m_highPhase.m_svf.m_a3;
        const Vector v172 = v59;
        const Vector v173 = v61;
        const Vector v174 = v53;
        const Vector v175 = v51;
        const Vector v176 = v57;
        const Vector v177 = v62;
        const Vector v178 = v66;
        const Vector v179 = v66;
        const Vector v180 = v66;
        const Vector v181 = m_state.m_tree.m_low.m_first.m_k;
        const Vector v182 = m_state.m_tree.m_low.m_first.m_a1;
        const Vector v183 = m_state.m_tree.m_low.m_first.m_a2;
        const Vector v184 = m_state.m_tree.m_low.m_first.m_a3;
        const Vector v185 = v76;
        const Vector v186 = v78;
        const Vector v187 = v71;
        const Vector v188 = v69;
        const Vector v189 = v74;
        const Vector v190 = v79;
        const Vector v191 = v82;
        const Vector v192 = m_state.m_tree.m_low.m_second.m_k;
        const Vector v193 = m_state.m_tree.m_low.m_second.m_a1;
        const Vector v194 = m_state.m_tree.m_low.m_second.m_a2;
        const Vector v195 = m_state.m_tree.m_low.m_second.m_a3;
        const Vector v196 = v92;
        const Vector v197 = v94;
        const Vector v198 = v87;
        const Vector v199 = v85;
        const Vector v200 = v90;
        const Vector v201 = v95;
        const Vector v202 = v98;
        const Vector v203 = v87;
        const Vector v204 = v99;
        const Vector v205 = v87;
        const Vector v206 = v99;
        const Vector v207 = m_state.m_tree.m_high.m_first.m_k;
        const Vector v208 = m_state.m_tree.m_high.m_first.m_a1;
        const Vector v209 = m_state.m_tree.m_high.m_first.m_a2;
        const Vector v210 = m_state.m_tree.m_high.m_first.m_a3;
        const Vector v211 = v109;
        const Vector v212 = v111;
        const Vector v213 = v104;
        const Vector v214 = v102;
        const Vector v215 = v107;
        const Vector v216 = v112;
        const Vector v217 = v115;
        const Vector v218 = m_state.m_tree.m_high.m_second.m_k;
        const Vector v219 = m_state.m_tree.m_high.m_second.m_a1;
        const Vector v220 = m_state.m_tree.m_high.m_second.m_a2;
        const Vector v221 = m_state.m_tree.m_high.m_second.m_a3;
        const Vector v222 = v125;
        const Vector v223 = v127;
        const Vector v224 = v120;
        const Vector v225 = v118;
        const Vector v226 = v123;
        const Vector v227 = v128;
        const Vector v228 = v131;
        const Vector v229 = v120;
        const Vector v230 = v132;
        const Vector v231 = v120;
        const Vector v232 = v132;
        const Vector v233 = v87;
        const Vector v234 = v99;
        const Vector v235 = v120;
        const Vector v236 = v132;
        m_state.m_tree.m_first.m_k = v133;
        m_state.m_tree.m_first.m_a1 = v134;
        m_state.m_tree.m_first.m_a2 = v135;
        m_state.m_tree.m_first.m_a3 = v136;
        m_state.m_tree.m_first.m_ic1 = v137;
        m_state.m_tree.m_first.m_ic2 = v138;
        m_state.m_tree.m_first.m_lowPass = v139;
        m_state.m_tree.m_first.m_bandPass = v140;
        m_state.m_tree.m_first.m_highPass = v141;
        m_state.m_tree.m_first.m_notch = v142;
        m_state.m_tree.m_first.m_allPass = v143;
        m_state.m_tree.m_second.m_k = v144;
        m_state.m_tree.m_second.m_a1 = v145;
        m_state.m_tree.m_second.m_a2 = v146;
        m_state.m_tree.m_second.m_a3 = v147;
        m_state.m_tree.m_second.m_ic1 = v148;
        m_state.m_tree.m_second.m_ic2 = v149;
        m_state.m_tree.m_second.m_lowPass = v150;
        m_state.m_tree.m_second.m_bandPass = v151;
        m_state.m_tree.m_second.m_highPass = v152;
        m_state.m_tree.m_second.m_notch = v153;
        m_state.m_tree.m_second.m_allPass = v154;
        m_state.m_tree.m_lowPhase.m_svf.m_k = v155;
        m_state.m_tree.m_lowPhase.m_svf.m_a1 = v156;
        m_state.m_tree.m_lowPhase.m_svf.m_a2 = v157;
        m_state.m_tree.m_lowPhase.m_svf.m_a3 = v158;
        m_state.m_tree.m_lowPhase.m_svf.m_ic1 = v159;
        m_state.m_tree.m_lowPhase.m_svf.m_ic2 = v160;
        m_state.m_tree.m_lowPhase.m_svf.m_lowPass = v161;
        m_state.m_tree.m_lowPhase.m_svf.m_bandPass = v162;
        m_state.m_tree.m_lowPhase.m_svf.m_highPass = v163;
        m_state.m_tree.m_lowPhase.m_svf.m_notch = v164;
        m_state.m_tree.m_lowPhase.m_svf.m_allPass = v165;
        m_state.m_tree.m_lowPhase.m_low.m_x = v166;
        m_state.m_tree.m_lowPhase.m_high.m_x = v167;
        m_state.m_tree.m_highPhase.m_svf.m_k = v168;
        m_state.m_tree.m_highPhase.m_svf.m_a1 = v169;
        m_state.m_tree.m_highPhase.m_svf.m_a2 = v170;
        m_state.m_tree.m_highPhase.m_svf.m_a3 = v171;
        m_state.m_tree.m_highPhase.m_svf.m_ic1 = v172;
        m_state.m_tree.m_highPhase.m_svf.m_ic2 = v173;
        m_state.m_tree.m_highPhase.m_svf.m_lowPass = v174;
        m_state.m_tree.m_highPhase.m_svf.m_bandPass = v175;
        m_state.m_tree.m_highPhase.m_svf.m_highPass = v176;
        m_state.m_tree.m_highPhase.m_svf.m_notch = v177;
        m_state.m_tree.m_highPhase.m_svf.m_allPass = v178;
        m_state.m_tree.m_highPhase.m_low.m_x = v179;
        m_state.m_tree.m_highPhase.m_high.m_x = v180;
        m_state.m_tree.m_low.m_first.m_k = v181;
        m_state.m_tree.m_low.m_first.m_a1 = v182;
        m_state.m_tree.m_low.m_first.m_a2 = v183;
        m_state.m_tree.m_low.m_first.m_a3 = v184;
        m_state.m_tree.m_low.m_first.m_ic1 = v185;
        m_state.m_tree.m_low.m_first.m_ic2 = v186;
        m_state.m_tree.m_low.m_first.m_lowPass = v187;
        m_state.m_tree.m_low.m_first.m_bandPass = v188;
        m_state.m_tree.m_low.m_first.m_highPass = v189;
        m_state.m_tree.m_low.m_first.m_notch = v190;
        m_state.m_tree.m_low.m_first.m_allPass = v191;
        m_state.m_tree.m_low.m_second.m_k = v192;
        m_state.m_tree.m_low.m_second.m_a1 = v193;
        m_state.m_tree.m_low.m_second.m_a2 = v194;
        m_state.m_tree.m_low.m_second.m_a3 = v195;
        m_state.m_tree.m_low.m_second.m_ic1 = v196;
        m_state.m_tree.m_low.m_second.m_ic2 = v197;
        m_state.m_tree.m_low.m_second.m_lowPass = v198;
        m_state.m_tree.m_low.m_second.m_bandPass = v199;
        m_state.m_tree.m_low.m_second.m_highPass = v200;
        m_state.m_tree.m_low.m_second.m_notch = v201;
        m_state.m_tree.m_low.m_second.m_allPass = v202;
        m_state.m_tree.m_low.m_lowPhase.m_x = v203;
        m_state.m_tree.m_low.m_highPhase.m_x = v204;
        m_state.m_tree.m_low.m_low.m_x = v205;
        m_state.m_tree.m_low.m_high.m_x = v206;
        m_state.m_tree.m_high.m_first.m_k = v207;
        m_state.m_tree.m_high.m_first.m_a1 = v208;
        m_state.m_tree.m_high.m_first.m_a2 = v209;
        m_state.m_tree.m_high.m_first.m_a3 = v210;
        m_state.m_tree.m_high.m_first.m_ic1 = v211;
        m_state.m_tree.m_high.m_first.m_ic2 = v212;
        m_state.m_tree.m_high.m_first.m_lowPass = v213;
        m_state.m_tree.m_high.m_first.m_bandPass = v214;
        m_state.m_tree.m_high.m_first.m_highPass = v215;
        m_state.m_tree.m_high.m_first.m_notch = v216;
        m_state.m_tree.m_high.m_first.m_allPass = v217;
        m_state.m_tree.m_high.m_second.m_k = v218;
        m_state.m_tree.m_high.m_second.m_a1 = v219;
        m_state.m_tree.m_high.m_second.m_a2 = v220;
        m_state.m_tree.m_high.m_second.m_a3 = v221;
        m_state.m_tree.m_high.m_second.m_ic1 = v222;
        m_state.m_tree.m_high.m_second.m_ic2 = v223;
        m_state.m_tree.m_high.m_second.m_lowPass = v224;
        m_state.m_tree.m_high.m_second.m_bandPass = v225;
        m_state.m_tree.m_high.m_second.m_highPass = v226;
        m_state.m_tree.m_high.m_second.m_notch = v227;
        m_state.m_tree.m_high.m_second.m_allPass = v228;
        m_state.m_tree.m_high.m_lowPhase.m_x = v229;
        m_state.m_tree.m_high.m_highPhase.m_x = v230;
        m_state.m_tree.m_high.m_low.m_x = v231;
        m_state.m_tree.m_high.m_high.m_x = v232;
        m_state.m_bands[0] = v233;
        m_state.m_bands[1] = v234;
        m_state.m_bands[2] = v235;
        m_state.m_bands[3] = v236;
    }

    // Coefficients generated from DSP.LinkwitzRileySplitter.computeCoefficients4Band.
    //

    struct LogicalSpec
    {
        struct
        {
            Vector m_value;
            struct
            {
                Vector m_value;
                struct
                {
                } m_low;

                struct
                {
                } m_high;

            } m_low;

            struct
            {
                Vector m_value;
                struct
                {
                } m_low;

                struct
                {
                } m_high;

            } m_high;

        } m_tree;

    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v2 = Math4096::Cos2pi(v1);
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v2);
        const Vector v4 = (v0 + v3);
        const Vector v5 = Vector::FusedMultiplyAdd(v0, v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v6 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v5);
        const Vector v7 = (v0 * v6);
        const Vector v8 = (v0 * v7);
        const Vector v9 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v10 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v11 = Math4096::Cos2pi(v10);
        const Vector v12 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v11);
        const Vector v13 = (v9 + v12);
        const Vector v14 = Vector::FusedMultiplyAdd(v9, v13, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v15 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v14);
        const Vector v16 = (v9 * v15);
        const Vector v17 = (v9 * v16);
        const Vector v18 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v20 = Math4096::Cos2pi(v19);
        const Vector v21 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v20);
        const Vector v22 = (v18 + v21);
        const Vector v23 = Vector::FusedMultiplyAdd(v18, v22, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v24 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v23);
        const Vector v25 = (v18 * v24);
        const Vector v26 = (v18 * v25);
        const Vector v27 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v28 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v29 = Math4096::Cos2pi(v28);
        const Vector v30 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v29);
        const Vector v31 = (v27 + v30);
        const Vector v32 = Vector::FusedMultiplyAdd(v27, v31, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v33 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v32);
        const Vector v34 = (v27 * v33);
        const Vector v35 = (v27 * v34);
        const Vector v36 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v37 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v38 = Math4096::Cos2pi(v37);
        const Vector v39 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v38);
        const Vector v40 = (v36 + v39);
        const Vector v41 = Vector::FusedMultiplyAdd(v36, v40, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v42 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v41);
        const Vector v43 = (v36 * v42);
        const Vector v44 = (v36 * v43);
        const Vector v45 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v46 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v47 = Math4096::Cos2pi(v46);
        const Vector v48 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v47);
        const Vector v49 = (v45 + v48);
        const Vector v50 = Vector::FusedMultiplyAdd(v45, v49, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v51 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v50);
        const Vector v52 = (v45 * v51);
        const Vector v53 = (v45 * v52);
        const Vector v54 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v55 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v56 = Math4096::Cos2pi(v55);
        const Vector v57 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v56);
        const Vector v58 = (v54 + v57);
        const Vector v59 = Vector::FusedMultiplyAdd(v54, v58, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v60 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v59);
        const Vector v61 = (v54 * v60);
        const Vector v62 = (v54 * v61);
        const Vector v63 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v64 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v65 = Math4096::Cos2pi(v64);
        const Vector v66 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v65);
        const Vector v67 = (v63 + v66);
        const Vector v68 = Vector::FusedMultiplyAdd(v63, v67, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v69 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v68);
        const Vector v70 = (v63 * v69);
        const Vector v71 = (v63 * v70);
        const Vector v72 = v3;
        const Vector v73 = v6;
        const Vector v74 = v7;
        const Vector v75 = v8;
        const Vector v76 = v12;
        const Vector v77 = v15;
        const Vector v78 = v16;
        const Vector v79 = v17;
        const Vector v80 = v21;
        const Vector v81 = v24;
        const Vector v82 = v25;
        const Vector v83 = v26;
        const Vector v84 = v30;
        const Vector v85 = v33;
        const Vector v86 = v34;
        const Vector v87 = v35;
        const Vector v88 = v39;
        const Vector v89 = v42;
        const Vector v90 = v43;
        const Vector v91 = v44;
        const Vector v92 = v48;
        const Vector v93 = v51;
        const Vector v94 = v52;
        const Vector v95 = v53;
        const Vector v96 = v57;
        const Vector v97 = v60;
        const Vector v98 = v61;
        const Vector v99 = v62;
        const Vector v100 = v66;
        const Vector v101 = v69;
        const Vector v102 = v70;
        const Vector v103 = v71;
        m_state.m_tree.m_first.m_k = v72;
        m_state.m_tree.m_first.m_a1 = v73;
        m_state.m_tree.m_first.m_a2 = v74;
        m_state.m_tree.m_first.m_a3 = v75;
        m_state.m_tree.m_second.m_k = v76;
        m_state.m_tree.m_second.m_a1 = v77;
        m_state.m_tree.m_second.m_a2 = v78;
        m_state.m_tree.m_second.m_a3 = v79;
        m_state.m_tree.m_lowPhase.m_svf.m_k = v80;
        m_state.m_tree.m_lowPhase.m_svf.m_a1 = v81;
        m_state.m_tree.m_lowPhase.m_svf.m_a2 = v82;
        m_state.m_tree.m_lowPhase.m_svf.m_a3 = v83;
        m_state.m_tree.m_highPhase.m_svf.m_k = v84;
        m_state.m_tree.m_highPhase.m_svf.m_a1 = v85;
        m_state.m_tree.m_highPhase.m_svf.m_a2 = v86;
        m_state.m_tree.m_highPhase.m_svf.m_a3 = v87;
        m_state.m_tree.m_low.m_first.m_k = v88;
        m_state.m_tree.m_low.m_first.m_a1 = v89;
        m_state.m_tree.m_low.m_first.m_a2 = v90;
        m_state.m_tree.m_low.m_first.m_a3 = v91;
        m_state.m_tree.m_low.m_second.m_k = v92;
        m_state.m_tree.m_low.m_second.m_a1 = v93;
        m_state.m_tree.m_low.m_second.m_a2 = v94;
        m_state.m_tree.m_low.m_second.m_a3 = v95;
        m_state.m_tree.m_high.m_first.m_k = v96;
        m_state.m_tree.m_high.m_first.m_a1 = v97;
        m_state.m_tree.m_high.m_first.m_a2 = v98;
        m_state.m_tree.m_high.m_first.m_a3 = v99;
        m_state.m_tree.m_high.m_second.m_k = v100;
        m_state.m_tree.m_high.m_second.m_a1 = v101;
        m_state.m_tree.m_high.m_second.m_a2 = v102;
        m_state.m_tree.m_high.m_second.m_a3 = v103;
    }

    // Snapshot generated from DSP.LinkwitzRileySplitter.makeSnapshot4Band.
    //

    struct Snapshot
    {
        struct
        {
            struct
            {
                Vector m_g;
                Vector m_k;
            } m_value;

            struct
            {
                struct
                {
                    Vector m_g;
                    Vector m_k;
                } m_value;

                struct
                {
                } m_low;

                struct
                {
                } m_high;

            } m_low;

            struct
            {
                struct
                {
                    Vector m_g;
                    Vector m_k;
                } m_value;

                struct
                {
                } m_low;

                struct
                {
                } m_high;

            } m_high;

        } m_tree;

    };

    static Snapshot MakeSnapshot(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v2 = Math4096::Cos2pi(v1);
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v2);
        const Vector v4 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v6 = Math4096::Cos2pi(v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v6);
        const Vector v8 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v9 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v10 = Math4096::Cos2pi(v9);
        const Vector v11 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v10);
        Snapshot result{};
        result.m_tree.m_value.m_g = v0;
        result.m_tree.m_value.m_k = v3;
        result.m_tree.m_low.m_value.m_g = v4;
        result.m_tree.m_low.m_value.m_k = v7;
        result.m_tree.m_high.m_value.m_g = v8;
        result.m_tree.m_high.m_value.m_k = v11;
        return result;
    }

};
