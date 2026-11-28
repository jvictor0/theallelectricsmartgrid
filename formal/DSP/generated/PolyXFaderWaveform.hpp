#pragma once

// Generated from DSP.PolyXFader.shapeSingle. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "PolyXFaderTables.hpp"
struct PolyXFaderWaveform
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Snapshot
    {
        float m_phaseShift;
        float m_mult;
        float m_attackFrac;
        float m_shape;
    };

    static float Evaluate(const Snapshot& snapshot, const double& input)
    {
        const bool v0 = (0xffbe77p-24f < snapshot.m_attackFrac);
        float v1;
        if (v0)
        {
            v1 = 0xffbe77p-24f;
        }
        else
        {
            v1 = snapshot.m_attackFrac;
        }

        const bool v2 = (0x83126fp-33f < v1);
        float v5;
        if (v2)
        {
            const bool v3 = (0xffbe77p-24f < snapshot.m_attackFrac);
            float v4;
            if (v3)
            {
                v4 = 0xffbe77p-24f;
            }
            else
            {
                v4 = snapshot.m_attackFrac;
            }

            v5 = v4;
        }
        else
        {
            v5 = 0x83126fp-33f;
        }

        const double v6 = static_cast<double>(snapshot.m_phaseShift);
        const float v7 = std::floor(snapshot.m_mult);
        const float v8 = (snapshot.m_mult - v7);
        const bool v9 = (0.0f < v8);
        float v10;
        if (v9)
        {
            v10 = v8;
        }
        else
        {
            v10 = 1.0f;
        }

        const float v11 = (1.0f / v10);
        const float v12 = (snapshot.m_shape * 0x800000p-22f);
        const bool v13 = (1.0f < v12);
        float v15;
        if (v13)
        {
            v15 = 1.0f;
        }
        else
        {
            const float v14 = (snapshot.m_shape * 0x800000p-22f);
            v15 = v14;
        }

        const bool v16 = (0.0f < v15);
        float v21;
        if (v16)
        {
            const float v17 = (snapshot.m_shape * 0x800000p-22f);
            const bool v18 = (1.0f < v17);
            float v20;
            if (v18)
            {
                v20 = 1.0f;
            }
            else
            {
                const float v19 = (snapshot.m_shape * 0x800000p-22f);
                v20 = v19;
            }

            v21 = v20;
        }
        else
        {
            v21 = 0.0f;
        }

        const float v22 = (snapshot.m_shape * 0x800000p-22f);
        const float v23 = (v22 - 1.0f);
        const bool v24 = (1.0f < v23);
        float v27;
        if (v24)
        {
            v27 = 1.0f;
        }
        else
        {
            const float v25 = (snapshot.m_shape * 0x800000p-22f);
            const float v26 = (v25 - 1.0f);
            v27 = v26;
        }

        const bool v28 = (0.0f < v27);
        float v35;
        if (v28)
        {
            const float v29 = (snapshot.m_shape * 0x800000p-22f);
            const float v30 = (v29 - 1.0f);
            const bool v31 = (1.0f < v30);
            float v34;
            if (v31)
            {
                v34 = 1.0f;
            }
            else
            {
                const float v32 = (snapshot.m_shape * 0x800000p-22f);
                const float v33 = (v32 - 1.0f);
                v34 = v33;
            }

            v35 = v34;
        }
        else
        {
            v35 = 0.0f;
        }

        const float v36 = (1.0f - v21);
        const float v37 = (v21 - v35);
        const float v38 = (1.0f / v5);
        const float v39 = (1.0f - v5);
        const float v40 = (1.0f / v39);
        const double v41 = (input + v6);
        const double v42 = (v41 + 0x18000000000000p-53);
        const double v43 = (input + v6);
        const double v44 = (v43 + 0x18000000000000p-53);
        const double v45 = std::floor(v44);
        const double v46 = (v42 - v45);
        const float v47 = static_cast<float>(v46);
        const float v48 = (v47 * snapshot.m_mult);
        const bool v49 = (v7 < v48);
        float v54;
        if (v49)
        {
            const float v50 = (v48 - v7);
            const float v51 = (v50 * v11);
            v54 = v51;
        }
        else
        {
            const float v52 = std::floor(v48);
            const float v53 = (v48 - v52);
            v54 = v53;
        }

        const bool v55 = (v7 < v48);
        float v56;
        if (v55)
        {
            v56 = v8;
        }
        else
        {
            v56 = 1.0f;
        }

        const float v57 = (v54 * v38);
        const float v58 = (v54 - v5);
        const float v59 = (v58 * v40);
        const float v60 = (1.0f - v59);
        const bool v61 = (v5 < v54);
        float v62;
        if (v61)
        {
            v62 = v60;
        }
        else
        {
            v62 = v57;
        }

        const auto v63 = PolyXFaderTables::Evaluate(v62);
        const bool v64 = (v5 < v54);
        float v65;
        if (v64)
        {
            v65 = v60;
        }
        else
        {
            v65 = v57;
        }

        const float v66 = (v65 * v36);
        const float v67 = (v63.first * v37);
        const float v68 = (v66 + v67);
        const float v69 = (v63.second * v35);
        const float v70 = (v68 + v69);
        const float v71 = (v56 * v70);
        return v71;
    }

};
