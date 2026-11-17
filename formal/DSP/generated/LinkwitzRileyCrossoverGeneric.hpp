#pragma once

// Generated from DSP.LinkwitzRileyCrossover.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and contraction disabled.
//

#include <limits>
#include <array>

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
        const Vector v2 = (m_state.m_first.m_a2 * v0);
        const Vector v3 = (v1 + v2);
        const Vector v4 = (m_state.m_first.m_a2 * m_state.m_first.m_ic1);
        const Vector v5 = (m_state.m_first.m_ic2 + v4);
        const Vector v6 = (m_state.m_first.m_a3 * v0);
        const Vector v7 = (v5 + v6);
        const Vector v8 = (m_state.m_first.m_k * v3);
        const Vector v9 = (input.m_x - v8);
        const Vector v10 = (v9 - v7);
        const Vector v11 = (v3 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v12 = (v11 - m_state.m_first.m_ic1);
        const Vector v13 = (v7 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v14 = (v13 - m_state.m_first.m_ic2);
        const Vector v15 = (v7 + v10);
        const Vector v16 = (v7 - m_state.m_lowSecond.m_ic2);
        const Vector v17 = (m_state.m_lowSecond.m_a1 * m_state.m_lowSecond.m_ic1);
        const Vector v18 = (m_state.m_lowSecond.m_a2 * v16);
        const Vector v19 = (v17 + v18);
        const Vector v20 = (m_state.m_lowSecond.m_a2 * m_state.m_lowSecond.m_ic1);
        const Vector v21 = (m_state.m_lowSecond.m_ic2 + v20);
        const Vector v22 = (m_state.m_lowSecond.m_a3 * v16);
        const Vector v23 = (v21 + v22);
        const Vector v24 = (m_state.m_lowSecond.m_k * v19);
        const Vector v25 = (v7 - v24);
        const Vector v26 = (v25 - v23);
        const Vector v27 = (v19 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v28 = (v27 - m_state.m_lowSecond.m_ic1);
        const Vector v29 = (v23 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v30 = (v29 - m_state.m_lowSecond.m_ic2);
        const Vector v31 = (v23 + v26);
        const Vector v32 = (v10 - m_state.m_highSecond.m_ic2);
        const Vector v33 = (m_state.m_highSecond.m_a1 * m_state.m_highSecond.m_ic1);
        const Vector v34 = (m_state.m_highSecond.m_a2 * v32);
        const Vector v35 = (v33 + v34);
        const Vector v36 = (m_state.m_highSecond.m_a2 * m_state.m_highSecond.m_ic1);
        const Vector v37 = (m_state.m_highSecond.m_ic2 + v36);
        const Vector v38 = (m_state.m_highSecond.m_a3 * v32);
        const Vector v39 = (v37 + v38);
        const Vector v40 = (m_state.m_highSecond.m_k * v35);
        const Vector v41 = (v10 - v40);
        const Vector v42 = (v41 - v39);
        const Vector v43 = (v35 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v44 = (v43 - m_state.m_highSecond.m_ic1);
        const Vector v45 = (v39 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v46 = (v45 - m_state.m_highSecond.m_ic2);
        const Vector v47 = (v39 + v42);
        const Vector v48 = m_state.m_first.m_k;
        const Vector v49 = m_state.m_first.m_a1;
        const Vector v50 = m_state.m_first.m_a2;
        const Vector v51 = m_state.m_first.m_a3;
        const Vector v52 = v12;
        const Vector v53 = v14;
        const Vector v54 = v7;
        const Vector v55 = v3;
        const Vector v56 = v10;
        const Vector v57 = v15;
        const Vector v58 = m_state.m_lowSecond.m_k;
        const Vector v59 = m_state.m_lowSecond.m_a1;
        const Vector v60 = m_state.m_lowSecond.m_a2;
        const Vector v61 = m_state.m_lowSecond.m_a3;
        const Vector v62 = v28;
        const Vector v63 = v30;
        const Vector v64 = v23;
        const Vector v65 = v19;
        const Vector v66 = v26;
        const Vector v67 = v31;
        const Vector v68 = m_state.m_highSecond.m_k;
        const Vector v69 = m_state.m_highSecond.m_a1;
        const Vector v70 = m_state.m_highSecond.m_a2;
        const Vector v71 = m_state.m_highSecond.m_a3;
        const Vector v72 = v44;
        const Vector v73 = v46;
        const Vector v74 = v39;
        const Vector v75 = v35;
        const Vector v76 = v42;
        const Vector v77 = v47;
        m_state.m_first.m_k = v48;
        m_state.m_first.m_a1 = v49;
        m_state.m_first.m_a2 = v50;
        m_state.m_first.m_a3 = v51;
        m_state.m_first.m_ic1 = v52;
        m_state.m_first.m_ic2 = v53;
        m_state.m_first.m_lowPass = v54;
        m_state.m_first.m_bandPass = v55;
        m_state.m_first.m_highPass = v56;
        m_state.m_first.m_notch = v57;
        m_state.m_lowSecond.m_k = v58;
        m_state.m_lowSecond.m_a1 = v59;
        m_state.m_lowSecond.m_a2 = v60;
        m_state.m_lowSecond.m_a3 = v61;
        m_state.m_lowSecond.m_ic1 = v62;
        m_state.m_lowSecond.m_ic2 = v63;
        m_state.m_lowSecond.m_lowPass = v64;
        m_state.m_lowSecond.m_bandPass = v65;
        m_state.m_lowSecond.m_highPass = v66;
        m_state.m_lowSecond.m_notch = v67;
        m_state.m_highSecond.m_k = v68;
        m_state.m_highSecond.m_a1 = v69;
        m_state.m_highSecond.m_a2 = v70;
        m_state.m_highSecond.m_a3 = v71;
        m_state.m_highSecond.m_ic1 = v72;
        m_state.m_highSecond.m_ic2 = v73;
        m_state.m_highSecond.m_lowPass = v74;
        m_state.m_highSecond.m_bandPass = v75;
        m_state.m_highSecond.m_highPass = v76;
        m_state.m_highSecond.m_notch = v77;
    }

};
