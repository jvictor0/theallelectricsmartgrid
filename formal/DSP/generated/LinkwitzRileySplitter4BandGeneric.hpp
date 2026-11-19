#pragma once

// Generated from DSP.LinkwitzRileySplitter.Runtime.process4Band. Do not edit.
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
            } m_first;

            struct
            {
                Vector m_k;
                Vector m_a1;
                Vector m_a2;
                Vector m_a3;
                Vector m_ic1;
                Vector m_ic2;
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
                } m_svf;

                struct
                {
                } m_low;

                struct
                {
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
                } m_svf;

                struct
                {
                } m_low;

                struct
                {
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
                } m_first;

                struct
                {
                    Vector m_k;
                    Vector m_a1;
                    Vector m_a2;
                    Vector m_a3;
                    Vector m_ic1;
                    Vector m_ic2;
                } m_second;

                struct
                {
                } m_lowPhase;

                struct
                {
                } m_highPhase;

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
                    Vector m_k;
                    Vector m_a1;
                    Vector m_a2;
                    Vector m_a3;
                    Vector m_ic1;
                    Vector m_ic2;
                } m_first;

                struct
                {
                    Vector m_k;
                    Vector m_a1;
                    Vector m_a2;
                    Vector m_a3;
                    Vector m_ic1;
                    Vector m_ic2;
                } m_second;

                struct
                {
                } m_lowPhase;

                struct
                {
                } m_highPhase;

                struct
                {
                } m_low;

                struct
                {
                } m_high;

            } m_high;

        } m_tree;

    };

    struct Input
    {
        Vector m_x;
    };

    struct Output
    {
        std::array<Vector, 4> m_bands;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_tree.m_first.m_ic2);
        const Vector v1 = (m_state.m_tree.m_first.m_a1 * m_state.m_tree.m_first.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_tree.m_first.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_tree.m_first.m_a2, m_state.m_tree.m_first.m_ic1, m_state.m_tree.m_first.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_tree.m_first.m_a3, v0, v3);
        const Vector v8 = (-m_state.m_tree.m_first.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        m_state.m_tree.m_first.m_ic1 = v9;
        const Vector v10 = (-m_state.m_tree.m_first.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        m_state.m_tree.m_first.m_ic2 = v11;
        const Vector v13 = (-m_state.m_tree.m_first.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        const Vector v16 = (v4 - m_state.m_tree.m_second.m_ic2);
        const Vector v17 = (m_state.m_tree.m_second.m_a1 * m_state.m_tree.m_second.m_ic1);
        const Vector v18 = Vector::FusedMultiplyAdd(m_state.m_tree.m_second.m_a2, v16, v17);
        const Vector v19 = Vector::FusedMultiplyAdd(m_state.m_tree.m_second.m_a2, m_state.m_tree.m_second.m_ic1, m_state.m_tree.m_second.m_ic2);
        const Vector v20 = Vector::FusedMultiplyAdd(m_state.m_tree.m_second.m_a3, v16, v19);
        const Vector v24 = (-m_state.m_tree.m_second.m_ic1);
        const Vector v25 = Vector::FusedMultiplyAdd(v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v24);
        m_state.m_tree.m_second.m_ic1 = v25;
        const Vector v26 = (-m_state.m_tree.m_second.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        m_state.m_tree.m_second.m_ic2 = v27;
        const Vector v32 = (v20 - m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v33 = (m_state.m_tree.m_lowPhase.m_svf.m_a1 * m_state.m_tree.m_lowPhase.m_svf.m_ic1);
        const Vector v34 = Vector::FusedMultiplyAdd(m_state.m_tree.m_lowPhase.m_svf.m_a2, v32, v33);
        const Vector v35 = Vector::FusedMultiplyAdd(m_state.m_tree.m_lowPhase.m_svf.m_a2, m_state.m_tree.m_lowPhase.m_svf.m_ic1, m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v36 = Vector::FusedMultiplyAdd(m_state.m_tree.m_lowPhase.m_svf.m_a3, v32, v35);
        const Vector v40 = (-m_state.m_tree.m_lowPhase.m_svf.m_ic1);
        const Vector v41 = Vector::FusedMultiplyAdd(v34, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v40);
        m_state.m_tree.m_lowPhase.m_svf.m_ic1 = v41;
        const Vector v42 = (-m_state.m_tree.m_lowPhase.m_svf.m_ic2);
        const Vector v43 = Vector::FusedMultiplyAdd(v36, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v42);
        m_state.m_tree.m_lowPhase.m_svf.m_ic2 = v43;
        const Vector v45 = (-m_state.m_tree.m_lowPhase.m_svf.m_k);
        const Vector v46 = (v45 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v47 = Vector::FusedMultiplyAdd(v46, v34, v20);
        const Vector v48 = (v15 - v20);
        const Vector v49 = (v48 - m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v50 = (m_state.m_tree.m_highPhase.m_svf.m_a1 * m_state.m_tree.m_highPhase.m_svf.m_ic1);
        const Vector v51 = Vector::FusedMultiplyAdd(m_state.m_tree.m_highPhase.m_svf.m_a2, v49, v50);
        const Vector v52 = Vector::FusedMultiplyAdd(m_state.m_tree.m_highPhase.m_svf.m_a2, m_state.m_tree.m_highPhase.m_svf.m_ic1, m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v53 = Vector::FusedMultiplyAdd(m_state.m_tree.m_highPhase.m_svf.m_a3, v49, v52);
        const Vector v58 = (-m_state.m_tree.m_highPhase.m_svf.m_ic1);
        const Vector v59 = Vector::FusedMultiplyAdd(v51, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v58);
        m_state.m_tree.m_highPhase.m_svf.m_ic1 = v59;
        const Vector v60 = (-m_state.m_tree.m_highPhase.m_svf.m_ic2);
        const Vector v61 = Vector::FusedMultiplyAdd(v53, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v60);
        m_state.m_tree.m_highPhase.m_svf.m_ic2 = v61;
        const Vector v63 = (-m_state.m_tree.m_highPhase.m_svf.m_k);
        const Vector v64 = (v63 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v65 = (v15 - v20);
        const Vector v66 = Vector::FusedMultiplyAdd(v64, v51, v65);
        const Vector v67 = (v47 - m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v68 = (m_state.m_tree.m_low.m_first.m_a1 * m_state.m_tree.m_low.m_first.m_ic1);
        const Vector v69 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_first.m_a2, v67, v68);
        const Vector v70 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_first.m_a2, m_state.m_tree.m_low.m_first.m_ic1, m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v71 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_first.m_a3, v67, v70);
        const Vector v75 = (-m_state.m_tree.m_low.m_first.m_ic1);
        const Vector v76 = Vector::FusedMultiplyAdd(v69, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v75);
        m_state.m_tree.m_low.m_first.m_ic1 = v76;
        const Vector v77 = (-m_state.m_tree.m_low.m_first.m_ic2);
        const Vector v78 = Vector::FusedMultiplyAdd(v71, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v77);
        m_state.m_tree.m_low.m_first.m_ic2 = v78;
        const Vector v80 = (-m_state.m_tree.m_low.m_first.m_k);
        const Vector v81 = (v80 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v82 = Vector::FusedMultiplyAdd(v81, v69, v47);
        const Vector v83 = (v71 - m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v84 = (m_state.m_tree.m_low.m_second.m_a1 * m_state.m_tree.m_low.m_second.m_ic1);
        const Vector v85 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_second.m_a2, v83, v84);
        const Vector v86 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_second.m_a2, m_state.m_tree.m_low.m_second.m_ic1, m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v87 = Vector::FusedMultiplyAdd(m_state.m_tree.m_low.m_second.m_a3, v83, v86);
        const Vector v91 = (-m_state.m_tree.m_low.m_second.m_ic1);
        const Vector v92 = Vector::FusedMultiplyAdd(v85, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v91);
        m_state.m_tree.m_low.m_second.m_ic1 = v92;
        const Vector v93 = (-m_state.m_tree.m_low.m_second.m_ic2);
        const Vector v94 = Vector::FusedMultiplyAdd(v87, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v93);
        m_state.m_tree.m_low.m_second.m_ic2 = v94;
        const Vector v99 = (v82 - v87);
        m_output.m_bands[0] = v87;
        m_output.m_bands[1] = v99;
        const Vector v100 = (v66 - m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v101 = (m_state.m_tree.m_high.m_first.m_a1 * m_state.m_tree.m_high.m_first.m_ic1);
        const Vector v102 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_first.m_a2, v100, v101);
        const Vector v103 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_first.m_a2, m_state.m_tree.m_high.m_first.m_ic1, m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v104 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_first.m_a3, v100, v103);
        const Vector v108 = (-m_state.m_tree.m_high.m_first.m_ic1);
        const Vector v109 = Vector::FusedMultiplyAdd(v102, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v108);
        m_state.m_tree.m_high.m_first.m_ic1 = v109;
        const Vector v110 = (-m_state.m_tree.m_high.m_first.m_ic2);
        const Vector v111 = Vector::FusedMultiplyAdd(v104, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v110);
        m_state.m_tree.m_high.m_first.m_ic2 = v111;
        const Vector v113 = (-m_state.m_tree.m_high.m_first.m_k);
        const Vector v114 = (v113 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v115 = Vector::FusedMultiplyAdd(v114, v102, v66);
        const Vector v116 = (v104 - m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v117 = (m_state.m_tree.m_high.m_second.m_a1 * m_state.m_tree.m_high.m_second.m_ic1);
        const Vector v118 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_second.m_a2, v116, v117);
        const Vector v119 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_second.m_a2, m_state.m_tree.m_high.m_second.m_ic1, m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v120 = Vector::FusedMultiplyAdd(m_state.m_tree.m_high.m_second.m_a3, v116, v119);
        const Vector v124 = (-m_state.m_tree.m_high.m_second.m_ic1);
        const Vector v125 = Vector::FusedMultiplyAdd(v118, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v124);
        m_state.m_tree.m_high.m_second.m_ic1 = v125;
        const Vector v126 = (-m_state.m_tree.m_high.m_second.m_ic2);
        const Vector v127 = Vector::FusedMultiplyAdd(v120, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v126);
        m_state.m_tree.m_high.m_second.m_ic2 = v127;
        const Vector v132 = (v115 - v120);
        m_output.m_bands[2] = v120;
        m_output.m_bands[3] = v132;
    }

    // Coefficients generated from DSP.LinkwitzRileySplitter.Runtime.computeCoefficients4Band.
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
        const Vector v2 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v2);
        const Vector v8 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v10 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v11 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v10);
        const Vector v12 = (v8 + v11);
        const Vector v13 = Vector::FusedMultiplyAdd(v8, v12, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v14 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v13);
        const Vector v16 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v18 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v18);
        const Vector v20 = (v16 + v19);
        const Vector v21 = Vector::FusedMultiplyAdd(v16, v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v21);
        const Vector v23 = (v16 * v22);
        const Vector v24 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v26 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v27 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v26);
        const Vector v28 = (v24 + v27);
        const Vector v29 = Vector::FusedMultiplyAdd(v24, v28, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v30 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v29);
        const Vector v31 = (v24 * v30);
        const Vector v32 = (v24 * v31);
        const Vector v51 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v52 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v51);
        const Vector v57 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v59 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v60 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v59);
        const Vector v61 = (v57 + v60);
        const Vector v62 = Vector::FusedMultiplyAdd(v57, v61, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v63 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v62);
        const Vector v65 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v67 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v68 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v67);
        const Vector v69 = (v65 + v68);
        const Vector v70 = Vector::FusedMultiplyAdd(v65, v69, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v71 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v70);
        const Vector v72 = (v65 * v71);
        const Vector v73 = Math4096::TanPi(spec.m_tree.m_value);
        const Vector v75 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v76 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v75);
        const Vector v77 = (v73 + v76);
        const Vector v78 = Vector::FusedMultiplyAdd(v73, v77, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v79 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v78);
        const Vector v80 = (v73 * v79);
        const Vector v81 = (v73 * v80);
        const Vector v100 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v101 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v100);
        const Vector v106 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v108 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v109 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v108);
        const Vector v110 = (v106 + v109);
        const Vector v111 = Vector::FusedMultiplyAdd(v106, v110, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v112 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v111);
        const Vector v114 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v116 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v117 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v116);
        const Vector v118 = (v114 + v117);
        const Vector v119 = Vector::FusedMultiplyAdd(v114, v118, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v120 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v119);
        const Vector v121 = (v114 * v120);
        const Vector v122 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v124 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v125 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v124);
        const Vector v126 = (v122 + v125);
        const Vector v127 = Vector::FusedMultiplyAdd(v122, v126, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v128 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v127);
        const Vector v129 = (v122 * v128);
        const Vector v130 = (v122 * v129);
        const Vector v149 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v150 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v149);
        const Vector v155 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v157 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v158 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v157);
        const Vector v159 = (v155 + v158);
        const Vector v160 = Vector::FusedMultiplyAdd(v155, v159, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v161 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v160);
        const Vector v163 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v165 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v166 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v165);
        const Vector v167 = (v163 + v166);
        const Vector v168 = Vector::FusedMultiplyAdd(v163, v167, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v169 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v168);
        const Vector v170 = (v163 * v169);
        const Vector v171 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v173 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v174 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v173);
        const Vector v175 = (v171 + v174);
        const Vector v176 = Vector::FusedMultiplyAdd(v171, v175, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v177 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v176);
        const Vector v178 = (v171 * v177);
        const Vector v179 = (v171 * v178);
        const Vector v198 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v199 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v198);
        const Vector v204 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v206 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v207 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v206);
        const Vector v208 = (v204 + v207);
        const Vector v209 = Vector::FusedMultiplyAdd(v204, v208, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v210 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v209);
        const Vector v212 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v214 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v215 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v214);
        const Vector v216 = (v212 + v215);
        const Vector v217 = Vector::FusedMultiplyAdd(v212, v216, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v218 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v217);
        const Vector v219 = (v212 * v218);
        const Vector v220 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v222 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v223 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v222);
        const Vector v224 = (v220 + v223);
        const Vector v225 = Vector::FusedMultiplyAdd(v220, v224, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v226 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v225);
        const Vector v227 = (v220 * v226);
        const Vector v228 = (v220 * v227);
        const Vector v247 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v248 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v247);
        const Vector v253 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v255 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v256 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v255);
        const Vector v257 = (v253 + v256);
        const Vector v258 = Vector::FusedMultiplyAdd(v253, v257, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v259 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v258);
        const Vector v261 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v263 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v264 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v263);
        const Vector v265 = (v261 + v264);
        const Vector v266 = Vector::FusedMultiplyAdd(v261, v265, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v267 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v266);
        const Vector v268 = (v261 * v267);
        const Vector v269 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v271 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v272 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v271);
        const Vector v273 = (v269 + v272);
        const Vector v274 = Vector::FusedMultiplyAdd(v269, v273, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v275 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v274);
        const Vector v276 = (v269 * v275);
        const Vector v277 = (v269 * v276);
        const Vector v296 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v297 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v296);
        const Vector v302 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v304 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v305 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v304);
        const Vector v306 = (v302 + v305);
        const Vector v307 = Vector::FusedMultiplyAdd(v302, v306, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v308 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v307);
        const Vector v310 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v312 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v313 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v312);
        const Vector v314 = (v310 + v313);
        const Vector v315 = Vector::FusedMultiplyAdd(v310, v314, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v316 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v315);
        const Vector v317 = (v310 * v316);
        const Vector v318 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v320 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v321 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v320);
        const Vector v322 = (v318 + v321);
        const Vector v323 = Vector::FusedMultiplyAdd(v318, v322, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v324 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v323);
        const Vector v325 = (v318 * v324);
        const Vector v326 = (v318 * v325);
        const Vector v345 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v346 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v345);
        const Vector v351 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v353 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v354 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v353);
        const Vector v355 = (v351 + v354);
        const Vector v356 = Vector::FusedMultiplyAdd(v351, v355, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v357 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v356);
        const Vector v359 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v361 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v362 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v361);
        const Vector v363 = (v359 + v362);
        const Vector v364 = Vector::FusedMultiplyAdd(v359, v363, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v365 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v364);
        const Vector v366 = (v359 * v365);
        const Vector v367 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v369 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v370 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v369);
        const Vector v371 = (v367 + v370);
        const Vector v372 = Vector::FusedMultiplyAdd(v367, v371, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v373 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v372);
        const Vector v374 = (v367 * v373);
        const Vector v375 = (v367 * v374);
        m_state.m_tree.m_first.m_k = v3;
        m_state.m_tree.m_first.m_a1 = v14;
        m_state.m_tree.m_first.m_a2 = v23;
        m_state.m_tree.m_first.m_a3 = v32;
        m_state.m_tree.m_second.m_k = v52;
        m_state.m_tree.m_second.m_a1 = v63;
        m_state.m_tree.m_second.m_a2 = v72;
        m_state.m_tree.m_second.m_a3 = v81;
        m_state.m_tree.m_lowPhase.m_svf.m_k = v101;
        m_state.m_tree.m_lowPhase.m_svf.m_a1 = v112;
        m_state.m_tree.m_lowPhase.m_svf.m_a2 = v121;
        m_state.m_tree.m_lowPhase.m_svf.m_a3 = v130;
        m_state.m_tree.m_highPhase.m_svf.m_k = v150;
        m_state.m_tree.m_highPhase.m_svf.m_a1 = v161;
        m_state.m_tree.m_highPhase.m_svf.m_a2 = v170;
        m_state.m_tree.m_highPhase.m_svf.m_a3 = v179;
        m_state.m_tree.m_low.m_first.m_k = v199;
        m_state.m_tree.m_low.m_first.m_a1 = v210;
        m_state.m_tree.m_low.m_first.m_a2 = v219;
        m_state.m_tree.m_low.m_first.m_a3 = v228;
        m_state.m_tree.m_low.m_second.m_k = v248;
        m_state.m_tree.m_low.m_second.m_a1 = v259;
        m_state.m_tree.m_low.m_second.m_a2 = v268;
        m_state.m_tree.m_low.m_second.m_a3 = v277;
        m_state.m_tree.m_high.m_first.m_k = v297;
        m_state.m_tree.m_high.m_first.m_a1 = v308;
        m_state.m_tree.m_high.m_first.m_a2 = v317;
        m_state.m_tree.m_high.m_first.m_a3 = v326;
        m_state.m_tree.m_high.m_second.m_k = v346;
        m_state.m_tree.m_high.m_second.m_a1 = v357;
        m_state.m_tree.m_high.m_second.m_a2 = v366;
        m_state.m_tree.m_high.m_second.m_a3 = v375;
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
        const Vector v2 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v2);
        const Vector v4 = Math4096::TanPi(spec.m_tree.m_low.m_value);
        const Vector v6 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v6);
        const Vector v8 = Math4096::TanPi(spec.m_tree.m_high.m_value);
        const Vector v10 = Vector::Splat(static_cast<Number>(Math4096::CosEighth(1)));
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
