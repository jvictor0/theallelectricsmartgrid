#pragma once

// Generated from DSP.MorphingTPTSVF.step. Do not edit.
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
struct MorphingTPTSVFGeneric
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
        } m_svf;

        Vector m_cosine;
        Vector m_sine;
        Vector m_blend;
        Vector m_output;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_svf.m_ic2);
        const Vector v1 = (m_state.m_svf.m_a1 * m_state.m_svf.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_svf.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_svf.m_a2, m_state.m_svf.m_ic1, m_state.m_svf.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_svf.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_svf.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_svf.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        const Vector v10 = (-m_state.m_svf.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        const Vector v12 = (v4 + v7);
        const Vector v13 = (-m_state.m_svf.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        const Vector v16 = (v4 * m_state.m_cosine);
        const Vector v17 = Vector::FusedMultiplyAdd(v2, m_state.m_sine, v16);
        const Vector v18 = (v2 * m_state.m_cosine);
        const Vector v19 = Vector::FusedMultiplyAdd(v7, m_state.m_sine, v18);
        const Vector v20 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)), m_state.m_blend, v19, v17);
        const Vector v21 = m_state.m_svf.m_k;
        const Vector v22 = m_state.m_svf.m_a1;
        const Vector v23 = m_state.m_svf.m_a2;
        const Vector v24 = m_state.m_svf.m_a3;
        const Vector v25 = v9;
        const Vector v26 = v11;
        const Vector v27 = v4;
        const Vector v28 = v2;
        const Vector v29 = v7;
        const Vector v30 = v12;
        const Vector v31 = v15;
        const Vector v32 = m_state.m_cosine;
        const Vector v33 = m_state.m_sine;
        const Vector v34 = m_state.m_blend;
        const Vector v35 = v20;
        m_state.m_svf.m_k = v21;
        m_state.m_svf.m_a1 = v22;
        m_state.m_svf.m_a2 = v23;
        m_state.m_svf.m_a3 = v24;
        m_state.m_svf.m_ic1 = v25;
        m_state.m_svf.m_ic2 = v26;
        m_state.m_svf.m_lowPass = v27;
        m_state.m_svf.m_bandPass = v28;
        m_state.m_svf.m_highPass = v29;
        m_state.m_svf.m_notch = v30;
        m_state.m_svf.m_allPass = v31;
        m_state.m_cosine = v32;
        m_state.m_sine = v33;
        m_state.m_blend = v34;
        m_state.m_output = v35;
    }

    // Response generated from DSP.MorphingTPTSVF.Snapshot.magnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        struct
        {
            Vector m_g;
            Vector m_k;
        } m_svf;

        Vector m_low;
        Vector m_band;
        Vector m_high;
    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_svf.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (snapshot.m_low * v1);
        const Vector v5 = (snapshot.m_high * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (snapshot.m_band * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_svf.m_k * v3);
        const Vector v10 = (v7 * v7);
        const Vector v11 = Vector::FusedMultiplyAdd(v6, v6, v10);
        const Vector v12 = (v7 * v7);
        const Vector v13 = Vector::FusedMultiplyAdd(v6, v6, v12);
        const Vector v14 = (v9 * v9);
        const Vector v15 = Vector::FusedMultiplyAdd(v8, v8, v14);
        const Vector v16 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v13, v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v17 = (v11 / v16);
        return v17;
    }

    // Coefficients generated from DSP.MorphingTPTSVF.prepareCoefficients.
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
        } m_svf;

        Vector m_blend;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_svf.m_cutoff);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / spec.m_svf.m_q);
        const Vector v2 = (v0 + v1);
        const Vector v3 = Vector::FusedMultiplyAdd(v0, v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v3);
        const Vector v5 = (v0 * v4);
        const Vector v6 = (v0 * v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v8 = (spec.m_blend - v7);
        const Vector v9 = (v8 / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v10 = (spec.m_blend / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v11 = Vector::IfLess(v7, spec.m_blend, v9, v10);
        const Vector v12 = Math4096::Cos2pi(v11);
        const Vector v13 = Math4096::Sin2pi(v11);
        const Vector v14 = Vector::IfLess(v7, spec.m_blend, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v12);
        const Vector v15 = Vector::IfLess(v7, spec.m_blend, v12, v13);
        const Vector v16 = Vector::IfLess(v7, spec.m_blend, v13, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v17 = v0;
        const Vector v18 = v1;
        const Vector v19 = v14;
        const Vector v20 = v15;
        const Vector v21 = v16;
        const Vector v22 = v1;
        const Vector v23 = v4;
        const Vector v24 = v5;
        const Vector v25 = v6;
        const Vector v26 = v12;
        const Vector v27 = v13;
        const Vector v28 = spec.m_blend;
        m_state.m_svf.m_k = v22;
        m_state.m_svf.m_a1 = v23;
        m_state.m_svf.m_a2 = v24;
        m_state.m_svf.m_a3 = v25;
        m_state.m_cosine = v26;
        m_state.m_sine = v27;
        m_state.m_blend = v28;
        m_responseSnapshot.m_svf.m_g = v17;
        m_responseSnapshot.m_svf.m_k = v18;
        m_responseSnapshot.m_low = v19;
        m_responseSnapshot.m_band = v20;
        m_responseSnapshot.m_high = v21;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<MorphingTPTSVFGeneric<Number, Size>, &MorphingTPTSVFGeneric<Number, Size>::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
