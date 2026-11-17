#pragma once

// Generated from DSP.LinkwitzRileyCrossover.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct LinkwitzRileyCrossoverGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
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
        } m_lowSecond;

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
        } m_highSecond;

    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_first.m_ic2);
        const Vector v1 = (m_state.m_first.m_a1 * m_state.m_first.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_first.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_first.m_a2, m_state.m_first.m_ic1, m_state.m_first.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_first.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_first.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_first.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_first.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (v4 - m_state.m_lowSecond.m_ic2);
        const Vector v14 = (m_state.m_lowSecond.m_a1 * m_state.m_lowSecond.m_ic1);
        const Vector v15 = Vector::FusedMultiplyAdd(m_state.m_lowSecond.m_a2, v13, v14);
        const Vector v16 = Vector::FusedMultiplyAdd(m_state.m_lowSecond.m_a2, m_state.m_lowSecond.m_ic1, m_state.m_lowSecond.m_ic2);
        const Vector v17 = Vector::FusedMultiplyAdd(m_state.m_lowSecond.m_a3, v13, v16);
        const Vector v18 = (-m_state.m_lowSecond.m_k);
        const Vector v19 = Vector::FusedMultiplyAdd(v18, v15, v4);
        const Vector v20 = (v19 - v17);
        const Vector v21 = (-m_state.m_lowSecond.m_ic1);
        const Vector v22 = Vector::FusedMultiplyAdd(v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v21);
        const Vector v23 = (-m_state.m_lowSecond.m_ic2);
        const Vector v24 = Vector::FusedMultiplyAdd(v17, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v23);
        const Vector v25 = (v17 + v20);
        const Vector v26 = (v7 - m_state.m_highSecond.m_ic2);
        const Vector v27 = (m_state.m_highSecond.m_a1 * m_state.m_highSecond.m_ic1);
        const Vector v28 = Vector::FusedMultiplyAdd(m_state.m_highSecond.m_a2, v26, v27);
        const Vector v29 = Vector::FusedMultiplyAdd(m_state.m_highSecond.m_a2, m_state.m_highSecond.m_ic1, m_state.m_highSecond.m_ic2);
        const Vector v30 = Vector::FusedMultiplyAdd(m_state.m_highSecond.m_a3, v26, v29);
        const Vector v31 = (-m_state.m_highSecond.m_k);
        const Vector v32 = Vector::FusedMultiplyAdd(v31, v28, v7);
        const Vector v33 = (v32 - v30);
        const Vector v34 = (-m_state.m_highSecond.m_ic1);
        const Vector v35 = Vector::FusedMultiplyAdd(v28, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v34);
        const Vector v36 = (-m_state.m_highSecond.m_ic2);
        const Vector v37 = Vector::FusedMultiplyAdd(v30, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v36);
        const Vector v38 = (v30 + v33);
        const Vector v39 = m_state.m_first.m_k;
        const Vector v40 = m_state.m_first.m_a1;
        const Vector v41 = m_state.m_first.m_a2;
        const Vector v42 = m_state.m_first.m_a3;
        const Vector v43 = v9;
        const Vector v44 = v11;
        const Vector v45 = v4;
        const Vector v46 = v2;
        const Vector v47 = v7;
        const Vector v48 = v12;
        const Vector v49 = m_state.m_lowSecond.m_k;
        const Vector v50 = m_state.m_lowSecond.m_a1;
        const Vector v51 = m_state.m_lowSecond.m_a2;
        const Vector v52 = m_state.m_lowSecond.m_a3;
        const Vector v53 = v22;
        const Vector v54 = v24;
        const Vector v55 = v17;
        const Vector v56 = v15;
        const Vector v57 = v20;
        const Vector v58 = v25;
        const Vector v59 = m_state.m_highSecond.m_k;
        const Vector v60 = m_state.m_highSecond.m_a1;
        const Vector v61 = m_state.m_highSecond.m_a2;
        const Vector v62 = m_state.m_highSecond.m_a3;
        const Vector v63 = v35;
        const Vector v64 = v37;
        const Vector v65 = v30;
        const Vector v66 = v28;
        const Vector v67 = v33;
        const Vector v68 = v38;
        m_state.m_first.m_k = v39;
        m_state.m_first.m_a1 = v40;
        m_state.m_first.m_a2 = v41;
        m_state.m_first.m_a3 = v42;
        m_state.m_first.m_ic1 = v43;
        m_state.m_first.m_ic2 = v44;
        m_state.m_first.m_lowPass = v45;
        m_state.m_first.m_bandPass = v46;
        m_state.m_first.m_highPass = v47;
        m_state.m_first.m_notch = v48;
        m_state.m_lowSecond.m_k = v49;
        m_state.m_lowSecond.m_a1 = v50;
        m_state.m_lowSecond.m_a2 = v51;
        m_state.m_lowSecond.m_a3 = v52;
        m_state.m_lowSecond.m_ic1 = v53;
        m_state.m_lowSecond.m_ic2 = v54;
        m_state.m_lowSecond.m_lowPass = v55;
        m_state.m_lowSecond.m_bandPass = v56;
        m_state.m_lowSecond.m_highPass = v57;
        m_state.m_lowSecond.m_notch = v58;
        m_state.m_highSecond.m_k = v59;
        m_state.m_highSecond.m_a1 = v60;
        m_state.m_highSecond.m_a2 = v61;
        m_state.m_highSecond.m_a3 = v62;
        m_state.m_highSecond.m_ic1 = v63;
        m_state.m_highSecond.m_ic2 = v64;
        m_state.m_highSecond.m_lowPass = v65;
        m_state.m_highSecond.m_bandPass = v66;
        m_state.m_highSecond.m_highPass = v67;
        m_state.m_highSecond.m_notch = v68;
    }

};
