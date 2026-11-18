#pragma once

// Generated from DSP.MultibandSaturator.step4Band. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"

#include "ResponseFrequency.hpp"
#include "GeneratedFilterUI.hpp"
#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct MultibandSaturator4BandCoreGeneric
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
        } m_splitter;

        std::array<Vector, 4> m_bandGain;
        Vector m_masterGain;
    };

    struct Input
    {
        Vector m_x;
        bool m_monoBass;
    };

    struct Output
    {
        struct m_bands_Element
        {
            Vector m_before;
            Vector m_after;
        };

        std::array<m_bands_Element, 4> m_bands;
        struct
        {
            Vector m_before;
            Vector m_after;
        } m_master;

        Vector m_sub;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_splitter.m_tree.m_first.m_ic2);
        const Vector v1 = (m_state.m_splitter.m_tree.m_first.m_a1 * m_state.m_splitter.m_tree.m_first.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_first.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_first.m_a2, m_state.m_splitter.m_tree.m_first.m_ic1, m_state.m_splitter.m_tree.m_first.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_first.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_splitter.m_tree.m_first.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_splitter.m_tree.m_first.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_splitter.m_tree.m_first.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (-m_state.m_splitter.m_tree.m_first.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        const Vector v16 = (v4 - m_state.m_splitter.m_tree.m_second.m_ic2);
        const Vector v17 = (m_state.m_splitter.m_tree.m_second.m_a1 * m_state.m_splitter.m_tree.m_second.m_ic1);
        const Vector v18 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_second.m_a2, v16, v17);
        const Vector v19 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_second.m_a2, m_state.m_splitter.m_tree.m_second.m_ic1, m_state.m_splitter.m_tree.m_second.m_ic2);
        const Vector v20 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_second.m_a3, v16, v19);
        const Vector v21 = (-m_state.m_splitter.m_tree.m_second.m_k);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, v18, v4);
        const Vector v23 = (v22 - v20);
        const Vector v24 = (-m_state.m_splitter.m_tree.m_second.m_ic1);
        const Vector v25 = Vector::FusedMultiplyAdd(v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v24);
        const Vector v26 = (-m_state.m_splitter.m_tree.m_second.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        const Vector v28 = (v20 + v23);
        const Vector v29 = (-m_state.m_splitter.m_tree.m_second.m_k);
        const Vector v30 = (v29 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v31 = Vector::FusedMultiplyAdd(v30, v18, v4);
        const Vector v32 = (v20 - m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v33 = (m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a1 * m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic1);
        const Vector v34 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a2, v32, v33);
        const Vector v35 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a2, m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic1, m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v36 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a3, v32, v35);
        const Vector v37 = (-m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_k);
        const Vector v38 = Vector::FusedMultiplyAdd(v37, v34, v20);
        const Vector v39 = (v38 - v36);
        const Vector v40 = (-m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic1);
        const Vector v41 = Vector::FusedMultiplyAdd(v34, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v40);
        const Vector v42 = (-m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v43 = Vector::FusedMultiplyAdd(v36, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v42);
        const Vector v44 = (v36 + v39);
        const Vector v45 = (-m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_k);
        const Vector v46 = (v45 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v47 = Vector::FusedMultiplyAdd(v46, v34, v20);
        const Vector v48 = (v15 - v20);
        const Vector v49 = (v48 - m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v50 = (m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a1 * m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic1);
        const Vector v51 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a2, v49, v50);
        const Vector v52 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a2, m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic1, m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v53 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a3, v49, v52);
        const Vector v54 = (-m_state.m_splitter.m_tree.m_highPhase.m_svf.m_k);
        const Vector v55 = (v15 - v20);
        const Vector v56 = Vector::FusedMultiplyAdd(v54, v51, v55);
        const Vector v57 = (v56 - v53);
        const Vector v58 = (-m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic1);
        const Vector v59 = Vector::FusedMultiplyAdd(v51, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v58);
        const Vector v60 = (-m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v61 = Vector::FusedMultiplyAdd(v53, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v60);
        const Vector v62 = (v53 + v57);
        const Vector v63 = (-m_state.m_splitter.m_tree.m_highPhase.m_svf.m_k);
        const Vector v64 = (v63 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v65 = (v15 - v20);
        const Vector v66 = Vector::FusedMultiplyAdd(v64, v51, v65);
        const Vector v67 = (v47 - m_state.m_splitter.m_tree.m_low.m_first.m_ic2);
        const Vector v68 = (m_state.m_splitter.m_tree.m_low.m_first.m_a1 * m_state.m_splitter.m_tree.m_low.m_first.m_ic1);
        const Vector v69 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_low.m_first.m_a2, v67, v68);
        const Vector v70 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_low.m_first.m_a2, m_state.m_splitter.m_tree.m_low.m_first.m_ic1, m_state.m_splitter.m_tree.m_low.m_first.m_ic2);
        const Vector v71 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_low.m_first.m_a3, v67, v70);
        const Vector v72 = (-m_state.m_splitter.m_tree.m_low.m_first.m_k);
        const Vector v73 = Vector::FusedMultiplyAdd(v72, v69, v47);
        const Vector v74 = (v73 - v71);
        const Vector v75 = (-m_state.m_splitter.m_tree.m_low.m_first.m_ic1);
        const Vector v76 = Vector::FusedMultiplyAdd(v69, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v75);
        const Vector v77 = (-m_state.m_splitter.m_tree.m_low.m_first.m_ic2);
        const Vector v78 = Vector::FusedMultiplyAdd(v71, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v77);
        const Vector v79 = (v71 + v74);
        const Vector v80 = (-m_state.m_splitter.m_tree.m_low.m_first.m_k);
        const Vector v81 = (v80 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v82 = Vector::FusedMultiplyAdd(v81, v69, v47);
        const Vector v83 = (v71 - m_state.m_splitter.m_tree.m_low.m_second.m_ic2);
        const Vector v84 = (m_state.m_splitter.m_tree.m_low.m_second.m_a1 * m_state.m_splitter.m_tree.m_low.m_second.m_ic1);
        const Vector v85 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_low.m_second.m_a2, v83, v84);
        const Vector v86 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_low.m_second.m_a2, m_state.m_splitter.m_tree.m_low.m_second.m_ic1, m_state.m_splitter.m_tree.m_low.m_second.m_ic2);
        const Vector v87 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_low.m_second.m_a3, v83, v86);
        const Vector v88 = (-m_state.m_splitter.m_tree.m_low.m_second.m_k);
        const Vector v89 = Vector::FusedMultiplyAdd(v88, v85, v71);
        const Vector v90 = (v89 - v87);
        const Vector v91 = (-m_state.m_splitter.m_tree.m_low.m_second.m_ic1);
        const Vector v92 = Vector::FusedMultiplyAdd(v85, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v91);
        const Vector v93 = (-m_state.m_splitter.m_tree.m_low.m_second.m_ic2);
        const Vector v94 = Vector::FusedMultiplyAdd(v87, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v93);
        const Vector v95 = (v87 + v90);
        const Vector v96 = (-m_state.m_splitter.m_tree.m_low.m_second.m_k);
        const Vector v97 = (v96 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v98 = Vector::FusedMultiplyAdd(v97, v85, v71);
        const Vector v99 = (v82 - v87);
        const Vector v100 = (v66 - m_state.m_splitter.m_tree.m_high.m_first.m_ic2);
        const Vector v101 = (m_state.m_splitter.m_tree.m_high.m_first.m_a1 * m_state.m_splitter.m_tree.m_high.m_first.m_ic1);
        const Vector v102 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_high.m_first.m_a2, v100, v101);
        const Vector v103 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_high.m_first.m_a2, m_state.m_splitter.m_tree.m_high.m_first.m_ic1, m_state.m_splitter.m_tree.m_high.m_first.m_ic2);
        const Vector v104 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_high.m_first.m_a3, v100, v103);
        const Vector v105 = (-m_state.m_splitter.m_tree.m_high.m_first.m_k);
        const Vector v106 = Vector::FusedMultiplyAdd(v105, v102, v66);
        const Vector v107 = (v106 - v104);
        const Vector v108 = (-m_state.m_splitter.m_tree.m_high.m_first.m_ic1);
        const Vector v109 = Vector::FusedMultiplyAdd(v102, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v108);
        const Vector v110 = (-m_state.m_splitter.m_tree.m_high.m_first.m_ic2);
        const Vector v111 = Vector::FusedMultiplyAdd(v104, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v110);
        const Vector v112 = (v104 + v107);
        const Vector v113 = (-m_state.m_splitter.m_tree.m_high.m_first.m_k);
        const Vector v114 = (v113 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v115 = Vector::FusedMultiplyAdd(v114, v102, v66);
        const Vector v116 = (v104 - m_state.m_splitter.m_tree.m_high.m_second.m_ic2);
        const Vector v117 = (m_state.m_splitter.m_tree.m_high.m_second.m_a1 * m_state.m_splitter.m_tree.m_high.m_second.m_ic1);
        const Vector v118 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_high.m_second.m_a2, v116, v117);
        const Vector v119 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_high.m_second.m_a2, m_state.m_splitter.m_tree.m_high.m_second.m_ic1, m_state.m_splitter.m_tree.m_high.m_second.m_ic2);
        const Vector v120 = Vector::FusedMultiplyAdd(m_state.m_splitter.m_tree.m_high.m_second.m_a3, v116, v119);
        const Vector v121 = (-m_state.m_splitter.m_tree.m_high.m_second.m_k);
        const Vector v122 = Vector::FusedMultiplyAdd(v121, v118, v104);
        const Vector v123 = (v122 - v120);
        const Vector v124 = (-m_state.m_splitter.m_tree.m_high.m_second.m_ic1);
        const Vector v125 = Vector::FusedMultiplyAdd(v118, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v124);
        const Vector v126 = (-m_state.m_splitter.m_tree.m_high.m_second.m_ic2);
        const Vector v127 = Vector::FusedMultiplyAdd(v120, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v126);
        const Vector v128 = (v120 + v123);
        const Vector v129 = (-m_state.m_splitter.m_tree.m_high.m_second.m_k);
        const Vector v130 = (v129 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v131 = Vector::FusedMultiplyAdd(v130, v118, v104);
        const Vector v132 = (v115 - v120);
        const bool v133 = (input.m_monoBass == true);
        Vector v135;
        if (v133)
        {
            const Vector v134 = v87.MeanBroadcast();
            v135 = v134;
        }
        else
        {
            v135 = v87;
        }

        const Vector v136 = (v135 * m_state.m_bandGain[0]);
        const Vector v137 = (v136 * v136);
        const Vector v138 = (v136 * (v137 + Vector::Splat(27)));
        const Vector v139 = Vector::FusedMultiplyAdd(v136 * Vector::Splat(9), v136, Vector::Splat(27));
        const Vector v140 = (v138 / v139);
        const Vector v141 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v140));
        const Vector v142 = (v99 * m_state.m_bandGain[1]);
        const Vector v143 = (v142 * v142);
        const Vector v144 = (v142 * (v143 + Vector::Splat(27)));
        const Vector v145 = Vector::FusedMultiplyAdd(v142 * Vector::Splat(9), v142, Vector::Splat(27));
        const Vector v146 = (v144 / v145);
        const Vector v147 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v146));
        const Vector v148 = (v120 * m_state.m_bandGain[2]);
        const Vector v149 = (v148 * v148);
        const Vector v150 = (v148 * (v149 + Vector::Splat(27)));
        const Vector v151 = Vector::FusedMultiplyAdd(v148 * Vector::Splat(9), v148, Vector::Splat(27));
        const Vector v152 = (v150 / v151);
        const Vector v153 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v152));
        const Vector v154 = (v132 * m_state.m_bandGain[3]);
        const Vector v155 = (v154 * v154);
        const Vector v156 = (v154 * (v155 + Vector::Splat(27)));
        const Vector v157 = Vector::FusedMultiplyAdd(v154 * Vector::Splat(9), v154, Vector::Splat(27));
        const Vector v158 = (v156 / v157);
        const Vector v159 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v158));
        const bool v160 = (input.m_monoBass == true);
        Vector v161;
        if (v160)
        {
            v161 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        }
        else
        {
            v161 = v141;
        }

        const Vector v162 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v161);
        const Vector v163 = (v162 + v147);
        const Vector v164 = (v163 + v153);
        const Vector v165 = (v164 + v159);
        const bool v166 = (input.m_monoBass == true);
        Vector v167;
        if (v166)
        {
            v167 = v141;
        }
        else
        {
            v167 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        }

        const Vector v168 = (v165 + v167);
        const Vector v169 = (v168 * m_state.m_masterGain);
        const Vector v170 = (v169 * v169);
        const Vector v171 = (v169 * (v170 + Vector::Splat(27)));
        const Vector v172 = Vector::FusedMultiplyAdd(v169 * Vector::Splat(9), v169, Vector::Splat(27));
        const Vector v173 = (v171 / v172);
        const Vector v174 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v173));
        const Vector v175 = m_state.m_splitter.m_tree.m_first.m_k;
        const Vector v176 = m_state.m_splitter.m_tree.m_first.m_a1;
        const Vector v177 = m_state.m_splitter.m_tree.m_first.m_a2;
        const Vector v178 = m_state.m_splitter.m_tree.m_first.m_a3;
        const Vector v179 = v9;
        const Vector v180 = v11;
        const Vector v181 = v4;
        const Vector v182 = v2;
        const Vector v183 = v7;
        const Vector v184 = v12;
        const Vector v185 = v15;
        const Vector v186 = m_state.m_splitter.m_tree.m_second.m_k;
        const Vector v187 = m_state.m_splitter.m_tree.m_second.m_a1;
        const Vector v188 = m_state.m_splitter.m_tree.m_second.m_a2;
        const Vector v189 = m_state.m_splitter.m_tree.m_second.m_a3;
        const Vector v190 = v25;
        const Vector v191 = v27;
        const Vector v192 = v20;
        const Vector v193 = v18;
        const Vector v194 = v23;
        const Vector v195 = v28;
        const Vector v196 = v31;
        const Vector v197 = m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_k;
        const Vector v198 = m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a1;
        const Vector v199 = m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a2;
        const Vector v200 = m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a3;
        const Vector v201 = v41;
        const Vector v202 = v43;
        const Vector v203 = v36;
        const Vector v204 = v34;
        const Vector v205 = v39;
        const Vector v206 = v44;
        const Vector v207 = v47;
        const Vector v208 = v47;
        const Vector v209 = v47;
        const Vector v210 = m_state.m_splitter.m_tree.m_highPhase.m_svf.m_k;
        const Vector v211 = m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a1;
        const Vector v212 = m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a2;
        const Vector v213 = m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a3;
        const Vector v214 = v59;
        const Vector v215 = v61;
        const Vector v216 = v53;
        const Vector v217 = v51;
        const Vector v218 = v57;
        const Vector v219 = v62;
        const Vector v220 = v66;
        const Vector v221 = v66;
        const Vector v222 = v66;
        const Vector v223 = m_state.m_splitter.m_tree.m_low.m_first.m_k;
        const Vector v224 = m_state.m_splitter.m_tree.m_low.m_first.m_a1;
        const Vector v225 = m_state.m_splitter.m_tree.m_low.m_first.m_a2;
        const Vector v226 = m_state.m_splitter.m_tree.m_low.m_first.m_a3;
        const Vector v227 = v76;
        const Vector v228 = v78;
        const Vector v229 = v71;
        const Vector v230 = v69;
        const Vector v231 = v74;
        const Vector v232 = v79;
        const Vector v233 = v82;
        const Vector v234 = m_state.m_splitter.m_tree.m_low.m_second.m_k;
        const Vector v235 = m_state.m_splitter.m_tree.m_low.m_second.m_a1;
        const Vector v236 = m_state.m_splitter.m_tree.m_low.m_second.m_a2;
        const Vector v237 = m_state.m_splitter.m_tree.m_low.m_second.m_a3;
        const Vector v238 = v92;
        const Vector v239 = v94;
        const Vector v240 = v87;
        const Vector v241 = v85;
        const Vector v242 = v90;
        const Vector v243 = v95;
        const Vector v244 = v98;
        const Vector v245 = v87;
        const Vector v246 = v99;
        const Vector v247 = v87;
        const Vector v248 = v99;
        const Vector v249 = m_state.m_splitter.m_tree.m_high.m_first.m_k;
        const Vector v250 = m_state.m_splitter.m_tree.m_high.m_first.m_a1;
        const Vector v251 = m_state.m_splitter.m_tree.m_high.m_first.m_a2;
        const Vector v252 = m_state.m_splitter.m_tree.m_high.m_first.m_a3;
        const Vector v253 = v109;
        const Vector v254 = v111;
        const Vector v255 = v104;
        const Vector v256 = v102;
        const Vector v257 = v107;
        const Vector v258 = v112;
        const Vector v259 = v115;
        const Vector v260 = m_state.m_splitter.m_tree.m_high.m_second.m_k;
        const Vector v261 = m_state.m_splitter.m_tree.m_high.m_second.m_a1;
        const Vector v262 = m_state.m_splitter.m_tree.m_high.m_second.m_a2;
        const Vector v263 = m_state.m_splitter.m_tree.m_high.m_second.m_a3;
        const Vector v264 = v125;
        const Vector v265 = v127;
        const Vector v266 = v120;
        const Vector v267 = v118;
        const Vector v268 = v123;
        const Vector v269 = v128;
        const Vector v270 = v131;
        const Vector v271 = v120;
        const Vector v272 = v132;
        const Vector v273 = v120;
        const Vector v274 = v132;
        const Vector v275 = v87;
        const Vector v276 = v99;
        const Vector v277 = v120;
        const Vector v278 = v132;
        const Vector v279 = m_state.m_bandGain[0];
        const Vector v280 = m_state.m_bandGain[1];
        const Vector v281 = m_state.m_bandGain[2];
        const Vector v282 = m_state.m_bandGain[3];
        const Vector v283 = m_state.m_masterGain;
        const Vector v284 = v136;
        const Vector v285 = v141;
        const Vector v286 = v142;
        const Vector v287 = v147;
        const Vector v288 = v148;
        const Vector v289 = v153;
        const Vector v290 = v154;
        const Vector v291 = v159;
        const Vector v292 = v169;
        const Vector v293 = v174;
        const Vector v294 = v167;
        m_state.m_splitter.m_tree.m_first.m_k = v175;
        m_state.m_splitter.m_tree.m_first.m_a1 = v176;
        m_state.m_splitter.m_tree.m_first.m_a2 = v177;
        m_state.m_splitter.m_tree.m_first.m_a3 = v178;
        m_state.m_splitter.m_tree.m_first.m_ic1 = v179;
        m_state.m_splitter.m_tree.m_first.m_ic2 = v180;
        m_state.m_splitter.m_tree.m_first.m_lowPass = v181;
        m_state.m_splitter.m_tree.m_first.m_bandPass = v182;
        m_state.m_splitter.m_tree.m_first.m_highPass = v183;
        m_state.m_splitter.m_tree.m_first.m_notch = v184;
        m_state.m_splitter.m_tree.m_first.m_allPass = v185;
        m_state.m_splitter.m_tree.m_second.m_k = v186;
        m_state.m_splitter.m_tree.m_second.m_a1 = v187;
        m_state.m_splitter.m_tree.m_second.m_a2 = v188;
        m_state.m_splitter.m_tree.m_second.m_a3 = v189;
        m_state.m_splitter.m_tree.m_second.m_ic1 = v190;
        m_state.m_splitter.m_tree.m_second.m_ic2 = v191;
        m_state.m_splitter.m_tree.m_second.m_lowPass = v192;
        m_state.m_splitter.m_tree.m_second.m_bandPass = v193;
        m_state.m_splitter.m_tree.m_second.m_highPass = v194;
        m_state.m_splitter.m_tree.m_second.m_notch = v195;
        m_state.m_splitter.m_tree.m_second.m_allPass = v196;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_k = v197;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a1 = v198;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a2 = v199;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a3 = v200;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic1 = v201;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic2 = v202;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_lowPass = v203;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_bandPass = v204;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_highPass = v205;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_notch = v206;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_allPass = v207;
        m_state.m_splitter.m_tree.m_lowPhase.m_low.m_x = v208;
        m_state.m_splitter.m_tree.m_lowPhase.m_high.m_x = v209;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_k = v210;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a1 = v211;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a2 = v212;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a3 = v213;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic1 = v214;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic2 = v215;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_lowPass = v216;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_bandPass = v217;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_highPass = v218;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_notch = v219;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_allPass = v220;
        m_state.m_splitter.m_tree.m_highPhase.m_low.m_x = v221;
        m_state.m_splitter.m_tree.m_highPhase.m_high.m_x = v222;
        m_state.m_splitter.m_tree.m_low.m_first.m_k = v223;
        m_state.m_splitter.m_tree.m_low.m_first.m_a1 = v224;
        m_state.m_splitter.m_tree.m_low.m_first.m_a2 = v225;
        m_state.m_splitter.m_tree.m_low.m_first.m_a3 = v226;
        m_state.m_splitter.m_tree.m_low.m_first.m_ic1 = v227;
        m_state.m_splitter.m_tree.m_low.m_first.m_ic2 = v228;
        m_state.m_splitter.m_tree.m_low.m_first.m_lowPass = v229;
        m_state.m_splitter.m_tree.m_low.m_first.m_bandPass = v230;
        m_state.m_splitter.m_tree.m_low.m_first.m_highPass = v231;
        m_state.m_splitter.m_tree.m_low.m_first.m_notch = v232;
        m_state.m_splitter.m_tree.m_low.m_first.m_allPass = v233;
        m_state.m_splitter.m_tree.m_low.m_second.m_k = v234;
        m_state.m_splitter.m_tree.m_low.m_second.m_a1 = v235;
        m_state.m_splitter.m_tree.m_low.m_second.m_a2 = v236;
        m_state.m_splitter.m_tree.m_low.m_second.m_a3 = v237;
        m_state.m_splitter.m_tree.m_low.m_second.m_ic1 = v238;
        m_state.m_splitter.m_tree.m_low.m_second.m_ic2 = v239;
        m_state.m_splitter.m_tree.m_low.m_second.m_lowPass = v240;
        m_state.m_splitter.m_tree.m_low.m_second.m_bandPass = v241;
        m_state.m_splitter.m_tree.m_low.m_second.m_highPass = v242;
        m_state.m_splitter.m_tree.m_low.m_second.m_notch = v243;
        m_state.m_splitter.m_tree.m_low.m_second.m_allPass = v244;
        m_state.m_splitter.m_tree.m_low.m_lowPhase.m_x = v245;
        m_state.m_splitter.m_tree.m_low.m_highPhase.m_x = v246;
        m_state.m_splitter.m_tree.m_low.m_low.m_x = v247;
        m_state.m_splitter.m_tree.m_low.m_high.m_x = v248;
        m_state.m_splitter.m_tree.m_high.m_first.m_k = v249;
        m_state.m_splitter.m_tree.m_high.m_first.m_a1 = v250;
        m_state.m_splitter.m_tree.m_high.m_first.m_a2 = v251;
        m_state.m_splitter.m_tree.m_high.m_first.m_a3 = v252;
        m_state.m_splitter.m_tree.m_high.m_first.m_ic1 = v253;
        m_state.m_splitter.m_tree.m_high.m_first.m_ic2 = v254;
        m_state.m_splitter.m_tree.m_high.m_first.m_lowPass = v255;
        m_state.m_splitter.m_tree.m_high.m_first.m_bandPass = v256;
        m_state.m_splitter.m_tree.m_high.m_first.m_highPass = v257;
        m_state.m_splitter.m_tree.m_high.m_first.m_notch = v258;
        m_state.m_splitter.m_tree.m_high.m_first.m_allPass = v259;
        m_state.m_splitter.m_tree.m_high.m_second.m_k = v260;
        m_state.m_splitter.m_tree.m_high.m_second.m_a1 = v261;
        m_state.m_splitter.m_tree.m_high.m_second.m_a2 = v262;
        m_state.m_splitter.m_tree.m_high.m_second.m_a3 = v263;
        m_state.m_splitter.m_tree.m_high.m_second.m_ic1 = v264;
        m_state.m_splitter.m_tree.m_high.m_second.m_ic2 = v265;
        m_state.m_splitter.m_tree.m_high.m_second.m_lowPass = v266;
        m_state.m_splitter.m_tree.m_high.m_second.m_bandPass = v267;
        m_state.m_splitter.m_tree.m_high.m_second.m_highPass = v268;
        m_state.m_splitter.m_tree.m_high.m_second.m_notch = v269;
        m_state.m_splitter.m_tree.m_high.m_second.m_allPass = v270;
        m_state.m_splitter.m_tree.m_high.m_lowPhase.m_x = v271;
        m_state.m_splitter.m_tree.m_high.m_highPhase.m_x = v272;
        m_state.m_splitter.m_tree.m_high.m_low.m_x = v273;
        m_state.m_splitter.m_tree.m_high.m_high.m_x = v274;
        m_state.m_splitter.m_bands[0] = v275;
        m_state.m_splitter.m_bands[1] = v276;
        m_state.m_splitter.m_bands[2] = v277;
        m_state.m_splitter.m_bands[3] = v278;
        m_state.m_bandGain[0] = v279;
        m_state.m_bandGain[1] = v280;
        m_state.m_bandGain[2] = v281;
        m_state.m_bandGain[3] = v282;
        m_state.m_masterGain = v283;
        m_output.m_bands[0].m_before = v284;
        m_output.m_bands[0].m_after = v285;
        m_output.m_bands[1].m_before = v286;
        m_output.m_bands[1].m_after = v287;
        m_output.m_bands[2].m_before = v288;
        m_output.m_bands[2].m_after = v289;
        m_output.m_bands[3].m_before = v290;
        m_output.m_bands[3].m_after = v291;
        m_output.m_master.m_before = v292;
        m_output.m_master.m_after = v293;
        m_output.m_sub = v294;
    }

    template<size_t Width>
    struct Preparation
    {
        using Vector = MultiChannelNumber<Number, Width>;

        struct Prepared
        {
            struct
            {
                Vector m_g;
                Vector m_k;
            } m_snapshot;

            Vector m_a1;
            Vector m_a2;
            Vector m_a3;
        };

        static Prepared Prepare(const Vector& input)
        {
            const Vector v0 = Math4096::TanPi(input);
            const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
            const Vector v2 = Math4096::Cos2pi(v1);
            const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v2);
            const Vector v4 = (v0 + v3);
            const Vector v5 = Vector::FusedMultiplyAdd(v0, v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
            const Vector v6 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v5);
            const Vector v7 = (v0 * v6);
            const Vector v8 = (v0 * v7);
            Prepared result{};
            result.m_snapshot.m_g = v0;
            result.m_snapshot.m_k = v3;
            result.m_a1 = v6;
            result.m_a2 = v7;
            result.m_a3 = v8;
            return result;
        }

    };

    // Response generated from DSP.MultibandSaturator.magnitudeSquared4Band.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        struct
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

        } m_splitter;

        std::array<Vector, 4> m_bandGain;
        Vector m_masterGain;
    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_splitter.m_tree.m_value.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v1 * v1);
        const Vector v4 = (v2 * v2);
        const Vector v5 = (v3 + v4);
        const Vector v6 = (v3 / v5);
        const Vector v9 = (snapshot.m_splitter.m_tree.m_low.m_value.m_g * input.m_cosHalf);
        const Vector v10 = (v9 * v9);
        const Vector v11 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v12 = (v10 * v10);
        const Vector v13 = (v11 * v11);
        const Vector v14 = (v12 + v13);
        const Vector v15 = (v12 / v14);
        const Vector v18 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v15);
        const Vector v19 = (v18 * v6);
        const Vector v20 = (snapshot.m_bandGain[0] * v19);
        const Vector v21 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) + v20);
        const Vector v22 = (snapshot.m_splitter.m_tree.m_value.m_g * input.m_cosHalf);
        const Vector v23 = (v22 * v22);
        const Vector v24 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v25 = (v23 * v23);
        const Vector v26 = (v24 * v24);
        const Vector v27 = (v25 + v26);
        const Vector v28 = (v25 / v27);
        const Vector v31 = (snapshot.m_splitter.m_tree.m_low.m_value.m_g * input.m_cosHalf);
        const Vector v32 = (v31 * v31);
        const Vector v33 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v34 = (v32 * v32);
        const Vector v35 = (v33 * v33);
        const Vector v38 = (v34 + v35);
        const Vector v39 = (v35 / v38);
        const Vector v40 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v39);
        const Vector v41 = (v40 * v28);
        const Vector v42 = (snapshot.m_bandGain[1] * v41);
        const Vector v43 = (v21 + v42);
        const Vector v44 = (snapshot.m_splitter.m_tree.m_value.m_g * input.m_cosHalf);
        const Vector v45 = (v44 * v44);
        const Vector v46 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v47 = (v45 * v45);
        const Vector v48 = (v46 * v46);
        const Vector v51 = (v47 + v48);
        const Vector v52 = (v48 / v51);
        const Vector v53 = (snapshot.m_splitter.m_tree.m_high.m_value.m_g * input.m_cosHalf);
        const Vector v54 = (v53 * v53);
        const Vector v55 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v56 = (v54 * v54);
        const Vector v57 = (v55 * v55);
        const Vector v58 = (v56 + v57);
        const Vector v59 = (v56 / v58);
        const Vector v62 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v59);
        const Vector v63 = (v62 * v52);
        const Vector v64 = (snapshot.m_bandGain[2] * v63);
        const Vector v65 = (v43 + v64);
        const Vector v66 = (snapshot.m_splitter.m_tree.m_value.m_g * input.m_cosHalf);
        const Vector v67 = (v66 * v66);
        const Vector v68 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v69 = (v67 * v67);
        const Vector v70 = (v68 * v68);
        const Vector v73 = (v69 + v70);
        const Vector v74 = (v70 / v73);
        const Vector v75 = (snapshot.m_splitter.m_tree.m_high.m_value.m_g * input.m_cosHalf);
        const Vector v76 = (v75 * v75);
        const Vector v77 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v78 = (v76 * v76);
        const Vector v79 = (v77 * v77);
        const Vector v82 = (v78 + v79);
        const Vector v83 = (v79 / v82);
        const Vector v84 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v83);
        const Vector v85 = (v84 * v74);
        const Vector v86 = (snapshot.m_bandGain[3] * v85);
        const Vector v87 = (v65 + v86);
        const Vector v88 = (snapshot.m_masterGain * v87);
        const Vector v89 = (v88 * v88);
        return v89;
    }

    // Coefficients generated from DSP.MultibandSaturator.prepare4Band.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        struct
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

        } m_splitter;

        std::array<Vector, 4> m_bandGain;
        Vector m_masterGain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_splitter.m_tree.m_value);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v2 = Math4096::Cos2pi(v1);
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v2);
        const Vector v4 = (v0 + v3);
        const Vector v5 = Vector::FusedMultiplyAdd(v0, v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v6 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v5);
        const Vector v7 = (v0 * v6);
        const Vector v8 = (v0 * v7);
        const Vector v9 = Math4096::TanPi(spec.m_splitter.m_tree.m_low.m_value);
        const Vector v10 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v11 = Math4096::Cos2pi(v10);
        const Vector v12 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v11);
        const Vector v13 = (v9 + v12);
        const Vector v14 = Vector::FusedMultiplyAdd(v9, v13, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v15 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v14);
        const Vector v16 = (v9 * v15);
        const Vector v17 = (v9 * v16);
        const Vector v18 = Math4096::TanPi(spec.m_splitter.m_tree.m_high.m_value);
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v20 = Math4096::Cos2pi(v19);
        const Vector v21 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v20);
        const Vector v22 = (v18 + v21);
        const Vector v23 = Vector::FusedMultiplyAdd(v18, v22, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v24 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v23);
        const Vector v25 = (v18 * v24);
        const Vector v26 = (v18 * v25);
        const Vector v27 = v0;
        const Vector v28 = v3;
        const Vector v29 = v9;
        const Vector v30 = v12;
        const Vector v31 = v18;
        const Vector v32 = v21;
        const Vector v33 = spec.m_bandGain[0];
        const Vector v34 = spec.m_bandGain[1];
        const Vector v35 = spec.m_bandGain[2];
        const Vector v36 = spec.m_bandGain[3];
        const Vector v37 = spec.m_masterGain;
        const Vector v38 = v3;
        const Vector v39 = v6;
        const Vector v40 = v7;
        const Vector v41 = v8;
        const Vector v42 = v3;
        const Vector v43 = v6;
        const Vector v44 = v7;
        const Vector v45 = v8;
        const Vector v46 = v21;
        const Vector v47 = v24;
        const Vector v48 = v25;
        const Vector v49 = v26;
        const Vector v50 = v12;
        const Vector v51 = v15;
        const Vector v52 = v16;
        const Vector v53 = v17;
        const Vector v54 = v12;
        const Vector v55 = v15;
        const Vector v56 = v16;
        const Vector v57 = v17;
        const Vector v58 = v12;
        const Vector v59 = v15;
        const Vector v60 = v16;
        const Vector v61 = v17;
        const Vector v62 = v21;
        const Vector v63 = v24;
        const Vector v64 = v25;
        const Vector v65 = v26;
        const Vector v66 = v21;
        const Vector v67 = v24;
        const Vector v68 = v25;
        const Vector v69 = v26;
        const Vector v70 = spec.m_bandGain[0];
        const Vector v71 = spec.m_bandGain[1];
        const Vector v72 = spec.m_bandGain[2];
        const Vector v73 = spec.m_bandGain[3];
        const Vector v74 = spec.m_masterGain;
        m_state.m_splitter.m_tree.m_first.m_k = v38;
        m_state.m_splitter.m_tree.m_first.m_a1 = v39;
        m_state.m_splitter.m_tree.m_first.m_a2 = v40;
        m_state.m_splitter.m_tree.m_first.m_a3 = v41;
        m_state.m_splitter.m_tree.m_second.m_k = v42;
        m_state.m_splitter.m_tree.m_second.m_a1 = v43;
        m_state.m_splitter.m_tree.m_second.m_a2 = v44;
        m_state.m_splitter.m_tree.m_second.m_a3 = v45;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_k = v46;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a1 = v47;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a2 = v48;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a3 = v49;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_k = v50;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a1 = v51;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a2 = v52;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a3 = v53;
        m_state.m_splitter.m_tree.m_low.m_first.m_k = v54;
        m_state.m_splitter.m_tree.m_low.m_first.m_a1 = v55;
        m_state.m_splitter.m_tree.m_low.m_first.m_a2 = v56;
        m_state.m_splitter.m_tree.m_low.m_first.m_a3 = v57;
        m_state.m_splitter.m_tree.m_low.m_second.m_k = v58;
        m_state.m_splitter.m_tree.m_low.m_second.m_a1 = v59;
        m_state.m_splitter.m_tree.m_low.m_second.m_a2 = v60;
        m_state.m_splitter.m_tree.m_low.m_second.m_a3 = v61;
        m_state.m_splitter.m_tree.m_high.m_first.m_k = v62;
        m_state.m_splitter.m_tree.m_high.m_first.m_a1 = v63;
        m_state.m_splitter.m_tree.m_high.m_first.m_a2 = v64;
        m_state.m_splitter.m_tree.m_high.m_first.m_a3 = v65;
        m_state.m_splitter.m_tree.m_high.m_second.m_k = v66;
        m_state.m_splitter.m_tree.m_high.m_second.m_a1 = v67;
        m_state.m_splitter.m_tree.m_high.m_second.m_a2 = v68;
        m_state.m_splitter.m_tree.m_high.m_second.m_a3 = v69;
        m_state.m_bandGain[0] = v70;
        m_state.m_bandGain[1] = v71;
        m_state.m_bandGain[2] = v72;
        m_state.m_bandGain[3] = v73;
        m_state.m_masterGain = v74;
        m_responseSnapshot.m_splitter.m_tree.m_value.m_g = v27;
        m_responseSnapshot.m_splitter.m_tree.m_value.m_k = v28;
        m_responseSnapshot.m_splitter.m_tree.m_low.m_value.m_g = v29;
        m_responseSnapshot.m_splitter.m_tree.m_low.m_value.m_k = v30;
        m_responseSnapshot.m_splitter.m_tree.m_high.m_value.m_g = v31;
        m_responseSnapshot.m_splitter.m_tree.m_high.m_value.m_k = v32;
        m_responseSnapshot.m_bandGain[0] = v33;
        m_responseSnapshot.m_bandGain[1] = v34;
        m_responseSnapshot.m_bandGain[2] = v35;
        m_responseSnapshot.m_bandGain[3] = v36;
        m_responseSnapshot.m_masterGain = v37;
    }

    struct PreparedSpec
    {
        struct
        {
            struct
            {
                struct
                {
                    struct
                    {
                        Vector m_g;
                        Vector m_k;
                    } m_snapshot;

                    Vector m_a1;
                    Vector m_a2;
                    Vector m_a3;
                } m_value;

                struct
                {
                    struct
                    {
                        struct
                        {
                            Vector m_g;
                            Vector m_k;
                        } m_snapshot;

                        Vector m_a1;
                        Vector m_a2;
                        Vector m_a3;
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
                        struct
                        {
                            Vector m_g;
                            Vector m_k;
                        } m_snapshot;

                        Vector m_a1;
                        Vector m_a2;
                        Vector m_a3;
                    } m_value;

                    struct
                    {
                    } m_low;

                    struct
                    {
                    } m_high;

                } m_high;

            } m_tree;

        } m_splitter;

        std::array<Vector, 4> m_bandGain;
        Vector m_masterGain;
    };

    void InstallPrepared(const PreparedSpec& spec)
    {
        const Vector v0 = spec.m_splitter.m_tree.m_value.m_snapshot.m_g;
        const Vector v1 = spec.m_splitter.m_tree.m_value.m_snapshot.m_k;
        const Vector v2 = spec.m_splitter.m_tree.m_low.m_value.m_snapshot.m_g;
        const Vector v3 = spec.m_splitter.m_tree.m_low.m_value.m_snapshot.m_k;
        const Vector v4 = spec.m_splitter.m_tree.m_high.m_value.m_snapshot.m_g;
        const Vector v5 = spec.m_splitter.m_tree.m_high.m_value.m_snapshot.m_k;
        const Vector v6 = spec.m_bandGain[0];
        const Vector v7 = spec.m_bandGain[1];
        const Vector v8 = spec.m_bandGain[2];
        const Vector v9 = spec.m_bandGain[3];
        const Vector v10 = spec.m_masterGain;
        const Vector v11 = spec.m_splitter.m_tree.m_value.m_snapshot.m_k;
        const Vector v12 = spec.m_splitter.m_tree.m_value.m_a1;
        const Vector v13 = spec.m_splitter.m_tree.m_value.m_a2;
        const Vector v14 = spec.m_splitter.m_tree.m_value.m_a3;
        const Vector v15 = spec.m_splitter.m_tree.m_value.m_snapshot.m_k;
        const Vector v16 = spec.m_splitter.m_tree.m_value.m_a1;
        const Vector v17 = spec.m_splitter.m_tree.m_value.m_a2;
        const Vector v18 = spec.m_splitter.m_tree.m_value.m_a3;
        const Vector v19 = spec.m_splitter.m_tree.m_high.m_value.m_snapshot.m_k;
        const Vector v20 = spec.m_splitter.m_tree.m_high.m_value.m_a1;
        const Vector v21 = spec.m_splitter.m_tree.m_high.m_value.m_a2;
        const Vector v22 = spec.m_splitter.m_tree.m_high.m_value.m_a3;
        const Vector v23 = spec.m_splitter.m_tree.m_low.m_value.m_snapshot.m_k;
        const Vector v24 = spec.m_splitter.m_tree.m_low.m_value.m_a1;
        const Vector v25 = spec.m_splitter.m_tree.m_low.m_value.m_a2;
        const Vector v26 = spec.m_splitter.m_tree.m_low.m_value.m_a3;
        const Vector v27 = spec.m_splitter.m_tree.m_low.m_value.m_snapshot.m_k;
        const Vector v28 = spec.m_splitter.m_tree.m_low.m_value.m_a1;
        const Vector v29 = spec.m_splitter.m_tree.m_low.m_value.m_a2;
        const Vector v30 = spec.m_splitter.m_tree.m_low.m_value.m_a3;
        const Vector v31 = spec.m_splitter.m_tree.m_low.m_value.m_snapshot.m_k;
        const Vector v32 = spec.m_splitter.m_tree.m_low.m_value.m_a1;
        const Vector v33 = spec.m_splitter.m_tree.m_low.m_value.m_a2;
        const Vector v34 = spec.m_splitter.m_tree.m_low.m_value.m_a3;
        const Vector v35 = spec.m_splitter.m_tree.m_high.m_value.m_snapshot.m_k;
        const Vector v36 = spec.m_splitter.m_tree.m_high.m_value.m_a1;
        const Vector v37 = spec.m_splitter.m_tree.m_high.m_value.m_a2;
        const Vector v38 = spec.m_splitter.m_tree.m_high.m_value.m_a3;
        const Vector v39 = spec.m_splitter.m_tree.m_high.m_value.m_snapshot.m_k;
        const Vector v40 = spec.m_splitter.m_tree.m_high.m_value.m_a1;
        const Vector v41 = spec.m_splitter.m_tree.m_high.m_value.m_a2;
        const Vector v42 = spec.m_splitter.m_tree.m_high.m_value.m_a3;
        const Vector v43 = spec.m_bandGain[0];
        const Vector v44 = spec.m_bandGain[1];
        const Vector v45 = spec.m_bandGain[2];
        const Vector v46 = spec.m_bandGain[3];
        const Vector v47 = spec.m_masterGain;
        m_state.m_splitter.m_tree.m_first.m_k = v11;
        m_state.m_splitter.m_tree.m_first.m_a1 = v12;
        m_state.m_splitter.m_tree.m_first.m_a2 = v13;
        m_state.m_splitter.m_tree.m_first.m_a3 = v14;
        m_state.m_splitter.m_tree.m_second.m_k = v15;
        m_state.m_splitter.m_tree.m_second.m_a1 = v16;
        m_state.m_splitter.m_tree.m_second.m_a2 = v17;
        m_state.m_splitter.m_tree.m_second.m_a3 = v18;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_k = v19;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a1 = v20;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a2 = v21;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_a3 = v22;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_k = v23;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a1 = v24;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a2 = v25;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_a3 = v26;
        m_state.m_splitter.m_tree.m_low.m_first.m_k = v27;
        m_state.m_splitter.m_tree.m_low.m_first.m_a1 = v28;
        m_state.m_splitter.m_tree.m_low.m_first.m_a2 = v29;
        m_state.m_splitter.m_tree.m_low.m_first.m_a3 = v30;
        m_state.m_splitter.m_tree.m_low.m_second.m_k = v31;
        m_state.m_splitter.m_tree.m_low.m_second.m_a1 = v32;
        m_state.m_splitter.m_tree.m_low.m_second.m_a2 = v33;
        m_state.m_splitter.m_tree.m_low.m_second.m_a3 = v34;
        m_state.m_splitter.m_tree.m_high.m_first.m_k = v35;
        m_state.m_splitter.m_tree.m_high.m_first.m_a1 = v36;
        m_state.m_splitter.m_tree.m_high.m_first.m_a2 = v37;
        m_state.m_splitter.m_tree.m_high.m_first.m_a3 = v38;
        m_state.m_splitter.m_tree.m_high.m_second.m_k = v39;
        m_state.m_splitter.m_tree.m_high.m_second.m_a1 = v40;
        m_state.m_splitter.m_tree.m_high.m_second.m_a2 = v41;
        m_state.m_splitter.m_tree.m_high.m_second.m_a3 = v42;
        m_state.m_bandGain[0] = v43;
        m_state.m_bandGain[1] = v44;
        m_state.m_bandGain[2] = v45;
        m_state.m_bandGain[3] = v46;
        m_state.m_masterGain = v47;
        m_responseSnapshot.m_splitter.m_tree.m_value.m_g = v0;
        m_responseSnapshot.m_splitter.m_tree.m_value.m_k = v1;
        m_responseSnapshot.m_splitter.m_tree.m_low.m_value.m_g = v2;
        m_responseSnapshot.m_splitter.m_tree.m_low.m_value.m_k = v3;
        m_responseSnapshot.m_splitter.m_tree.m_high.m_value.m_g = v4;
        m_responseSnapshot.m_splitter.m_tree.m_high.m_value.m_k = v5;
        m_responseSnapshot.m_bandGain[0] = v6;
        m_responseSnapshot.m_bandGain[1] = v7;
        m_responseSnapshot.m_bandGain[2] = v8;
        m_responseSnapshot.m_bandGain[3] = v9;
        m_responseSnapshot.m_masterGain = v10;
    }

    // Reset generated from DSP.MultibandSaturator.reset4Band.
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
        const Vector v37 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v38 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v39 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v40 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v41 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v42 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v43 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v44 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v45 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v46 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v47 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v48 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v49 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v50 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v51 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v52 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v53 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v54 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v55 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v56 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v57 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v58 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v59 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v60 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v61 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v62 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v63 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v64 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v65 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v66 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v67 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v68 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v69 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v70 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v71 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        m_state.m_splitter.m_tree.m_first.m_ic1 = v0;
        m_state.m_splitter.m_tree.m_first.m_ic2 = v1;
        m_state.m_splitter.m_tree.m_first.m_lowPass = v2;
        m_state.m_splitter.m_tree.m_first.m_bandPass = v3;
        m_state.m_splitter.m_tree.m_first.m_highPass = v4;
        m_state.m_splitter.m_tree.m_first.m_notch = v5;
        m_state.m_splitter.m_tree.m_first.m_allPass = v6;
        m_state.m_splitter.m_tree.m_second.m_ic1 = v7;
        m_state.m_splitter.m_tree.m_second.m_ic2 = v8;
        m_state.m_splitter.m_tree.m_second.m_lowPass = v9;
        m_state.m_splitter.m_tree.m_second.m_bandPass = v10;
        m_state.m_splitter.m_tree.m_second.m_highPass = v11;
        m_state.m_splitter.m_tree.m_second.m_notch = v12;
        m_state.m_splitter.m_tree.m_second.m_allPass = v13;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic1 = v14;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_ic2 = v15;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_lowPass = v16;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_bandPass = v17;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_highPass = v18;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_notch = v19;
        m_state.m_splitter.m_tree.m_lowPhase.m_svf.m_allPass = v20;
        m_state.m_splitter.m_tree.m_lowPhase.m_low.m_x = v21;
        m_state.m_splitter.m_tree.m_lowPhase.m_high.m_x = v22;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic1 = v23;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_ic2 = v24;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_lowPass = v25;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_bandPass = v26;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_highPass = v27;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_notch = v28;
        m_state.m_splitter.m_tree.m_highPhase.m_svf.m_allPass = v29;
        m_state.m_splitter.m_tree.m_highPhase.m_low.m_x = v30;
        m_state.m_splitter.m_tree.m_highPhase.m_high.m_x = v31;
        m_state.m_splitter.m_tree.m_low.m_first.m_ic1 = v32;
        m_state.m_splitter.m_tree.m_low.m_first.m_ic2 = v33;
        m_state.m_splitter.m_tree.m_low.m_first.m_lowPass = v34;
        m_state.m_splitter.m_tree.m_low.m_first.m_bandPass = v35;
        m_state.m_splitter.m_tree.m_low.m_first.m_highPass = v36;
        m_state.m_splitter.m_tree.m_low.m_first.m_notch = v37;
        m_state.m_splitter.m_tree.m_low.m_first.m_allPass = v38;
        m_state.m_splitter.m_tree.m_low.m_second.m_ic1 = v39;
        m_state.m_splitter.m_tree.m_low.m_second.m_ic2 = v40;
        m_state.m_splitter.m_tree.m_low.m_second.m_lowPass = v41;
        m_state.m_splitter.m_tree.m_low.m_second.m_bandPass = v42;
        m_state.m_splitter.m_tree.m_low.m_second.m_highPass = v43;
        m_state.m_splitter.m_tree.m_low.m_second.m_notch = v44;
        m_state.m_splitter.m_tree.m_low.m_second.m_allPass = v45;
        m_state.m_splitter.m_tree.m_low.m_lowPhase.m_x = v46;
        m_state.m_splitter.m_tree.m_low.m_highPhase.m_x = v47;
        m_state.m_splitter.m_tree.m_low.m_low.m_x = v48;
        m_state.m_splitter.m_tree.m_low.m_high.m_x = v49;
        m_state.m_splitter.m_tree.m_high.m_first.m_ic1 = v50;
        m_state.m_splitter.m_tree.m_high.m_first.m_ic2 = v51;
        m_state.m_splitter.m_tree.m_high.m_first.m_lowPass = v52;
        m_state.m_splitter.m_tree.m_high.m_first.m_bandPass = v53;
        m_state.m_splitter.m_tree.m_high.m_first.m_highPass = v54;
        m_state.m_splitter.m_tree.m_high.m_first.m_notch = v55;
        m_state.m_splitter.m_tree.m_high.m_first.m_allPass = v56;
        m_state.m_splitter.m_tree.m_high.m_second.m_ic1 = v57;
        m_state.m_splitter.m_tree.m_high.m_second.m_ic2 = v58;
        m_state.m_splitter.m_tree.m_high.m_second.m_lowPass = v59;
        m_state.m_splitter.m_tree.m_high.m_second.m_bandPass = v60;
        m_state.m_splitter.m_tree.m_high.m_second.m_highPass = v61;
        m_state.m_splitter.m_tree.m_high.m_second.m_notch = v62;
        m_state.m_splitter.m_tree.m_high.m_second.m_allPass = v63;
        m_state.m_splitter.m_tree.m_high.m_lowPhase.m_x = v64;
        m_state.m_splitter.m_tree.m_high.m_highPhase.m_x = v65;
        m_state.m_splitter.m_tree.m_high.m_low.m_x = v66;
        m_state.m_splitter.m_tree.m_high.m_high.m_x = v67;
        m_state.m_splitter.m_bands[0] = v68;
        m_state.m_splitter.m_bands[1] = v69;
        m_state.m_splitter.m_bands[2] = v70;
        m_state.m_splitter.m_bands[3] = v71;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<MultibandSaturator4BandCoreGeneric<Number, Size>, &MultibandSaturator4BandCoreGeneric<Number, Size>::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
