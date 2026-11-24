#pragma once

// Generated from DSP.SquiggleBoyQuadReverb.Runtime.initializeSample. Do not edit.
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
struct SquiggleBoyQuadReverbInitEverySample
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
        state.m_diffusion.m_fst.m_history.Clear();
        state.m_diffusion.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(1ULL));
        state.m_diffusion.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(2ULL));
        state.m_diffusion.m_snd.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(3ULL));
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(4ULL));
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(5ULL));
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(6ULL));
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(7ULL));
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(8ULL));
        state.m_delay.Clear();
        state.m_first.m_history.Clear();
        state.m_first.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(9ULL));
        state.m_second.m_history.Clear();
        state.m_second.m_clock.Initialize(0x1d4c0000000000p-32, static_cast<std::uint64_t>(10ULL));
        const Vector v0 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        state.m_wet = v0;
        const Vector v1 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v2 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v3 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v4 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v5 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v6 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        state.m_damping.m_filter.m_highPass.m_lowPass.m_G = v1;
        state.m_damping.m_filter.m_highPass.m_lowPass.m_s = v2;
        state.m_damping.m_filter.m_lowPass.m_G = v3;
        state.m_damping.m_filter.m_lowPass.m_s = v4;
        state.m_damping.m_snapshot.m_highPass.m_g = v5;
        state.m_damping.m_snapshot.m_lowPass.m_g = v6;
        const Vector v7 = input.m_delaySamples;
        const Vector v8 = input.m_feedback;
        const Vector v9 = input.m_saturatorGain;
        state.m_settings.m_delaySamples = v7;
        state.m_settings.m_feedback = v8;
        state.m_settings.m_saturatorGain = v9;
        const Vector v10 = Vector::IfLess(input.m_dampingBase, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), input.m_dampingBase, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v11 = Math4096::TanPi(v10);
        const Vector v12 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v11);
        const Vector v13 = (v11 / v12);
        const Vector v14 = (input.m_dampingBase * input.m_dampingWidth);
        const Vector v15 = (input.m_dampingBase * input.m_dampingWidth);
        const Vector v16 = Vector::IfLess(v14, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), v15, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v17 = Math4096::TanPi(v16);
        const Vector v18 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v17);
        const Vector v19 = (v17 / v18);
        state.m_damping.m_filter.m_highPass.m_lowPass.m_G = v13;
        state.m_damping.m_filter.m_lowPass.m_G = v19;
        state.m_damping.m_snapshot.m_highPass.m_g = v11;
        state.m_damping.m_snapshot.m_lowPass.m_g = v17;
    }

};
