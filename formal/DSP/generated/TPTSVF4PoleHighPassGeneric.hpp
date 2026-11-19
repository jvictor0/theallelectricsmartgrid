#pragma once

// Generated from DSP.TPTSVF4Pole.Runtime.processHighPass. Do not edit.
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
        } m_stage1;

        struct
        {
            Vector m_k;
            Vector m_a1;
            Vector m_a2;
            Vector m_a3;
            Vector m_ic1;
            Vector m_ic2;
        } m_stage2;

    };

    struct Input
    {
        Vector m_x;
    };

    struct Output
    {
        Vector m_x;
    };

    State m_state;
    Output m_output;

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
        m_state.m_stage1.m_ic1 = v9;
        const Vector v10 = (-m_state.m_stage1.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        m_state.m_stage1.m_ic2 = v11;
        const Vector v16 = (v7 - m_state.m_stage2.m_ic2);
        const Vector v17 = (m_state.m_stage2.m_a1 * m_state.m_stage2.m_ic1);
        const Vector v18 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, v16, v17);
        const Vector v19 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a2, m_state.m_stage2.m_ic1, m_state.m_stage2.m_ic2);
        const Vector v20 = Vector::FusedMultiplyAdd(m_state.m_stage2.m_a3, v16, v19);
        const Vector v21 = (-m_state.m_stage2.m_k);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, v18, v7);
        const Vector v23 = (v22 - v20);
        m_output.m_x = v23;
        const Vector v24 = (-m_state.m_stage2.m_ic1);
        const Vector v25 = Vector::FusedMultiplyAdd(v18, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v24);
        m_state.m_stage2.m_ic1 = v25;
        const Vector v26 = (-m_state.m_stage2.m_ic2);
        const Vector v27 = Vector::FusedMultiplyAdd(v20, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v26);
        m_state.m_stage2.m_ic2 = v27;
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

    // Coefficients generated from DSP.TPTSVF4Pole.Runtime.prepareCoefficients.
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
        m_state.m_stage1.m_k = v1;
        m_state.m_stage1.m_a1 = v4;
        m_state.m_stage1.m_a2 = v5;
        m_state.m_stage1.m_a3 = v6;
        m_state.m_stage2.m_k = v8;
        m_state.m_stage2.m_a1 = v11;
        m_state.m_stage2.m_a2 = v12;
        m_state.m_stage2.m_a3 = v13;
        m_responseSnapshot.m_stage1.m_g = v0;
        m_responseSnapshot.m_stage1.m_k = v1;
        m_responseSnapshot.m_stage2.m_g = v7;
        m_responseSnapshot.m_stage2.m_k = v8;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVF4PoleHighPassGeneric<Number, Size>, &TPTSVF4PoleHighPassGeneric<Number, Size>::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
