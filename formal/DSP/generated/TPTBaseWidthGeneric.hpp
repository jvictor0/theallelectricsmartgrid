#pragma once

// Generated from DSP.TPTBaseWidth.step. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "ResponseFrequency.hpp"

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct TPTBaseWidthGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            struct
            {
                Vector m_G;
                Vector m_s;
                Vector m_y;
            } m_lowPass;

            Vector m_y;
        } m_highPass;

        struct
        {
            Vector m_G;
            Vector m_s;
            Vector m_y;
        } m_lowPass;

    };

    struct Input
    {
        Vector m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const Vector v0 = (input.m_x - m_state.m_highPass.m_lowPass.m_s);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, m_state.m_highPass.m_lowPass.m_G, m_state.m_highPass.m_lowPass.m_s);
        const Vector v2 = Vector::FusedMultiplyAdd(v0, m_state.m_highPass.m_lowPass.m_G, v1);
        const Vector v3 = (input.m_x - v1);
        const Vector v4 = (v3 - m_state.m_lowPass.m_s);
        const Vector v5 = Vector::FusedMultiplyAdd(v4, m_state.m_lowPass.m_G, m_state.m_lowPass.m_s);
        const Vector v6 = Vector::FusedMultiplyAdd(v4, m_state.m_lowPass.m_G, v5);
        const Vector v7 = m_state.m_highPass.m_lowPass.m_G;
        const Vector v8 = v2;
        const Vector v9 = v1;
        const Vector v10 = v3;
        const Vector v11 = m_state.m_lowPass.m_G;
        const Vector v12 = v6;
        const Vector v13 = v5;
        m_state.m_highPass.m_lowPass.m_G = v7;
        m_state.m_highPass.m_lowPass.m_s = v8;
        m_state.m_highPass.m_lowPass.m_y = v9;
        m_state.m_highPass.m_y = v10;
        m_state.m_lowPass.m_G = v11;
        m_state.m_lowPass.m_s = v12;
        m_state.m_lowPass.m_y = v13;
    }

    // Response generated from DSP.TPTBaseWidth.Snapshot.magnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        struct
        {
            Vector m_g;
        } m_highPass;

        struct
        {
            Vector m_g;
        } m_lowPass;

    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v1 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v2 = (snapshot.m_highPass.m_g * input.m_cosHalf);
        const Vector v3 = (snapshot.m_highPass.m_g * input.m_cosHalf);
        const Vector v4 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v5 = Vector::FusedMultiplyAdd(v2, v3, v4);
        const Vector v6 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v1, v5, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v7 = (v0 / v6);
        const Vector v8 = (snapshot.m_lowPass.m_g * input.m_cosHalf);
        const Vector v9 = (v8 * v8);
        const Vector v10 = (v8 * v8);
        const Vector v11 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v12 = Vector::FusedMultiplyAdd(v8, v8, v11);
        const Vector v13 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v10, v12, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v14 = (v9 / v13);
        const Vector v15 = (v7 * v14);
        return v15;
    }

};
