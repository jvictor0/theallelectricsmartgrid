#pragma once

// Generated from DSP.TPTSVF.step. Do not edit.
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
struct TPTSVFGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
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
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_ic2);
        const Vector v1 = (m_state.m_a1 * m_state.m_ic1);
        const Vector v2 = Vector::FusedMultiplyAdd(m_state.m_a2, v0, v1);
        const Vector v3 = Vector::FusedMultiplyAdd(m_state.m_a2, m_state.m_ic1, m_state.m_ic2);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_a3, v0, v3);
        const Vector v5 = (-m_state.m_k);
        const Vector v6 = Vector::FusedMultiplyAdd(v5, v2, input.m_x);
        const Vector v7 = (v6 - v4);
        const Vector v8 = (-m_state.m_ic1);
        const Vector v9 = Vector::FusedMultiplyAdd(v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v8);
        m_state.m_ic1 = v9;
        const Vector v10 = (-m_state.m_ic2);
        const Vector v11 = Vector::FusedMultiplyAdd(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v10);
        m_state.m_ic2 = v11;
        const Vector v12 = (v4 + v7);
        m_state.m_lowPass = v4;
        m_state.m_highPass = v7;
        m_state.m_notch = v12;
        const Vector v13 = (-m_state.m_k);
        const Vector v14 = (v13 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v15 = Vector::FusedMultiplyAdd(v14, v2, input.m_x);
        m_state.m_bandPass = v2;
        m_state.m_allPass = v15;
    }

    // Response generated from DSP.TPTSVF.Snapshot.lowPassMagnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        Vector m_g;
        Vector m_k;
    };

    static Vector LowPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.bandPassMagnitudeSquared.
    //

    static Vector BandPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.highPassMagnitudeSquared.
    //

    static Vector HighPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.notchMagnitudeSquared.
    //

    static Vector NotchMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) * v3);
        const Vector v8 = (v1 - v2);
        const Vector v9 = (snapshot.m_k * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.allPassMagnitudeSquared.
    //

    static Vector AllPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v3 = (v0 * input.m_sinHalf);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v1);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) * v2);
        const Vector v6 = (v4 - v5);
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)) - snapshot.m_k);
        const Vector v8 = (v7 * v3);
        const Vector v9 = (v1 - v2);
        const Vector v10 = (snapshot.m_k * v3);
        const Vector v11 = (v8 * v8);
        const Vector v12 = Vector::FusedMultiplyAdd(v6, v6, v11);
        const Vector v13 = (v8 * v8);
        const Vector v14 = Vector::FusedMultiplyAdd(v6, v6, v13);
        const Vector v15 = (v10 * v10);
        const Vector v16 = Vector::FusedMultiplyAdd(v9, v9, v15);
        const Vector v17 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v14, v16, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v18 = (v12 / v17);
        return v18;
    }

    // Coefficients generated from DSP.TPTSVF.prepareCoefficients.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        Vector m_cutoff;
        Vector m_q;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_cutoff);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / spec.m_q);
        const Vector v2 = (v0 + v1);
        const Vector v3 = Vector::FusedMultiplyAdd(v0, v2, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v3);
        const Vector v5 = (v0 * v4);
        const Vector v6 = (v0 * v5);
        m_state.m_k = v1;
        m_state.m_a1 = v4;
        m_state.m_a2 = v5;
        m_state.m_a3 = v6;
        m_responseSnapshot.m_g = v0;
        m_responseSnapshot.m_k = v1;
    }

    // Prepared coefficients generated from DSP.TPTSVF.prepareFromSnapshot.
    //

    void ComputeCoefficientsFromSnapshot(const Snapshot& spec)
    {
        const Vector v0 = (spec.m_g + spec.m_k);
        const Vector v1 = Vector::FusedMultiplyAdd(spec.m_g, v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v2 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v1);
        const Vector v3 = (spec.m_g * v2);
        const Vector v4 = (spec.m_g * v3);
        const Vector v5 = spec.m_g;
        const Vector v6 = spec.m_k;
        const Vector v7 = spec.m_k;
        m_state.m_k = v7;
        m_state.m_a1 = v2;
        m_state.m_a2 = v3;
        m_state.m_a3 = v4;
        m_responseSnapshot.m_g = v5;
        m_responseSnapshot.m_k = v6;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using LowPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::LowPassMagnitudeSquared>>;

    using BandPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::BandPassMagnitudeSquared>>;

    using HighPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::HighPassMagnitudeSquared>>;

    using NotchUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::NotchMagnitudeSquared>>;

    using AllPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::AllPassMagnitudeSquared>>;

};

template<typename Number, size_t Size>
struct TPTSVFLowPassGeneric : GeneratedFilterView<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::LowPassMagnitudeSquared>
{
};

template<typename Number, size_t Size>
struct TPTSVFBandPassGeneric : GeneratedFilterView<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::BandPassMagnitudeSquared>
{
};

template<typename Number, size_t Size>
struct TPTSVFHighPassGeneric : GeneratedFilterView<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::HighPassMagnitudeSquared>
{
};

template<typename Number, size_t Size>
struct TPTSVFNotchGeneric : GeneratedFilterView<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::NotchMagnitudeSquared>
{
};

template<typename Number, size_t Size>
struct TPTSVFAllPassGeneric : GeneratedFilterView<TPTSVFGeneric<Number, Size>, &TPTSVFGeneric<Number, Size>::AllPassMagnitudeSquared>
{
};
