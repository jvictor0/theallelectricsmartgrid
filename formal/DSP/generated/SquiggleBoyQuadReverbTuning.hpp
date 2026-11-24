#pragma once

// Generated from DSP.SquiggleBoyQuadReverb.Runtime.tuneFramed. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "Math.hpp"

#include "CircularBufferGeneric.hpp"
#include "WanderingRandomLFO.hpp"
#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct SquiggleBoyQuadReverbTuning
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Input
    {
        Vector m_delaySamples;
        Vector m_feedback;
        Vector m_dampingBase;
        Vector m_dampingWidth;
        Vector m_saturatorGain;
    };

    template<typename State, typename InputView>
    static void Process(State& state, [[maybe_unused]] const InputView& input)
    {
        const Vector v0 = input.m_delaySamples;
        const Vector v1 = input.m_feedback;
        const Vector v2 = input.m_saturatorGain;
        state.m_settings.m_delaySamples = v0;
        state.m_settings.m_feedback = v1;
        state.m_settings.m_saturatorGain = v2;
        const Vector v3 = Vector::IfLess(input.m_dampingBase, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), input.m_dampingBase, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v4 = Math4096::TanPi(v3);
        const Vector v5 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v4);
        const Vector v6 = (v4 / v5);
        const Vector v7 = (input.m_dampingBase * input.m_dampingWidth);
        const Vector v8 = (input.m_dampingBase * input.m_dampingWidth);
        const Vector v9 = Vector::IfLess(v7, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), v8, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v10 = Math4096::TanPi(v9);
        const Vector v11 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v10);
        const Vector v12 = (v10 / v11);
        state.m_damping.m_filter.m_highPass.m_lowPass.m_G = v6;
        state.m_damping.m_filter.m_lowPass.m_G = v12;
        state.m_damping.m_snapshot.m_highPass.m_g = v4;
        state.m_damping.m_snapshot.m_lowPass.m_g = v10;
    }

};
