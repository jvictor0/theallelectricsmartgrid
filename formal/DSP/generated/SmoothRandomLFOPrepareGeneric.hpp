#pragma once

// Generated from DSP.SmoothRandomLFO.prepare. Do not edit.
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
struct SmoothRandomLFOPrepareGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Input
    {
        Vector m_p0;
        Vector m_p1;
        Vector m_p2;
        Vector m_p3;
        Vector m_beforePrevious;
        Vector m_previous;
        Vector m_width;
        Vector m_next;
        Vector m_afterNext;
    };

    struct Prepared
    {
        Vector m_a;
        Vector m_b;
        Vector m_c;
        Vector m_d;
    };

    static Prepared Prepare(const Input& input)
    {
        const Vector v0 = (input.m_p2 - input.m_p1);
        const Vector v1 = (input.m_previous + input.m_width);
        const Vector v2 = (v1 + input.m_next);
        const Vector v3 = (v0 / v2);
        const Vector v4 = (input.m_p1 - input.m_p0);
        const Vector v5 = (input.m_beforePrevious + input.m_previous);
        const Vector v6 = (v5 + input.m_width);
        const Vector v7 = (v4 / v6);
        const Vector v8 = (v3 - v7);
        const Vector v9 = (input.m_previous + input.m_width);
        const Vector v10 = (v8 / v9);
        const Vector v11 = (input.m_width * v10);
        const Vector v12 = (input.m_width * v11);
        const Vector v13 = (input.m_p2 - input.m_p1);
        const Vector v14 = (input.m_previous + input.m_width);
        const Vector v15 = (v14 + input.m_next);
        const Vector v16 = (v13 / v15);
        const Vector v17 = (input.m_width * v16);
        const Vector v18 = (v17 - v12);
        const Vector v19 = (input.m_p3 - input.m_p2);
        const Vector v20 = (input.m_width + input.m_next);
        const Vector v21 = (v20 + input.m_afterNext);
        const Vector v22 = (v19 / v21);
        const Vector v23 = (input.m_p2 - input.m_p1);
        const Vector v24 = (input.m_previous + input.m_width);
        const Vector v25 = (v24 + input.m_next);
        const Vector v26 = (v23 / v25);
        const Vector v27 = (v22 - v26);
        const Vector v28 = (input.m_width + input.m_next);
        const Vector v29 = (v27 / v28);
        const Vector v30 = (input.m_width * v29);
        const Vector v31 = (input.m_width * v30);
        const Vector v32 = (v31 - v12);
        const Vector v33 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)) * v12);
        const Vector v34 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)) * v18);
        const Vector v35 = (input.m_p2 - input.m_p1);
        const Vector v36 = (input.m_previous + input.m_width);
        const Vector v37 = (v36 + input.m_next);
        const Vector v38 = (v35 / v37);
        const Vector v39 = (input.m_p1 - v18);
        const Vector v40 = Vector::FusedMultiplyAdd(input.m_previous, v38, v39);
        Prepared result{};
        result.m_a = v32;
        result.m_b = v33;
        result.m_c = v34;
        result.m_d = v40;
        return result;
    }

};
