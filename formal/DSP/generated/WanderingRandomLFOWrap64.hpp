#pragma once

// Generated from DSP.WanderingRandomLFO.wrap64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct WanderingRandomLFOWrap64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    [[gnu::noinline]] static void Process(State& state)
    {
        const std::uint64_t v0 = (state.m_seed + static_cast<std::uint64_t>(11400714819323198485ULL));
        const std::uint64_t v1 = (v0 >> (static_cast<std::uint64_t>(30ULL) & 63ULL));
        const std::uint64_t v2 = (v0 ^ v1);
        const std::uint64_t v3 = (v2 * static_cast<std::uint64_t>(13787848793156543929ULL));
        const std::uint64_t v4 = (v3 >> (static_cast<std::uint64_t>(27ULL) & 63ULL));
        const std::uint64_t v5 = (v3 ^ v4);
        const std::uint64_t v6 = (v5 * static_cast<std::uint64_t>(10723151780598845931ULL));
        const std::uint64_t v7 = (v6 >> (static_cast<std::uint64_t>(31ULL) & 63ULL));
        const std::uint64_t v8 = (v6 ^ v7);
        const std::uint64_t v9 = (v8 >> (static_cast<std::uint64_t>(12ULL) & 63ULL));
        const double v10 = static_cast<double>(v9);
        const double v11 = (v10 + 0x10000000000000p-53);
        const double v12 = (v11 / 0x10000000000000p0);
        const double v13 = (0x10000000000000p-51 * v12);
        const double v14 = (v13 - 1.0);
        const double v15 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v16 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v17 = (v16 + state.m_segment.m_afterNext);
        const double v18 = (v15 / v17);
        const double v19 = (state.m_segment.m_p2 - state.m_segment.m_p1);
        const double v20 = (state.m_segment.m_previous + state.m_segment.m_width);
        const double v21 = (v20 + state.m_segment.m_next);
        const double v22 = (v19 / v21);
        const double v23 = (v18 - v22);
        const double v24 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v25 = (v23 / v24);
        const double v26 = (state.m_segment.m_next * v25);
        const double v27 = (state.m_segment.m_next * v26);
        const double v34 = (v14 - state.m_segment.m_p3);
        const double v35 = (state.m_segment.m_next + state.m_segment.m_afterNext);
        const double v36 = (v35 + 1.0);
        const double v37 = (v34 / v36);
        const double v38 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v39 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v40 = (v39 + state.m_segment.m_afterNext);
        const double v41 = (v38 / v40);
        const double v42 = (v37 - v41);
        const double v43 = (state.m_segment.m_next + state.m_segment.m_afterNext);
        const double v44 = (v42 / v43);
        const double v45 = (state.m_segment.m_next * v44);
        const double v46 = (state.m_segment.m_next * v45);
        const double v47 = (v46 - v27);
        const double v48 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v49 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v50 = (v49 + state.m_segment.m_afterNext);
        const double v51 = (v48 / v50);
        const double v52 = (state.m_segment.m_p2 - state.m_segment.m_p1);
        const double v53 = (state.m_segment.m_previous + state.m_segment.m_width);
        const double v54 = (v53 + state.m_segment.m_next);
        const double v55 = (v52 / v54);
        const double v56 = (v51 - v55);
        const double v57 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v58 = (v56 / v57);
        const double v59 = (state.m_segment.m_next * v58);
        const double v60 = (state.m_segment.m_next * v59);
        const double v67 = (0x18000000000000p-51 * v60);
        const double v68 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v69 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v70 = (v69 + state.m_segment.m_afterNext);
        const double v71 = (v68 / v70);
        const double v72 = (state.m_segment.m_p2 - state.m_segment.m_p1);
        const double v73 = (state.m_segment.m_previous + state.m_segment.m_width);
        const double v74 = (v73 + state.m_segment.m_next);
        const double v75 = (v72 / v74);
        const double v76 = (v71 - v75);
        const double v77 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v78 = (v76 / v77);
        const double v79 = (state.m_segment.m_next * v78);
        const double v80 = (state.m_segment.m_next * v79);
        const double v81 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v82 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v83 = (v82 + state.m_segment.m_afterNext);
        const double v84 = (v81 / v83);
        const double v85 = (state.m_segment.m_next * v84);
        const double v86 = (v85 - v80);
        const double v87 = (0x18000000000000p-51 * v86);
        const double v88 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v89 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v90 = (v89 + state.m_segment.m_afterNext);
        const double v91 = (v88 / v90);
        const double v92 = (state.m_segment.m_p2 - state.m_segment.m_p1);
        const double v93 = (state.m_segment.m_previous + state.m_segment.m_width);
        const double v94 = (v93 + state.m_segment.m_next);
        const double v95 = (v92 / v94);
        const double v96 = (v91 - v95);
        const double v97 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v98 = (v96 / v97);
        const double v99 = (state.m_segment.m_next * v98);
        const double v100 = (state.m_segment.m_next * v99);
        const double v101 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v102 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v103 = (v102 + state.m_segment.m_afterNext);
        const double v104 = (v101 / v103);
        const double v105 = (state.m_segment.m_next * v104);
        const double v106 = (v105 - v100);
        const double v107 = (state.m_segment.m_p3 - state.m_segment.m_p2);
        const double v108 = (state.m_segment.m_width + state.m_segment.m_next);
        const double v109 = (v108 + state.m_segment.m_afterNext);
        const double v110 = (v107 / v109);
        const double v111 = (state.m_segment.m_p2 - v106);
        const double v112 = std::fma(state.m_segment.m_width, v110, v111);
        const double v113 = (state.m_phase - 1.0);
        const double v114 = state.m_segment.m_p1;
        const double v115 = state.m_segment.m_p2;
        const double v116 = state.m_segment.m_p3;
        const double v117 = state.m_segment.m_previous;
        const double v118 = state.m_segment.m_width;
        const double v119 = state.m_segment.m_next;
        const double v120 = state.m_segment.m_afterNext;
        const double v121 = 1.0;
        state.m_segment.m_p0 = v114;
        state.m_segment.m_p1 = v115;
        state.m_segment.m_p2 = v116;
        state.m_segment.m_p3 = v14;
        state.m_segment.m_beforePrevious = v117;
        state.m_segment.m_previous = v118;
        state.m_segment.m_width = v119;
        state.m_segment.m_next = v120;
        state.m_segment.m_afterNext = v121;
        state.m_cubic.m_a = v47;
        state.m_cubic.m_b = v67;
        state.m_cubic.m_c = v87;
        state.m_cubic.m_d = v112;
        state.m_phase = v113;
        state.m_seed = v0;
    }

};
