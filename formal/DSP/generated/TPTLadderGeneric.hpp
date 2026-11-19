#pragma once

// Generated from DSP.TPTLadder.Runtime.process. Do not edit.
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
struct TPTLadderGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_k;
        struct
        {
            Vector m_G;
            Vector m_s;
        } m_stage1;

        struct
        {
            Vector m_G;
            Vector m_s;
        } m_stage2;

        struct
        {
            Vector m_G;
            Vector m_s;
        } m_stage3;

        struct
        {
            Vector m_G;
            Vector m_s;
        } m_stage4;

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
        const Vector v0 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) - m_state.m_stage1.m_s);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, m_state.m_stage1.m_G, m_state.m_stage1.m_s);
        const Vector v2 = (v1 - m_state.m_stage2.m_s);
        const Vector v3 = Vector::FusedMultiplyAdd(v2, m_state.m_stage2.m_G, m_state.m_stage2.m_s);
        const Vector v4 = (v3 - m_state.m_stage3.m_s);
        const Vector v5 = Vector::FusedMultiplyAdd(v4, m_state.m_stage3.m_G, m_state.m_stage3.m_s);
        const Vector v6 = (v5 - m_state.m_stage4.m_s);
        const Vector v7 = Vector::FusedMultiplyAdd(v6, m_state.m_stage4.m_G, m_state.m_stage4.m_s);
        const Vector v8 = (m_state.m_stage1.m_G * m_state.m_stage2.m_G);
        const Vector v9 = (v8 * m_state.m_stage3.m_G);
        const Vector v10 = (v9 * m_state.m_stage4.m_G);
        const Vector v11 = (-m_state.m_k);
        const Vector v12 = Vector::FusedMultiplyAdd(v11, v7, input.m_x);
        const Vector v13 = Vector::FusedMultiplyAdd(m_state.m_k, v10, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v14 = (v12 / v13);
        const Vector v15 = (v14 - m_state.m_stage1.m_s);
        const Vector v16 = Vector::FusedMultiplyAdd(v15, m_state.m_stage1.m_G, m_state.m_stage1.m_s);
        const Vector v17 = Vector::FusedMultiplyAdd(v15, m_state.m_stage1.m_G, v16);
        m_state.m_stage1.m_s = v17;
        const Vector v18 = (v16 - m_state.m_stage2.m_s);
        const Vector v19 = Vector::FusedMultiplyAdd(v18, m_state.m_stage2.m_G, m_state.m_stage2.m_s);
        const Vector v20 = Vector::FusedMultiplyAdd(v18, m_state.m_stage2.m_G, v19);
        m_state.m_stage2.m_s = v20;
        const Vector v21 = (v19 - m_state.m_stage3.m_s);
        const Vector v22 = Vector::FusedMultiplyAdd(v21, m_state.m_stage3.m_G, m_state.m_stage3.m_s);
        const Vector v23 = Vector::FusedMultiplyAdd(v21, m_state.m_stage3.m_G, v22);
        m_state.m_stage3.m_s = v23;
        const Vector v24 = (v22 - m_state.m_stage4.m_s);
        const Vector v25 = Vector::FusedMultiplyAdd(v24, m_state.m_stage4.m_G, m_state.m_stage4.m_s);
        const Vector v26 = Vector::FusedMultiplyAdd(v24, m_state.m_stage4.m_G, v25);
        m_state.m_stage4.m_s = v26;
        m_output.m_x = v25;
    }

    // Response generated from DSP.TPTLadder.Snapshot.magnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        Vector m_g1;
        Vector m_g2;
        Vector m_g3;
        Vector m_g4;
        Vector m_k;
    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g1 * input.m_cosHalf);
        const Vector v1 = (snapshot.m_g2 * input.m_cosHalf);
        const Vector v2 = (snapshot.m_g3 * input.m_cosHalf);
        const Vector v3 = (snapshot.m_g4 * input.m_cosHalf);
        const Vector v4 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v5 = (-v4);
        const Vector v6 = Vector::FusedMultiplyAdd(v0, v1, v5);
        const Vector v7 = (input.m_sinHalf * v1);
        const Vector v8 = Vector::FusedMultiplyAdd(v0, input.m_sinHalf, v7);
        const Vector v9 = (v8 * input.m_sinHalf);
        const Vector v10 = (-v9);
        const Vector v11 = Vector::FusedMultiplyAdd(v6, v2, v10);
        const Vector v12 = (v8 * v2);
        const Vector v13 = Vector::FusedMultiplyAdd(v6, input.m_sinHalf, v12);
        const Vector v14 = (v0 * v1);
        const Vector v15 = (v14 * v2);
        const Vector v16 = (v15 * v3);
        const Vector v17 = (v13 * input.m_sinHalf);
        const Vector v18 = (-v17);
        const Vector v19 = Vector::FusedMultiplyAdd(v11, v3, v18);
        const Vector v20 = Vector::FusedMultiplyAdd(snapshot.m_k, v16, v19);
        const Vector v21 = (v13 * v3);
        const Vector v22 = Vector::FusedMultiplyAdd(v11, input.m_sinHalf, v21);
        const Vector v23 = (v16 * v16);
        const Vector v24 = (v16 * v16);
        const Vector v25 = (v22 * v22);
        const Vector v26 = Vector::FusedMultiplyAdd(v20, v20, v25);
        const Vector v27 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v24, v26, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v28 = (v23 / v27);
        return v28;
    }

    // Coefficients generated from DSP.TPTLadder.Runtime.prepareCoefficients.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        Vector m_cutoff;
        Vector m_feedback;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_cutoff);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v0);
        const Vector v2 = (v0 / v1);
        const Vector v3 = Math4096::TanPi(spec.m_cutoff);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v3);
        const Vector v5 = (v3 / v4);
        const Vector v6 = Math4096::TanPi(spec.m_cutoff);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v6);
        const Vector v8 = (v6 / v7);
        const Vector v9 = Math4096::TanPi(spec.m_cutoff);
        const Vector v10 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v9);
        const Vector v11 = (v9 / v10);
        const Vector v12 = spec.m_feedback;
        const Vector v13 = spec.m_feedback;
        m_state.m_k = v13;
        m_state.m_stage1.m_G = v2;
        m_state.m_stage2.m_G = v5;
        m_state.m_stage3.m_G = v8;
        m_state.m_stage4.m_G = v11;
        m_responseSnapshot.m_g1 = v0;
        m_responseSnapshot.m_g2 = v3;
        m_responseSnapshot.m_g3 = v6;
        m_responseSnapshot.m_g4 = v9;
        m_responseSnapshot.m_k = v12;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTLadderGeneric<Number, Size>, &TPTLadderGeneric<Number, Size>::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
