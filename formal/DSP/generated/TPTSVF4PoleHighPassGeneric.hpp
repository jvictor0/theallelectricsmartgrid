#pragma once

// Generated from DSP.TPTSVF4Pole.stepHighPass. Do not edit.
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
struct TPTSVF4PoleHighPassGeneric
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

    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_stage1.m_ic2);
        const Vector v1 = (m_state.m_stage1.m_a1 * m_state.m_stage1.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_stage1.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_stage1.m_a2, m_state.m_stage1.m_ic1, m_state.m_stage1.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_stage1.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_stage1.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_stage1.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_stage1.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (-m_state.m_stage1.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        const Vector v16 = (v7 - m_state.m_stage2.m_ic2);
        const Vector v17 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const Vector v18 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, v16, v17);
        const Vector v19 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, m_state.m_stage2.m_ic1, m_state.m_stage2.m_ic2);
        const Vector v20 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a3, v16, v19);
        const Vector v21 = (-m_state.m_stage2.m_k);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, v18, v7);
        const Vector v23 = (v22 - v20);
        const Vector v24 = (-m_state.m_stage2.m_ic1);
        const Vector v25 = Vector::FusedMultiplyAdd(v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v24);
        const Vector v26 = (-m_state.m_stage2.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        const Vector v28 = (v20 + v23);
        const Vector v29 = (-m_state.m_stage2.m_k);
        const Vector v30 = (v29 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v31 = Vector::FusedMultiplyAdd(v30, v18, v7);
        const Vector v32 = m_state.m_stage1.m_k;
        const Vector v33 = m_state.m_stage1.m_a1;
        const Vector v34 = m_state.m_stage1.m_a2;
        const Vector v35 = m_state.m_stage1.m_a3;
        const Vector v36 = v9;
        const Vector v37 = v11;
        const Vector v38 = v4;
        const Vector v39 = v2;
        const Vector v40 = v7;
        const Vector v41 = v12;
        const Vector v42 = v15;
        const Vector v43 = m_state.m_stage2.m_k;
        const Vector v44 = m_state.m_stage2.m_a1;
        const Vector v45 = m_state.m_stage2.m_a2;
        const Vector v46 = m_state.m_stage2.m_a3;
        const Vector v47 = v25;
        const Vector v48 = v27;
        const Vector v49 = v20;
        const Vector v50 = v18;
        const Vector v51 = v23;
        const Vector v52 = v28;
        const Vector v53 = v31;
        m_state.m_stage1.m_k = v32;
        m_state.m_stage1.m_a1 = v33;
        m_state.m_stage1.m_a2 = v34;
        m_state.m_stage1.m_a3 = v35;
        m_state.m_stage1.m_ic1 = v36;
        m_state.m_stage1.m_ic2 = v37;
        m_state.m_stage1.m_lowPass = v38;
        m_state.m_stage1.m_bandPass = v39;
        m_state.m_stage1.m_highPass = v40;
        m_state.m_stage1.m_notch = v41;
        m_state.m_stage1.m_allPass = v42;
        m_state.m_stage2.m_k = v43;
        m_state.m_stage2.m_a1 = v44;
        m_state.m_stage2.m_a2 = v45;
        m_state.m_stage2.m_a3 = v46;
        m_state.m_stage2.m_ic1 = v47;
        m_state.m_stage2.m_ic2 = v48;
        m_state.m_stage2.m_lowPass = v49;
        m_state.m_stage2.m_bandPass = v50;
        m_state.m_stage2.m_highPass = v51;
        m_state.m_stage2.m_notch = v52;
        m_state.m_stage2.m_allPass = v53;
    }

    // Coefficients generated from DSP.TPTSVF4Pole.computeCoefficients.
    //

    struct LogicalSpec
    {
        struct
        {
            Vector m_cutoff;
            Vector m_q;
        } m_stage1;

        struct
        {
            Vector m_cutoff;
            Vector m_q;
        } m_stage2;

    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_stage1.m_cutoff);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / spec.m_stage1.m_q);
        const Vector v2 = (v0 + v1);
        const Vector v3 = Vector::FusedMultiplyAdd(v0, v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v3);
        const Vector v5 = (v0 * v4);
        const Vector v6 = (v0 * v5);
        const Vector v7 = Math4096::TanPi(spec.m_stage2.m_cutoff);
        const Vector v8 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / spec.m_stage2.m_q);
        const Vector v9 = (v7 + v8);
        const Vector v10 = Vector::FusedMultiplyAdd(v7, v9, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v11 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v10);
        const Vector v12 = (v7 * v11);
        const Vector v13 = (v7 * v12);
        const Vector v14 = v1;
        const Vector v15 = v4;
        const Vector v16 = v5;
        const Vector v17 = v6;
        const Vector v18 = v8;
        const Vector v19 = v11;
        const Vector v20 = v12;
        const Vector v21 = v13;
        m_state.m_stage1.m_k = v14;
        m_state.m_stage1.m_a1 = v15;
        m_state.m_stage1.m_a2 = v16;
        m_state.m_stage1.m_a3 = v17;
        m_state.m_stage2.m_k = v18;
        m_state.m_stage2.m_a1 = v19;
        m_state.m_stage2.m_a2 = v20;
        m_state.m_stage2.m_a3 = v21;
    }

    // Response generated from DSP.TPTSVF4Pole.Snapshot.highPassMagnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        struct
        {
            Vector m_g;
            Vector m_k;
        } m_stage1;

        struct
        {
            Vector m_g;
            Vector m_k;
        } m_stage2;

    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_stage1.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_stage1.m_k * v3);
        const Vector v10 = (v7 * v7);
        const Vector v11 = Vector::FusedMultiplyAdd(v6, v6, v10);
        const Vector v12 = (v7 * v7);
        const Vector v13 = Vector::FusedMultiplyAdd(v6, v6, v12);
        const Vector v14 = (v9 * v9);
        const Vector v15 = Vector::FusedMultiplyAdd(v8, v8, v14);
        const Vector v16 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v13, v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v17 = (v11 / v16);
        const Vector v18 = (snapshot.m_stage2.m_g * input.m_cosHalf);
        const Vector v19 = (v18 * v18);
        const Vector v20 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v21 = (v18 * input.m_sinHalf);
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v19);
        const Vector v23 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v20);
        const Vector v24 = (v22 - v23);
        const Vector v25 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v21);
        const Vector v26 = (v19 - v20);
        const Vector v27 = (snapshot.m_stage2.m_k * v21);
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
