#pragma once

// Generated from DSP.WanderingRandomLFO.initialize64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct WanderingRandomLFOInit64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Input
    {
        double m_lambdaSamples;
        std::uint64_t m_seed;
    };

    template<typename State>
    static void Process(State& state, [[maybe_unused]] const Input& input)
    {
        const bool v0 = (1.0 < input.m_lambdaSamples);
        double v1;
        if (v0)
        {
            v1 = input.m_lambdaSamples;
        }
        else
        {
            v1 = 1.0;
        }

        const std::uint64_t v2 = (input.m_seed ^ static_cast<std::uint64_t>(16053923693885880380ULL));
        const std::uint64_t v3 = (input.m_seed + static_cast<std::uint64_t>(11400714819323198485ULL));
        const std::uint64_t v4 = (v3 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
        const std::uint64_t v5 = (v3 ^ v4);
        const std::uint64_t v6 = (v5 * static_cast<std::uint64_t>(13787848793156543929ULL));
        const std::uint64_t v7 = (v6 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
        const std::uint64_t v8 = (v6 ^ v7);
        const std::uint64_t v9 = (v8 * static_cast<std::uint64_t>(10723151780598845931ULL));
        const std::uint64_t v10 = (v9 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
        const std::uint64_t v11 = (v9 ^ v10);
        const std::uint64_t v12 = (v11 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
        const double v13 = static_cast<double>(v12);
        const double v14 = (v13 + 0x10000000000000p-53);
        const double v15 = (v14 / 0x10000000000000p0);
        const double v16 = (0x10000000000000p-51 * v15);
        const double v17 = (v16 - 1.0);
        const std::uint64_t v18 = (v3 + static_cast<std::uint64_t>(11400714819323198485ULL));
        const std::uint64_t v19 = (v18 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
        const std::uint64_t v20 = (v18 ^ v19);
        const std::uint64_t v21 = (v20 * static_cast<std::uint64_t>(13787848793156543929ULL));
        const std::uint64_t v22 = (v21 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
        const std::uint64_t v23 = (v21 ^ v22);
        const std::uint64_t v24 = (v23 * static_cast<std::uint64_t>(10723151780598845931ULL));
        const std::uint64_t v25 = (v24 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
        const std::uint64_t v26 = (v24 ^ v25);
        const std::uint64_t v27 = (v26 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
        const double v28 = static_cast<double>(v27);
        const double v29 = (v28 + 0x10000000000000p-53);
        const double v30 = (v29 / 0x10000000000000p0);
        const double v31 = (0x10000000000000p-51 * v30);
        const double v32 = (v31 - 1.0);
        const std::uint64_t v33 = (v18 + static_cast<std::uint64_t>(11400714819323198485ULL));
        const std::uint64_t v34 = (v33 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
        const std::uint64_t v35 = (v33 ^ v34);
        const std::uint64_t v36 = (v35 * static_cast<std::uint64_t>(13787848793156543929ULL));
        const std::uint64_t v37 = (v36 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
        const std::uint64_t v38 = (v36 ^ v37);
        const std::uint64_t v39 = (v38 * static_cast<std::uint64_t>(10723151780598845931ULL));
        const std::uint64_t v40 = (v39 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
        const std::uint64_t v41 = (v39 ^ v40);
        const std::uint64_t v42 = (v41 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
        const double v43 = static_cast<double>(v42);
        const double v44 = (v43 + 0x10000000000000p-53);
        const double v45 = (v44 / 0x10000000000000p0);
        const double v46 = (0x10000000000000p-51 * v45);
        const double v47 = (v46 - 1.0);
        const std::uint64_t v48 = (v33 + static_cast<std::uint64_t>(11400714819323198485ULL));
        const std::uint64_t v49 = (v48 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
        const std::uint64_t v50 = (v48 ^ v49);
        const std::uint64_t v51 = (v50 * static_cast<std::uint64_t>(13787848793156543929ULL));
        const std::uint64_t v52 = (v51 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
        const std::uint64_t v53 = (v51 ^ v52);
        const std::uint64_t v54 = (v53 * static_cast<std::uint64_t>(10723151780598845931ULL));
        const std::uint64_t v55 = (v54 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
        const std::uint64_t v56 = (v54 ^ v55);
        const std::uint64_t v57 = (v56 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
        const double v58 = static_cast<double>(v57);
        const double v59 = (v58 + 0x10000000000000p-53);
        const double v60 = (v59 / 0x10000000000000p0);
        const double v61 = (0x10000000000000p-51 * v60);
        const double v62 = (v61 - 1.0);
        const double v63 = (v47 - v32);
        const double v64 = (1.0 + 1.0);
        const double v65 = (v64 + 1.0);
        const double v66 = (v63 / v65);
        const double v67 = (v32 - v17);
        const double v68 = (1.0 + 1.0);
        const double v69 = (v68 + 1.0);
        const double v70 = (v67 / v69);
        const double v71 = (v66 - v70);
        const double v72 = (1.0 + 1.0);
        const double v73 = (v71 / v72);
        const double v74 = (1.0 * v73);
        const double v75 = (1.0 * v74);
        const double v82 = (v62 - v47);
        const double v83 = (1.0 + 1.0);
        const double v84 = (v83 + 1.0);
        const double v85 = (v82 / v84);
        const double v86 = (v47 - v32);
        const double v87 = (1.0 + 1.0);
        const double v88 = (v87 + 1.0);
        const double v89 = (v86 / v88);
        const double v90 = (v85 - v89);
        const double v91 = (1.0 + 1.0);
        const double v92 = (v90 / v91);
        const double v93 = (1.0 * v92);
        const double v94 = (1.0 * v93);
        const double v95 = (v94 - v75);
        const double v96 = (v47 - v32);
        const double v97 = (1.0 + 1.0);
        const double v98 = (v97 + 1.0);
        const double v99 = (v96 / v98);
        const double v100 = (v32 - v17);
        const double v101 = (1.0 + 1.0);
        const double v102 = (v101 + 1.0);
        const double v103 = (v100 / v102);
        const double v104 = (v99 - v103);
        const double v105 = (1.0 + 1.0);
        const double v106 = (v104 / v105);
        const double v107 = (1.0 * v106);
        const double v108 = (1.0 * v107);
        const double v115 = (0x18000000000000p-51 * v108);
        const double v116 = (v47 - v32);
        const double v117 = (1.0 + 1.0);
        const double v118 = (v117 + 1.0);
        const double v119 = (v116 / v118);
        const double v120 = (v32 - v17);
        const double v121 = (1.0 + 1.0);
        const double v122 = (v121 + 1.0);
        const double v123 = (v120 / v122);
        const double v124 = (v119 - v123);
        const double v125 = (1.0 + 1.0);
        const double v126 = (v124 / v125);
        const double v127 = (1.0 * v126);
        const double v128 = (1.0 * v127);
        const double v129 = (v47 - v32);
        const double v130 = (1.0 + 1.0);
        const double v131 = (v130 + 1.0);
        const double v132 = (v129 / v131);
        const double v133 = (1.0 * v132);
        const double v134 = (v133 - v128);
        const double v135 = (0x18000000000000p-51 * v134);
        const double v136 = (v47 - v32);
        const double v137 = (1.0 + 1.0);
        const double v138 = (v137 + 1.0);
        const double v139 = (v136 / v138);
        const double v140 = (v32 - v17);
        const double v141 = (1.0 + 1.0);
        const double v142 = (v141 + 1.0);
        const double v143 = (v140 / v142);
        const double v144 = (v139 - v143);
        const double v145 = (1.0 + 1.0);
        const double v146 = (v144 / v145);
        const double v147 = (1.0 * v146);
        const double v148 = (1.0 * v147);
        const double v149 = (v47 - v32);
        const double v150 = (1.0 + 1.0);
        const double v151 = (v150 + 1.0);
        const double v152 = (v149 / v151);
        const double v153 = (1.0 * v152);
        const double v154 = (v153 - v148);
        const double v155 = (v47 - v32);
        const double v156 = (1.0 + 1.0);
        const double v157 = (v156 + 1.0);
        const double v158 = (v155 / v157);
        const double v159 = (v32 - v154);
        const double v160 = std::fma(1.0, v158, v159);
        const double v161 = (1.0 / v1);
        const double v162 = 1.0;
        const double v163 = 1.0;
        const double v164 = 1.0;
        const double v165 = 1.0;
        const double v166 = 1.0;
        const double v167 = 0.0;
        const size_t v168 = 0;
        state.m_segment.m_p0 = v17;
        state.m_segment.m_p1 = v32;
        state.m_segment.m_p2 = v47;
        state.m_segment.m_p3 = v62;
        state.m_segment.m_beforePrevious = v162;
        state.m_segment.m_previous = v163;
        state.m_segment.m_width = v164;
        state.m_segment.m_next = v165;
        state.m_segment.m_afterNext = v166;
        state.m_cubic.m_a = v95;
        state.m_cubic.m_b = v115;
        state.m_cubic.m_c = v135;
        state.m_cubic.m_d = v160;
        state.m_phase = v167;
        state.m_phaseIncrement = v161;
        state.m_lambdaSamples = v1;
        state.m_frameRemaining = v168;
        state.m_seed = v48;
        state.m_speedSeed = v2;
    }

};
