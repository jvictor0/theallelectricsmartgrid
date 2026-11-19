#pragma once

// Generated from DSP.DelayKernels.Runtime.prepareCoefficients. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"

#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct TunedDecayCombTuningGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Input
    {
        Vector m_frequency;
        Vector m_feedbackSign;
        Vector m_log2DecaySeconds;
        Vector m_dampOctaves;
        Vector m_sampleRate;
    };

    struct Prepared
    {
        Vector m_delay;
        Vector m_a;
        Vector m_b;
        Vector m_r;
        Vector m_delayGain;
        Vector m_g;
        Vector m_k;
        Vector m_h;
        Vector m_gain;
        Vector m_sign;
        Vector m_bypass;
    };

    static Prepared Prepare(const Input& input)
    {
        const Vector v0 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v1 = Vector::IfLess(input.m_feedbackSign, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v2 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), input.m_feedbackSign, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v3 = Vector::IfLess(input.m_feedbackSign, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v2);
        const Vector v4 = input.m_log2DecaySeconds.Exp2();
        const Vector v5 = input.m_dampOctaves.Exp2();
        const Vector v6 = (input.m_frequency * v5);
        const Vector v7 = Vector::IfLess(v6, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xfae148p-25f : 0x1f5c28f5c28f5cp-54)), v6, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xfae148p-25f : 0x1f5c28f5c28f5cp-54)));
        const Vector v8 = Math4096::TanPi(input.m_frequency);
        const Vector v9 = Math4096::TanPi(v7);
        const Vector v10 = (v9 / v8);
        const Vector v11 = (v10 * v10);
        const Vector v12 = (v11 - Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v13 = (v12 * v12);
        const Vector v14 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v13);
        const Vector v15 = v14.Sqrt();
        const Vector v16 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v15);
        const Vector v17 = v15.Sqrt();
        const Vector v18 = (v8 * v17);
        const Vector v19 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v16);
        const Vector v20 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v19);
        const Vector v21 = v20.Sqrt();
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v16);
        const Vector v23 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v16);
        const Vector v24 = (v22 * v23);
        const Vector v25 = v24.Sqrt();
        const Vector v26 = (v21 * v18);
        const Vector v27 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v26);
        const Vector v28 = (v18 * v18);
        const Vector v29 = (v27 + v28);
        const Vector v30 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v29);
        const Vector v31 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v32 = v16.Sqrt();
        const Vector v33 = (v21 * v32);
        const Vector v34 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v16);
        const Vector v35 = (Math::ATan2(v33, v34) / Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xC90FDBp-21f : 0x1921FB54442D18p-50)));
        const Vector v36 = (-v35);
        const Vector v37 = Vector::IfLess(v3, v31, v36, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v38 = (v1 + v37);
        const Vector v39 = (v38 / input.m_frequency);
        const Vector v40 = v39.Floor();
        const Vector v41 = (v40 - Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v42 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v41, v41, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v43 = (v39 - v42);
        const Vector v44 = (input.m_frequency * v43);
        const Vector v45 = Math4096::TanPi(v44);
        const Vector v46 = (v8 - v45);
        const Vector v47 = (v8 + v45);
        const Vector v48 = (v46 / v47);
        const Vector v49 = (v48 * v48);
        const Vector v50 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v49);
        const Vector v51 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v50, v50, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)));
        const Vector v52 = v51.Sqrt();
        const Vector v53 = (input.m_sampleRate * v4);
        const Vector v54 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) / v53);
        const Vector v55 = (-(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xdd0c55p-21f : 0x1ba18a998fffa0p-50)) * v54));
        const Vector v56 = v55.Exp();
        const Vector v57 = (v42 / v53);
        const Vector v58 = (-(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xdd0c55p-21f : 0x1ba18a998fffa0p-50)) * v57));
        const Vector v59 = v58.Exp();
        Prepared result{};
        result.m_delay = v42;
        result.m_a = v48;
        result.m_b = v52;
        result.m_r = v56;
        result.m_delayGain = v59;
        result.m_g = v18;
        result.m_k = v21;
        result.m_h = v30;
        result.m_gain = v25;
        result.m_sign = input.m_feedbackSign;
        result.m_bypass = v3;
        return result;
    }

};
