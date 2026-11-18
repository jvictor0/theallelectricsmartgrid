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
        m_state.m_tree.m_first.m_ic1 = v9;
        const Vector v10 = (-m_state.m_tree.m_first.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        m_state.m_tree.m_first.m_ic2 = v11;
        const Vector v12 = (v4 + v7);
        m_state.m_tree.m_first.m_highPass = v7;
        m_state.m_tree.m_first.m_notch = v12;
        const Vector v13 = (-m_state.m_tree.m_first.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        m_state.m_tree.m_first.m_bandPass = v2;
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
        m_state.m_tree.m_second.m_ic1 = v25;
        const Vector v26 = (-m_state.m_tree.m_second.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        m_state.m_tree.m_second.m_ic2 = v27;
        const Vector v28 = (v20 + v23);
        m_state.m_tree.m_second.m_highPass = v23;
        m_state.m_tree.m_second.m_notch = v28;
        const Vector v29 = (-m_state.m_tree.m_second.m_k);
        const Vector v30 = (v29 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v31 = Vector::FusedMultiplyAdd(v30, v18, v4);
        m_state.m_tree.m_first.m_lowPass = v4;
        m_state.m_tree.m_second.m_bandPass = v18;
        m_state.m_tree.m_second.m_allPass = v31;
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
        m_state.m_tree.m_lowPhase.m_svf.m_ic1 = v41;
        const Vector v42 = (-m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v43 = Vector::FusedMultiplyAdd(v36, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v42);
        m_state.m_tree.m_lowPhase.m_svf.m_ic2 = v43;
        const Vector v44 = (v36 + v39);
        m_state.m_tree.m_lowPhase.m_svf.m_lowPass = v36;
        m_state.m_tree.m_lowPhase.m_svf.m_highPass = v39;
        m_state.m_tree.m_lowPhase.m_svf.m_notch = v44;
        const Vector v45 = (-m_state.m_tree.m_lowPhase.m_svf.m_k);
        const Vector v46 = (v45 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v47 = Vector::FusedMultiplyAdd(v46, v34, v20);
        m_state.m_tree.m_lowPhase.m_svf.m_bandPass = v34;
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
        m_state.m_tree.m_highPhase.m_svf.m_ic1 = v59;
        const Vector v60 = (-m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v61 = Vector::FusedMultiplyAdd(v53, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v60);
        m_state.m_tree.m_highPhase.m_svf.m_ic2 = v61;
        const Vector v62 = (v53 + v57);
        m_state.m_tree.m_highPhase.m_svf.m_lowPass = v53;
        m_state.m_tree.m_highPhase.m_svf.m_highPass = v57;
        m_state.m_tree.m_highPhase.m_svf.m_notch = v62;
        const Vector v63 = (-m_state.m_tree.m_highPhase.m_svf.m_k);
        const Vector v64 = (v63 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v65 = (v15 - v20);
        m_state.m_tree.m_first.m_allPass = v15;
        m_state.m_tree.m_second.m_lowPass = v20;
        const Vector v66 = Vector::FusedMultiplyAdd(v64, v51, v65);
        m_state.m_tree.m_highPhase.m_svf.m_bandPass = v51;
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
        m_state.m_tree.m_low.m_first.m_ic1 = v76;
        const Vector v77 = (-m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v78 = Vector::FusedMultiplyAdd(v71, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v77);
        m_state.m_tree.m_low.m_first.m_ic2 = v78;
        const Vector v79 = (v71 + v74);
        m_state.m_tree.m_low.m_first.m_highPass = v74;
        m_state.m_tree.m_low.m_first.m_notch = v79;
        const Vector v80 = (-m_state.m_tree.m_low.m_first.m_k);
        const Vector v81 = (v80 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v82 = Vector::FusedMultiplyAdd(v81, v69, v47);
        m_state.m_tree.m_lowPhase.m_svf.m_allPass = v47;
        m_state.m_tree.m_lowPhase.m_low.m_x = v47;
        m_state.m_tree.m_lowPhase.m_high.m_x = v47;
        m_state.m_tree.m_low.m_first.m_bandPass = v69;
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
        m_state.m_tree.m_low.m_second.m_ic1 = v92;
        const Vector v93 = (-m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v94 = Vector::FusedMultiplyAdd(v87, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v93);
        m_state.m_tree.m_low.m_second.m_ic2 = v94;
        const Vector v95 = (v87 + v90);
        m_state.m_tree.m_low.m_second.m_highPass = v90;
        m_state.m_tree.m_low.m_second.m_notch = v95;
        const Vector v96 = (-m_state.m_tree.m_low.m_second.m_k);
        const Vector v97 = (v96 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v98 = Vector::FusedMultiplyAdd(v97, v85, v71);
        m_state.m_tree.m_low.m_first.m_lowPass = v71;
        m_state.m_tree.m_low.m_second.m_bandPass = v85;
        m_state.m_tree.m_low.m_second.m_allPass = v98;
        const Vector v99 = (v82 - v87);
        m_state.m_tree.m_low.m_first.m_allPass = v82;
        m_state.m_tree.m_low.m_second.m_lowPass = v87;
        m_state.m_tree.m_low.m_lowPhase.m_x = v87;
        m_state.m_tree.m_low.m_highPhase.m_x = v99;
        m_state.m_tree.m_low.m_low.m_x = v87;
        m_state.m_tree.m_low.m_high.m_x = v99;
        m_state.m_bands[0] = v87;
        m_state.m_bands[1] = v99;
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
        m_state.m_tree.m_high.m_first.m_ic1 = v109;
        const Vector v110 = (-m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v111 = Vector::FusedMultiplyAdd(v104, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v110);
        m_state.m_tree.m_high.m_first.m_ic2 = v111;
        const Vector v112 = (v104 + v107);
        m_state.m_tree.m_high.m_first.m_highPass = v107;
        m_state.m_tree.m_high.m_first.m_notch = v112;
        const Vector v113 = (-m_state.m_tree.m_high.m_first.m_k);
        const Vector v114 = (v113 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v115 = Vector::FusedMultiplyAdd(v114, v102, v66);
        m_state.m_tree.m_highPhase.m_svf.m_allPass = v66;
        m_state.m_tree.m_highPhase.m_low.m_x = v66;
        m_state.m_tree.m_highPhase.m_high.m_x = v66;
        m_state.m_tree.m_high.m_first.m_bandPass = v102;
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
        m_state.m_tree.m_high.m_second.m_ic1 = v125;
        const Vector v126 = (-m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v127 = Vector::FusedMultiplyAdd(v120, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v126);
        m_state.m_tree.m_high.m_second.m_ic2 = v127;
        const Vector v128 = (v120 + v123);
        m_state.m_tree.m_high.m_second.m_highPass = v123;
        m_state.m_tree.m_high.m_second.m_notch = v128;
        const Vector v129 = (-m_state.m_tree.m_high.m_second.m_k);
        const Vector v130 = (v129 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v131 = Vector::FusedMultiplyAdd(v130, v118, v104);
        m_state.m_tree.m_high.m_first.m_lowPass = v104;
        m_state.m_tree.m_high.m_second.m_bandPass = v118;
        m_state.m_tree.m_high.m_second.m_allPass = v131;
        const Vector v132 = (v115 - v120);
        m_state.m_tree.m_high.m_first.m_allPass = v115;
        m_state.m_tree.m_high.m_second.m_lowPass = v120;
        m_state.m_tree.m_high.m_lowPhase.m_x = v120;
        m_state.m_tree.m_high.m_highPhase.m_x = v132;
        m_state.m_tree.m_high.m_low.m_x = v120;
        m_state.m_tree.m_high.m_high.m_x = v132;
        m_state.m_bands[2] = v120;
        m_state.m_bands[3] = v132;
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
        m_state.m_tree.m_first.m_k = v3;
        m_state.m_tree.m_first.m_a1 = v6;
        m_state.m_tree.m_first.m_a2 = v7;
        m_state.m_tree.m_first.m_a3 = v8;
        m_state.m_tree.m_second.m_k = v12;
        m_state.m_tree.m_second.m_a1 = v15;
        m_state.m_tree.m_second.m_a2 = v16;
        m_state.m_tree.m_second.m_a3 = v17;
        m_state.m_tree.m_lowPhase.m_svf.m_k = v21;
        m_state.m_tree.m_lowPhase.m_svf.m_a1 = v24;
        m_state.m_tree.m_lowPhase.m_svf.m_a2 = v25;
        m_state.m_tree.m_lowPhase.m_svf.m_a3 = v26;
        m_state.m_tree.m_highPhase.m_svf.m_k = v30;
        m_state.m_tree.m_highPhase.m_svf.m_a1 = v33;
        m_state.m_tree.m_highPhase.m_svf.m_a2 = v34;
        m_state.m_tree.m_highPhase.m_svf.m_a3 = v35;
        m_state.m_tree.m_low.m_first.m_k = v39;
        m_state.m_tree.m_low.m_first.m_a1 = v42;
        m_state.m_tree.m_low.m_first.m_a2 = v43;
        m_state.m_tree.m_low.m_first.m_a3 = v44;
        m_state.m_tree.m_low.m_second.m_k = v48;
        m_state.m_tree.m_low.m_second.m_a1 = v51;
        m_state.m_tree.m_low.m_second.m_a2 = v52;
        m_state.m_tree.m_low.m_second.m_a3 = v53;
        m_state.m_tree.m_high.m_first.m_k = v57;
        m_state.m_tree.m_high.m_first.m_a1 = v60;
        m_state.m_tree.m_high.m_first.m_a2 = v61;
        m_state.m_tree.m_high.m_first.m_a3 = v62;
        m_state.m_tree.m_high.m_second.m_k = v66;
        m_state.m_tree.m_high.m_second.m_a1 = v69;
        m_state.m_tree.m_high.m_second.m_a2 = v70;
        m_state.m_tree.m_high.m_second.m_a3 = v71;
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
