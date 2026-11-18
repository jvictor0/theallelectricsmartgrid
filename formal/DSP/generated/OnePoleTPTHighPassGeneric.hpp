#pragma once

// Generated from DSP.OnePoleTPTHighPass.step. Do not edit.
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
struct OnePoleTPTHighPassGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_lowPass;

        Vector m_y;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_lowPass.m_s);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, m_state.m_lowPass.m_G, m_state.m_lowPass.m_s);
        const Vector v2 = Vector::FusedMultiplyAdd(v0, m_state.m_lowPass.m_G, v1);
        const Vector v3 = (input.m_x - v1);
        const Vector v4 = m_state.m_lowPass.m_G;
        const Vector v5 = v2;
        const Vector v6 = v1;
        const Vector v7 = v3;
        m_state.m_lowPass.m_G = v4;
        m_state.m_lowPass.m_s = v5;
        m_state.m_lowPass.m_y = v6;
        m_state.m_y = v7;
    }

    // Response generated from DSP.OnePoleTPTHighPass.Snapshot.magnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        Vector m_g;
    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v1 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v2 = (snapshot.m_g * input.m_cosHalf);
        const Vector v3 = (snapshot.m_g * input.m_cosHalf);
        const Vector v4 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v5 = Vector::FusedMultiplyAdd(v2, v3, v4);
        const Vector v6 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v1, v5, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v7 = (v0 / v6);
        return v7;
    }

    // Coefficients generated from DSP.OnePoleTPTHighPass.prepareCoefficients.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        Vector m_cutoff;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_cutoff);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v0);
        const Vector v2 = (v0 / v1);
        const Vector v3 = v0;
        const Vector v4 = v2;
        m_state.m_lowPass.m_G = v4;
        m_responseSnapshot.m_g = v3;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<OnePoleTPTHighPassGeneric<Number, Size>, &OnePoleTPTHighPassGeneric<Number, Size>::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
