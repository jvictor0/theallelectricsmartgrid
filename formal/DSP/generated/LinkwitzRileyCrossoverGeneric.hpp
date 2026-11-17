#pragma once

// Generated from DSP.LinkwitzRileyCrossover.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"

#include "ResponseFrequency.hpp"
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

    // Coefficients generated from DSP.LinkwitzRileyCrossover.computeCoefficients.
    //

    struct LogicalSpec
    {
        Vector m_cutoff;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-20f : 0x10000000000000p-49)));
        const Vector v1 = Math4096::Cos2pi(v0);
        const Vector v2 = Math4096::TanPi(spec.m_cutoff);
        const Vector v3 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v1);
        const Vector v4 = (v2 + v3);
        const Vector v5 = Vector::FusedMultiplyAdd(v2, v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v6 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v5);
        const Vector v7 = (v2 * v6);
        const Vector v8 = (v2 * v7);
        const Vector v9 = Math4096::TanPi(spec.m_cutoff);
        const Vector v10 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v1);
        const Vector v11 = (v9 + v10);
        const Vector v12 = Vector::FusedMultiplyAdd(v9, v11, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v13 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v12);
        const Vector v14 = (v9 * v13);
        const Vector v15 = (v9 * v14);
        const Vector v16 = Math4096::TanPi(spec.m_cutoff);
        const Vector v17 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v1);
        const Vector v18 = (v16 + v17);
        const Vector v19 = Vector::FusedMultiplyAdd(v16, v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v20 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v19);
        const Vector v21 = (v16 * v20);
        const Vector v22 = (v16 * v21);
        const Vector v23 = v3;
        const Vector v24 = v6;
        const Vector v25 = v7;
        const Vector v26 = v8;
        const Vector v27 = v10;
        const Vector v28 = v13;
        const Vector v29 = v14;
        const Vector v30 = v15;
        const Vector v31 = v17;
        const Vector v32 = v20;
        const Vector v33 = v21;
        const Vector v34 = v22;
        m_state.m_first.m_k = v23;
        m_state.m_first.m_a1 = v24;
        m_state.m_first.m_a2 = v25;
        m_state.m_first.m_a3 = v26;
        m_state.m_lowSecond.m_k = v27;
        m_state.m_lowSecond.m_a1 = v28;
        m_state.m_lowSecond.m_a2 = v29;
        m_state.m_lowSecond.m_a3 = v30;
        m_state.m_highSecond.m_k = v31;
        m_state.m_highSecond.m_a1 = v32;
        m_state.m_highSecond.m_a2 = v33;
        m_state.m_highSecond.m_a3 = v34;
    }

    // Response generated from DSP.LinkwitzRileyCrossover.Snapshot.lowPassMagnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        struct
        {
            Vector m_g;
            Vector m_k;
        } m_first;

        struct
        {
            Vector m_g;
            Vector m_k;
        } m_lowSecond;

        struct
        {
            Vector m_g;
            Vector m_k;
        } m_highSecond;

    };

    static Vector LowPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_first.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_first.m_k * v3);
        const Vector v10 = (v7 * v7);
        const Vector v11 = Vector::FusedMultiplyAdd(v6, v6, v10);
        const Vector v12 = (v7 * v7);
        const Vector v13 = Vector::FusedMultiplyAdd(v6, v6, v12);
        const Vector v14 = (v9 * v9);
        const Vector v15 = Vector::FusedMultiplyAdd(v8, v8, v14);
        const Vector v16 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v13, v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v17 = (v11 / v16);
        const Vector v18 = (snapshot.m_lowSecond.m_g * input.m_cosHalf);
        const Vector v19 = (v18 * v18);
        const Vector v20 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v21 = (v18 * input.m_sinHalf);
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v19);
        const Vector v23 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v20);
        const Vector v24 = (v22 - v23);
        const Vector v25 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v21);
        const Vector v26 = (v19 - v20);
        const Vector v27 = (snapshot.m_lowSecond.m_k * v21);
        const Vector v28 = (v25 * v25);
        const Vector v29 = Vector::FusedMultiplyAdd(v24, v24, v28);
        const Vector v30 = (v25 * v25);
        const Vector v31 = Vector::FusedMultiplyAdd(v24, v24, v30);
        const Vector v32 = (v27 * v27);
        const Vector v33 = Vector::FusedMultiplyAdd(v26, v26, v32);
        const Vector v34 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v31, v33, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v35 = (v29 / v34);
        const Vector v36 = (v17 * v35);
        return v36;
    }

    // Response generated from DSP.LinkwitzRileyCrossover.Snapshot.highPassMagnitudeSquared.
    //

    static Vector HighPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_first.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_first.m_k * v3);
        const Vector v10 = (v7 * v7);
        const Vector v11 = Vector::FusedMultiplyAdd(v6, v6, v10);
        const Vector v12 = (v7 * v7);
        const Vector v13 = Vector::FusedMultiplyAdd(v6, v6, v12);
        const Vector v14 = (v9 * v9);
        const Vector v15 = Vector::FusedMultiplyAdd(v8, v8, v14);
        const Vector v16 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v13, v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v17 = (v11 / v16);
        const Vector v18 = (snapshot.m_highSecond.m_g * input.m_cosHalf);
        const Vector v19 = (v18 * v18);
        const Vector v20 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v21 = (v18 * input.m_sinHalf);
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v19);
        const Vector v23 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v20);
        const Vector v24 = (v22 - v23);
        const Vector v25 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v21);
        const Vector v26 = (v19 - v20);
        const Vector v27 = (snapshot.m_highSecond.m_k * v21);
        const Vector v28 = (v25 * v25);
        const Vector v29 = Vector::FusedMultiplyAdd(v24, v24, v28);
        const Vector v30 = (v25 * v25);
        const Vector v31 = Vector::FusedMultiplyAdd(v24, v24, v30);
        const Vector v32 = (v27 * v27);
        const Vector v33 = Vector::FusedMultiplyAdd(v26, v26, v32);
        const Vector v34 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v31, v33, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v35 = (v29 / v34);
        const Vector v36 = (v17 * v35);
        return v36;
    }

};
