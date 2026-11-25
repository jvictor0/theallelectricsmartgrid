#pragma once

// Generated from DSP.ButterworthDownsampler.prepare2Float. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct ButterworthDownsampler2Prepare
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Prepared
    {
        double m_gain;
        double m_feedback1;
        double m_feedback2;
    };

    static Prepared Prepare(const double& input)
    {
        const double v0 = (0x1921fb54442d18p-51 * input);
        const double v1 = std::tan(v0);
        const double v2 = (0x1921fb54442d18p-51 * input);
        const double v3 = std::tan(v2);
        const double v4 = (v1 * v3);
        const double v5 = (0x10000000000000p-54 * 0x1921fb54442d18p-51);
        const double v6 = std::cos(v5);
        const double v7 = (0x10000000000000p-51 * v6);
        const double v8 = (0x1921fb54442d18p-51 * input);
        const double v9 = std::tan(v8);
        const double v10 = (v7 * v9);
        const double v11 = (1.0 + v10);
        const double v12 = (v11 + v4);
        const double v13 = (v4 / v12);
        const double v14 = (1.0 - v4);
        const double v15 = (0x10000000000000p-51 * v14);
        const double v16 = (v15 / v12);
        const double v17 = (v10 - 1.0);
        const double v18 = (v17 - v4);
        const double v19 = (v18 / v12);
        Prepared result{};
        result.m_gain = v13;
        result.m_feedback1 = v16;
        result.m_feedback2 = v19;
        return result;
    }

};
