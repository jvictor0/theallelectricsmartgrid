#pragma once

// Generated from DSP.NormalizedTanh.step. Do not edit.
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
struct NormalizedTanhGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            Vector m_gain;
            Vector m_tanhGain;
        } m_coefficients;

        Vector m_output;
    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x * m_state.m_coefficients.m_gain);
        const Vector v1 = (v0 * v0);
        const Vector v2 = (v0 * (v1 + Vector::Splat(27)));
        const Vector v3 = Vector::FusedMultiplyAdd(v0 * Vector::Splat(9), v0, Vector::Splat(27));
        const Vector v4 = (v2 / v3);
        const Vector v5 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v4));
        const Vector v6 = (v5 / m_state.m_coefficients.m_tanhGain);
        const Vector v7 = m_state.m_coefficients.m_gain;
        const Vector v8 = m_state.m_coefficients.m_tanhGain;
        const Vector v9 = v6;
        m_state.m_coefficients.m_gain = v7;
        m_state.m_coefficients.m_tanhGain = v8;
        m_state.m_output = v9;
    }

    // Coefficients generated from DSP.NormalizedTanh.computeCoefficients.
    //

    struct LogicalSpec
    {
        Vector m_gain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = (spec.m_gain * spec.m_gain);
        const Vector v1 = (spec.m_gain * (v0 + Vector::Splat(27)));
        const Vector v2 = Vector::FusedMultiplyAdd(spec.m_gain * Vector::Splat(9), spec.m_gain, Vector::Splat(27));
        const Vector v3 = (v1 / v2);
        const Vector v4 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v3));
        const Vector v5 = spec.m_gain;
        const Vector v6 = v4;
        m_state.m_coefficients.m_gain = v5;
        m_state.m_coefficients.m_tanhGain = v6;
    }

    // Reset generated from DSP.NormalizedTanh.reset.
    //

    void Reset()
    {
        const Vector v0 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        m_state.m_output = v0;
    }

};
