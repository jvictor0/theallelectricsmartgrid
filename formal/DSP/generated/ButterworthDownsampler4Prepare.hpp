#pragma once

// Generated from DSP.ButterworthDownsampler.prepare4Float. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

struct ButterworthDownsampler4Prepare
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Prepared
    {
        struct
        {
            double m_gain;
            double m_feedback1;
            double m_feedback2;
        } m_first;

        struct
        {
            double m_gain;
            double m_feedback1;
            double m_feedback2;
        } m_second;

    };

    static Prepared Prepare(const double& input)
    {
        const double v0 = (0x1921fb54442d18p-51 * input);
        const double v1 = std::tan(v0);
        const double v2 = (v1 * v1);
        const double v3 = (0x10000000000000p-55 * 0x1921fb54442d18p-51);
        const double v4 = std::cos(v3);
        const double v5 = (0x10000000000000p-51 * v4);
        const double v6 = (v5 * v1);
        const double v7 = (1.0 + v6);
        const double v8 = (v7 + v2);
        const double v9 = (v2 / v8);
        const double v10 = (1.0 - v2);
        const double v11 = (0x10000000000000p-51 * v10);
        const double v12 = (v11 / v8);
        const double v13 = (v6 - 1.0);
        const double v14 = (v13 - v2);
        const double v15 = (v14 / v8);
        const double v16 = (v1 * v1);
        const double v17 = (0x18000000000000p-54 * 0x1921fb54442d18p-51);
        const double v18 = std::cos(v17);
        const double v19 = (0x10000000000000p-51 * v18);
        const double v20 = (v19 * v1);
        const double v21 = (1.0 + v20);
        const double v22 = (v21 + v16);
        const double v23 = (v16 / v22);
        const double v24 = (1.0 - v16);
        const double v25 = (0x10000000000000p-51 * v24);
        const double v26 = (v25 / v22);
        const double v27 = (v20 - 1.0);
        const double v28 = (v27 - v16);
        const double v29 = (v28 / v22);
        Prepared result{};
        result.m_first.m_gain = v9;
        result.m_first.m_feedback1 = v12;
        result.m_first.m_feedback2 = v15;
        result.m_second.m_gain = v23;
        result.m_second.m_feedback1 = v26;
        result.m_second.m_feedback2 = v29;
        return result;
    }

};
