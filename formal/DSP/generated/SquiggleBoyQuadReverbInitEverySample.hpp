#pragma once

// Generated from DSP.SquiggleBoyQuadReverb.Runtime.initializeSample. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
#include "Math.hpp"

#include "CircularBuffer.hpp"
#include "WanderingRandomLFO.hpp"
#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct SquiggleBoyQuadReverbInitEverySample
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct Input
    {
        Vector m_delaySamples;
        Vector m_feedback;
        Vector m_dampingBase;
        Vector m_dampingWidth;
        Vector m_saturatorGain;
    };

    template<typename State, typename InputView>
    static void Process(State& state, [[maybe_unused]] const InputView& input)
    {
        state.m_diffusion.m_fst.m_history.Clear();
        const std::uint64_t v0 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v1 = (static_cast<std::uint64_t>(1ULL) + v0);
        state.m_diffusion.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v1);
        const std::uint64_t v2 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v3 = (static_cast<std::uint64_t>(1ULL) + v2);
        state.m_diffusion.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v3);
        const std::uint64_t v4 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v5 = (static_cast<std::uint64_t>(1ULL) + v4);
        state.m_diffusion.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v5);
        const std::uint64_t v6 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v7 = (static_cast<std::uint64_t>(1ULL) + v6);
        state.m_diffusion.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v7);
        state.m_diffusion.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v8 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v9 = (static_cast<std::uint64_t>(2ULL) + v8);
        state.m_diffusion.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v9);
        const std::uint64_t v10 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v11 = (static_cast<std::uint64_t>(2ULL) + v10);
        state.m_diffusion.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v11);
        const std::uint64_t v12 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v13 = (static_cast<std::uint64_t>(2ULL) + v12);
        state.m_diffusion.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v13);
        const std::uint64_t v14 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v15 = (static_cast<std::uint64_t>(2ULL) + v14);
        state.m_diffusion.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v15);
        state.m_diffusion.m_snd.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v16 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v17 = (static_cast<std::uint64_t>(3ULL) + v16);
        state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v17);
        const std::uint64_t v18 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v19 = (static_cast<std::uint64_t>(3ULL) + v18);
        state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v19);
        const std::uint64_t v20 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v21 = (static_cast<std::uint64_t>(3ULL) + v20);
        state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v21);
        const std::uint64_t v22 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v23 = (static_cast<std::uint64_t>(3ULL) + v22);
        state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v23);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v24 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v25 = (static_cast<std::uint64_t>(4ULL) + v24);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v25);
        const std::uint64_t v26 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v27 = (static_cast<std::uint64_t>(4ULL) + v26);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v27);
        const std::uint64_t v28 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v29 = (static_cast<std::uint64_t>(4ULL) + v28);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v29);
        const std::uint64_t v30 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v31 = (static_cast<std::uint64_t>(4ULL) + v30);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v31);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v32 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v33 = (static_cast<std::uint64_t>(5ULL) + v32);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v33);
        const std::uint64_t v34 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v35 = (static_cast<std::uint64_t>(5ULL) + v34);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v35);
        const std::uint64_t v36 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v37 = (static_cast<std::uint64_t>(5ULL) + v36);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v37);
        const std::uint64_t v38 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v39 = (static_cast<std::uint64_t>(5ULL) + v38);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v39);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v40 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v41 = (static_cast<std::uint64_t>(6ULL) + v40);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v41);
        const std::uint64_t v42 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v43 = (static_cast<std::uint64_t>(6ULL) + v42);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v43);
        const std::uint64_t v44 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v45 = (static_cast<std::uint64_t>(6ULL) + v44);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v45);
        const std::uint64_t v46 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v47 = (static_cast<std::uint64_t>(6ULL) + v46);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v47);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v48 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v49 = (static_cast<std::uint64_t>(7ULL) + v48);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v49);
        const std::uint64_t v50 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v51 = (static_cast<std::uint64_t>(7ULL) + v50);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v51);
        const std::uint64_t v52 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v53 = (static_cast<std::uint64_t>(7ULL) + v52);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v53);
        const std::uint64_t v54 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v55 = (static_cast<std::uint64_t>(7ULL) + v54);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v55);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Clear();
        const std::uint64_t v56 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v57 = (static_cast<std::uint64_t>(8ULL) + v56);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v57);
        const std::uint64_t v58 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v59 = (static_cast<std::uint64_t>(8ULL) + v58);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v59);
        const std::uint64_t v60 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v61 = (static_cast<std::uint64_t>(8ULL) + v60);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v61);
        const std::uint64_t v62 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v63 = (static_cast<std::uint64_t>(8ULL) + v62);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v63);
        state.m_delay.Clear();
        state.m_first.m_history.Clear();
        const std::uint64_t v64 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v65 = (static_cast<std::uint64_t>(9ULL) + v64);
        state.m_first.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v65);
        const std::uint64_t v66 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v67 = (static_cast<std::uint64_t>(9ULL) + v66);
        state.m_first.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v67);
        const std::uint64_t v68 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v69 = (static_cast<std::uint64_t>(9ULL) + v68);
        state.m_first.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v69);
        const std::uint64_t v70 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v71 = (static_cast<std::uint64_t>(9ULL) + v70);
        state.m_first.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v71);
        state.m_second.m_history.Clear();
        const std::uint64_t v72 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(0ULL));
        const std::uint64_t v73 = (static_cast<std::uint64_t>(10ULL) + v72);
        state.m_second.m_clock.m_lane0.Initialize(0x1d4c0000000000p-32, v73);
        const std::uint64_t v74 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(1ULL));
        const std::uint64_t v75 = (static_cast<std::uint64_t>(10ULL) + v74);
        state.m_second.m_clock.m_lane1.Initialize(0x1d4c0000000000p-32, v75);
        const std::uint64_t v76 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(2ULL));
        const std::uint64_t v77 = (static_cast<std::uint64_t>(10ULL) + v76);
        state.m_second.m_clock.m_lane2.Initialize(0x1d4c0000000000p-32, v77);
        const std::uint64_t v78 = (static_cast<std::uint64_t>(16ULL) * static_cast<std::uint64_t>(3ULL));
        const std::uint64_t v79 = (static_cast<std::uint64_t>(10ULL) + v78);
        state.m_second.m_clock.m_lane3.Initialize(0x1d4c0000000000p-32, v79);
        const Vector v80 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        state.m_wet = v80;
        const Vector v81 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v82 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v83 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v84 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v85 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v86 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        state.m_damping.m_filter.m_highPass.m_lowPass.m_G = v81;
        state.m_damping.m_filter.m_highPass.m_lowPass.m_s = v82;
        state.m_damping.m_filter.m_lowPass.m_G = v83;
        state.m_damping.m_filter.m_lowPass.m_s = v84;
        state.m_damping.m_snapshot.m_highPass.m_g = v85;
        state.m_damping.m_snapshot.m_lowPass.m_g = v86;
        const Vector v87 = input.m_delaySamples;
        const Vector v88 = input.m_feedback;
        const Vector v89 = input.m_saturatorGain;
        state.m_settings.m_delaySamples = v87;
        state.m_settings.m_feedback = v88;
        state.m_settings.m_saturatorGain = v89;
        const Vector v90 = Vector::IfLess(input.m_dampingBase, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), input.m_dampingBase, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v91 = Math4096::TanPi(v90);
        const Vector v92 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v91);
        const Vector v93 = (v91 / v92);
        const Vector v94 = (input.m_dampingBase * input.m_dampingWidth);
        const Vector v95 = (input.m_dampingBase * input.m_dampingWidth);
        const Vector v96 = Vector::IfLess(v94, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)), v95, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xff7ceep-25f : 0x1fef9db22d0e56p-54)));
        const Vector v97 = Math4096::TanPi(v96);
        const Vector v98 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v97);
        const Vector v99 = (v97 / v98);
        state.m_damping.m_filter.m_highPass.m_lowPass.m_G = v93;
        state.m_damping.m_filter.m_lowPass.m_G = v99;
        state.m_damping.m_snapshot.m_highPass.m_g = v91;
        state.m_damping.m_snapshot.m_lowPass.m_g = v97;
    }

};
