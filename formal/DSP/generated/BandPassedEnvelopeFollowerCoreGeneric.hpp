#pragma once

// Generated from DSP.BandPassedEnvelopeFollower.step. Do not edit.
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
struct BandPassedEnvelopeFollowerCoreGeneric
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

        Vector m_power;
        Vector m_powerAlpha;
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
        const Vector v16 = (v2 - m_state.m_stage2.m_ic2);
        const Vector v17 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const Vector v18 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, v16, v17);
        const Vector v19 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, m_state.m_stage2.m_ic1, m_state.m_stage2.m_ic2);
        const Vector v20 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a3, v16, v19);
        const Vector v21 = (-m_state.m_stage2.m_k);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, v18, v2);
        const Vector v23 = (v22 - v20);
        const Vector v24 = (-m_state.m_stage2.m_ic1);
        const Vector v25 = Vector::FusedMultiplyAdd(v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v24);
        const Vector v26 = (-m_state.m_stage2.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        const Vector v28 = (v20 + v23);
        const Vector v29 = (-m_state.m_stage2.m_k);
        const Vector v30 = (v29 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v31 = Vector::FusedMultiplyAdd(v30, v18, v2);
        const Vector v32 = (v18 * v18);
        const Vector v33 = (v32 - m_state.m_power);
        const Vector v34 = (m_state.m_powerAlpha * v33);
        const Vector v35 = (m_state.m_power + v34);
        const Vector v36 = m_state.m_stage1.m_k;
        const Vector v37 = m_state.m_stage1.m_a1;
        const Vector v38 = m_state.m_stage1.m_a2;
        const Vector v39 = m_state.m_stage1.m_a3;
        const Vector v40 = v9;
        const Vector v41 = v11;
        const Vector v42 = v4;
        const Vector v43 = v2;
        const Vector v44 = v7;
        const Vector v45 = v12;
        const Vector v46 = v15;
        const Vector v47 = m_state.m_stage2.m_k;
        const Vector v48 = m_state.m_stage2.m_a1;
        const Vector v49 = m_state.m_stage2.m_a2;
        const Vector v50 = m_state.m_stage2.m_a3;
        const Vector v51 = v25;
        const Vector v52 = v27;
        const Vector v53 = v20;
        const Vector v54 = v18;
        const Vector v55 = v23;
        const Vector v56 = v28;
        const Vector v57 = v31;
        const Vector v58 = v35;
        const Vector v59 = m_state.m_powerAlpha;
        m_state.m_stage1.m_k = v36;
        m_state.m_stage1.m_a1 = v37;
        m_state.m_stage1.m_a2 = v38;
        m_state.m_stage1.m_a3 = v39;
        m_state.m_stage1.m_ic1 = v40;
        m_state.m_stage1.m_ic2 = v41;
        m_state.m_stage1.m_lowPass = v42;
        m_state.m_stage1.m_bandPass = v43;
        m_state.m_stage1.m_highPass = v44;
        m_state.m_stage1.m_notch = v45;
        m_state.m_stage1.m_allPass = v46;
        m_state.m_stage2.m_k = v47;
        m_state.m_stage2.m_a1 = v48;
        m_state.m_stage2.m_a2 = v49;
        m_state.m_stage2.m_a3 = v50;
        m_state.m_stage2.m_ic1 = v51;
        m_state.m_stage2.m_ic2 = v52;
        m_state.m_stage2.m_lowPass = v53;
        m_state.m_stage2.m_bandPass = v54;
        m_state.m_stage2.m_highPass = v55;
        m_state.m_stage2.m_notch = v56;
        m_state.m_stage2.m_allPass = v57;
        m_state.m_power = v58;
        m_state.m_powerAlpha = v59;
    }

    // Coefficients generated from DSP.BandPassedEnvelopeFollower.computeCoefficients.
    //

    struct LogicalSpec
    {
        Vector m_integratorGain;
        Vector m_powerAlpha;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = (spec.m_integratorGain + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v1 = Vector::FusedMultiplyAdd(spec.m_integratorGain, v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v2 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v1);
        const Vector v3 = (spec.m_integratorGain * v2);
        const Vector v4 = (spec.m_integratorGain * v3);
        const Vector v5 = (spec.m_integratorGain + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v6 = Vector::FusedMultiplyAdd(spec.m_integratorGain, v5, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v6);
        const Vector v8 = (spec.m_integratorGain * v7);
        const Vector v9 = (spec.m_integratorGain * v8);
        const Vector v10 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0));
        const Vector v11 = v2;
        const Vector v12 = v3;
        const Vector v13 = v4;
        const Vector v14 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0));
        const Vector v15 = v7;
        const Vector v16 = v8;
        const Vector v17 = v9;
        const Vector v18 = spec.m_powerAlpha;
        m_state.m_stage1.m_k = v10;
        m_state.m_stage1.m_a1 = v11;
        m_state.m_stage1.m_a2 = v12;
        m_state.m_stage1.m_a3 = v13;
        m_state.m_stage2.m_k = v14;
        m_state.m_stage2.m_a1 = v15;
        m_state.m_stage2.m_a2 = v16;
        m_state.m_stage2.m_a3 = v17;
        m_state.m_powerAlpha = v18;
    }

    // Reset generated from DSP.BandPassedEnvelopeFollower.reset.
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
        m_state.m_stage1.m_ic1 = v0;
        m_state.m_stage1.m_ic2 = v1;
        m_state.m_stage1.m_lowPass = v2;
        m_state.m_stage1.m_bandPass = v3;
        m_state.m_stage1.m_highPass = v4;
        m_state.m_stage1.m_notch = v5;
        m_state.m_stage1.m_allPass = v6;
        m_state.m_stage2.m_ic1 = v7;
        m_state.m_stage2.m_ic2 = v8;
        m_state.m_stage2.m_lowPass = v9;
        m_state.m_stage2.m_bandPass = v10;
        m_state.m_stage2.m_highPass = v11;
        m_state.m_stage2.m_notch = v12;
        m_state.m_stage2.m_allPass = v13;
        m_state.m_power = v14;
    }

};
