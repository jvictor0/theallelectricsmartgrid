#pragma once

// Generated from DSP.PolyXFader.shapeRamp. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "PolyXFaderTables.hpp"
struct PolyXFaderMorph
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Snapshot
    {
        float m_shape;
    };

    static float Evaluate(const Snapshot& snapshot, const float& input)
    {
        const float v0 = (snapshot.m_shape * 0x800000p-22f);
        const bool v1 = (1.0f < v0);
        float v3;
        if (v1)
        {
            v3 = 1.0f;
        }
        else
        {
            const float v2 = (snapshot.m_shape * 0x800000p-22f);
            v3 = v2;
        }

        const bool v4 = (0.0f < v3);
        float v9;
        if (v4)
        {
            const float v5 = (snapshot.m_shape * 0x800000p-22f);
            const bool v6 = (1.0f < v5);
            float v8;
            if (v6)
            {
                v8 = 1.0f;
            }
            else
            {
                const float v7 = (snapshot.m_shape * 0x800000p-22f);
                v8 = v7;
            }

            v9 = v8;
        }
        else
        {
            v9 = 0.0f;
        }

        const float v10 = (snapshot.m_shape * 0x800000p-22f);
        const float v11 = (v10 - 1.0f);
        const bool v12 = (1.0f < v11);
        float v15;
        if (v12)
        {
            v15 = 1.0f;
        }
        else
        {
            const float v13 = (snapshot.m_shape * 0x800000p-22f);
            const float v14 = (v13 - 1.0f);
            v15 = v14;
        }

        const bool v16 = (0.0f < v15);
        float v23;
        if (v16)
        {
            const float v17 = (snapshot.m_shape * 0x800000p-22f);
            const float v18 = (v17 - 1.0f);
            const bool v19 = (1.0f < v18);
            float v22;
            if (v19)
            {
                v22 = 1.0f;
            }
            else
            {
                const float v20 = (snapshot.m_shape * 0x800000p-22f);
                const float v21 = (v20 - 1.0f);
                v22 = v21;
            }

            v23 = v22;
        }
        else
        {
            v23 = 0.0f;
        }

        const float v24 = (1.0f - v9);
        const float v25 = (v9 - v23);
        const auto v26 = PolyXFaderTables::Evaluate(input);
        const float v27 = (input * v24);
        const float v28 = (v26.first * v25);
        const float v29 = (v27 + v28);
        const float v30 = (v26.second * v23);
        const float v31 = (v29 + v30);
        return v31;
    }

};
