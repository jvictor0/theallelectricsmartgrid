#pragma once

// Generated from DSP.SmoothRandomLFO.evaluate. Do not edit.
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
struct SmoothRandomLFOCubicGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Snapshot
    {
        Vector m_a;
        Vector m_b;
        Vector m_c;
        Vector m_d;
    };

    static Vector Evaluate(const Snapshot& snapshot, const Vector& input)
    {
        const Vector v0 = Vector::FusedMultiplyAdd(snapshot.m_a, input, snapshot.m_b);
        const Vector v1 = Vector::FusedMultiplyAdd(v0, input, snapshot.m_c);
        const Vector v2 = Vector::FusedMultiplyAdd(v1, input, snapshot.m_d);
        return v2;
    }

};
