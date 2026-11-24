#pragma once

// Generated from DSP.WanderingRandomLFO.wrapFramed32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct WanderingRandomLFOWrapFramed32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State>
    [[gnu::noinline]] static void Process(State& state)
    {
        const std::uint64_t v0 = (state.m_endpoint.m_seed + static_cast<std::uint64_t>(11400714819323198485ULL));
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
        const double v15 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v16 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v17 = (v16 + state.m_endpoint.m_segment.m_afterNext);
        const double v18 = (v15 / v17);
        const double v19 = (state.m_endpoint.m_segment.m_p2 - state.m_endpoint.m_segment.m_p1);
        const double v20 = (state.m_endpoint.m_segment.m_previous + state.m_endpoint.m_segment.m_width);
        const double v21 = (v20 + state.m_endpoint.m_segment.m_next);
        const double v22 = (v19 / v21);
        const double v23 = (v18 - v22);
        const double v24 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v25 = (v23 / v24);
        const double v26 = (state.m_endpoint.m_segment.m_next * v25);
        const double v27 = (state.m_endpoint.m_segment.m_next * v26);
        const double v34 = (v14 - state.m_endpoint.m_segment.m_p3);
        const double v35 = (state.m_endpoint.m_segment.m_next + state.m_endpoint.m_segment.m_afterNext);
        const double v36 = (v35 + 1.0);
        const double v37 = (v34 / v36);
        const double v38 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v39 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v40 = (v39 + state.m_endpoint.m_segment.m_afterNext);
        const double v41 = (v38 / v40);
        const double v42 = (v37 - v41);
        const double v43 = (state.m_endpoint.m_segment.m_next + state.m_endpoint.m_segment.m_afterNext);
        const double v44 = (v42 / v43);
        const double v45 = (state.m_endpoint.m_segment.m_next * v44);
        const double v46 = (state.m_endpoint.m_segment.m_next * v45);
        const double v47 = (v46 - v27);
        const float v48 = static_cast<float>(v47);
        const double v49 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v50 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v51 = (v50 + state.m_endpoint.m_segment.m_afterNext);
        const double v52 = (v49 / v51);
        const double v53 = (state.m_endpoint.m_segment.m_p2 - state.m_endpoint.m_segment.m_p1);
        const double v54 = (state.m_endpoint.m_segment.m_previous + state.m_endpoint.m_segment.m_width);
        const double v55 = (v54 + state.m_endpoint.m_segment.m_next);
        const double v56 = (v53 / v55);
        const double v57 = (v52 - v56);
        const double v58 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v59 = (v57 / v58);
        const double v60 = (state.m_endpoint.m_segment.m_next * v59);
        const double v61 = (state.m_endpoint.m_segment.m_next * v60);
        const double v68 = (0x18000000000000p-51 * v61);
        const float v69 = static_cast<float>(v68);
        const double v70 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v71 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v72 = (v71 + state.m_endpoint.m_segment.m_afterNext);
        const double v73 = (v70 / v72);
        const double v74 = (state.m_endpoint.m_segment.m_p2 - state.m_endpoint.m_segment.m_p1);
        const double v75 = (state.m_endpoint.m_segment.m_previous + state.m_endpoint.m_segment.m_width);
        const double v76 = (v75 + state.m_endpoint.m_segment.m_next);
        const double v77 = (v74 / v76);
        const double v78 = (v73 - v77);
        const double v79 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v80 = (v78 / v79);
        const double v81 = (state.m_endpoint.m_segment.m_next * v80);
        const double v82 = (state.m_endpoint.m_segment.m_next * v81);
        const double v83 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v84 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v85 = (v84 + state.m_endpoint.m_segment.m_afterNext);
        const double v86 = (v83 / v85);
        const double v87 = (state.m_endpoint.m_segment.m_next * v86);
        const double v88 = (v87 - v82);
        const double v89 = (0x18000000000000p-51 * v88);
        const float v90 = static_cast<float>(v89);
        const double v91 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v92 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v93 = (v92 + state.m_endpoint.m_segment.m_afterNext);
        const double v94 = (v91 / v93);
        const double v95 = (state.m_endpoint.m_segment.m_p2 - state.m_endpoint.m_segment.m_p1);
        const double v96 = (state.m_endpoint.m_segment.m_previous + state.m_endpoint.m_segment.m_width);
        const double v97 = (v96 + state.m_endpoint.m_segment.m_next);
        const double v98 = (v95 / v97);
        const double v99 = (v94 - v98);
        const double v100 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v101 = (v99 / v100);
        const double v102 = (state.m_endpoint.m_segment.m_next * v101);
        const double v103 = (state.m_endpoint.m_segment.m_next * v102);
        const double v104 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v105 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v106 = (v105 + state.m_endpoint.m_segment.m_afterNext);
        const double v107 = (v104 / v106);
        const double v108 = (state.m_endpoint.m_segment.m_next * v107);
        const double v109 = (v108 - v103);
        const double v110 = (state.m_endpoint.m_segment.m_p3 - state.m_endpoint.m_segment.m_p2);
        const double v111 = (state.m_endpoint.m_segment.m_width + state.m_endpoint.m_segment.m_next);
        const double v112 = (v111 + state.m_endpoint.m_segment.m_afterNext);
        const double v113 = (v110 / v112);
        const double v114 = (state.m_endpoint.m_segment.m_p2 - v109);
        const double v115 = std::fma(state.m_endpoint.m_segment.m_width, v113, v114);
        const float v116 = static_cast<float>(v115);
        const double v117 = (state.m_endpoint.m_phase - 1.0);
        const double v118 = state.m_endpoint.m_segment.m_p1;
        const double v119 = state.m_endpoint.m_segment.m_p2;
        const double v120 = state.m_endpoint.m_segment.m_p3;
        const double v121 = state.m_endpoint.m_segment.m_previous;
        const double v122 = state.m_endpoint.m_segment.m_width;
        const double v123 = state.m_endpoint.m_segment.m_next;
        const double v124 = state.m_endpoint.m_segment.m_afterNext;
        const double v125 = 1.0;
        state.m_endpoint.m_segment.m_p0 = v118;
        state.m_endpoint.m_segment.m_p1 = v119;
        state.m_endpoint.m_segment.m_p2 = v120;
        state.m_endpoint.m_segment.m_p3 = v14;
        state.m_endpoint.m_segment.m_beforePrevious = v121;
        state.m_endpoint.m_segment.m_previous = v122;
        state.m_endpoint.m_segment.m_width = v123;
        state.m_endpoint.m_segment.m_next = v124;
        state.m_endpoint.m_segment.m_afterNext = v125;
        state.m_endpoint.m_cubic.m_a = v48;
        state.m_endpoint.m_cubic.m_b = v69;
        state.m_endpoint.m_cubic.m_c = v90;
        state.m_endpoint.m_cubic.m_d = v116;
        state.m_endpoint.m_phase = v117;
        state.m_endpoint.m_seed = v0;
    }

};
