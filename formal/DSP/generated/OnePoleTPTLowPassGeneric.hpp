#pragma once

// Generated from DSP.OnePoleTPTLowPass.step. Do not edit.
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
struct OnePoleTPTLowPassGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_G;
        Vector m_s;
        Vector m_y;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_s);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, m_state.m_G, m_state.m_s);
        const Vector v2 = Vector::FusedMultiplyAdd(v0, m_state.m_G, v1);
        const Vector v3 = m_state.m_G;
        const Vector v4 = v2;
        const Vector v5 = v1;
        m_state.m_G = v3;
        m_state.m_s = v4;
        m_state.m_y = v5;
    }

    // Coefficients generated from DSP.OnePoleTPTLowPass.computeCoefficients.
    //

    struct LogicalSpec
    {
        Vector m_cutoff;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Math4096::TanPi(spec.m_cutoff);
        const Vector v1 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v0);
        const Vector v2 = (v0 / v1);
        const Vector v3 = v2;
        m_state.m_G = v3;
    }

    // Response generated from DSP.OnePoleTPTLowPass.Snapshot.magnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        Vector m_g;
    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (snapshot.m_g * input.m_cosHalf);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (v0 * v0);
        const Vector v3 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v4 = Vector::FusedMultiplyAdd(v0, v0, v3);
        const Vector v5 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v2, v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v6 = (v1 / v5);
        return v6;
    }

};
