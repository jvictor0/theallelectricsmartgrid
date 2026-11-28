#pragma once

// Generated from DSP.SampleRateReducer.process. Do not edit.
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

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct SampleRateReducerCoreGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_phase;
        Vector m_output;
    };

    struct Input
    {
        Vector m_freq;
        Vector m_sample;
    };

    template<typename State, typename InputView>
    static Vector Process(State& state, [[maybe_unused]] const InputView& input)
    {
        const Vector v0 = Vector::IfLess(input.m_freq, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), input.m_freq, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v1 = (state.m_phase + v0);
        const Vector v2 = Vector::IfLess(v1, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), state.m_output, input.m_sample);
        const Vector v3 = v1.Floor();
        const Vector v4 = (v1 - v3);
        const Vector v5 = Vector::IfLess(v1, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v1, v4);
        const Vector v6 = Vector::IfLess(input.m_freq, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v2, input.m_sample);
        state.m_phase = v5;
        state.m_output = v2;
        return v6;
    }

};
