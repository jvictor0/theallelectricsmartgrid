#pragma once

// Generated from DSP.SquiggleBoyQuadReverb.Runtime.processSample. Do not edit.
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
struct SquiggleBoyQuadReverbEverySample
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            struct
            {
                CircularBufferGeneric<Number, Size, 2048> m_history;
                struct
                {
                    PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                    PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                    PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                    PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                } m_clock;

            } m_fst;

            struct
            {
                struct
                {
                    CircularBufferGeneric<Number, Size, 2048> m_history;
                    struct
                    {
                        PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                        PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                        PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                        PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                    } m_clock;

                } m_fst;

                struct
                {
                    struct
                    {
                        CircularBufferGeneric<Number, Size, 2048> m_history;
                        struct
                        {
                            PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                            PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                            PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                            PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                        } m_clock;

                    } m_fst;

                    struct
                    {
                        struct
                        {
                            CircularBufferGeneric<Number, Size, 2048> m_history;
                            struct
                            {
                                PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                                PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                                PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                                PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                            } m_clock;

                        } m_fst;

                        struct
                        {
                            struct
                            {
                                CircularBufferGeneric<Number, Size, 8192> m_history;
                                struct
                                {
                                    PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                                    PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                                    PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                                    PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                                } m_clock;

                            } m_fst;

                            struct
                            {
                                struct
                                {
                                    CircularBufferGeneric<Number, Size, 8192> m_history;
                                    struct
                                    {
                                        PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                                        PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                                        PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                                        PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                                    } m_clock;

                                } m_fst;

                                struct
                                {
                                    struct
                                    {
                                        CircularBufferGeneric<Number, Size, 16384> m_history;
                                        struct
                                        {
                                            PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                                            PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                                            PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                                            PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                                        } m_clock;

                                    } m_fst;

                                    struct
                                    {
                                        struct
                                        {
                                            CircularBufferGeneric<Number, Size, 16384> m_history;
                                            struct
                                            {
                                                PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                                                PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                                                PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                                                PackedWanderingRandomLFO<Number, Size, false> m_lane3;
                                            } m_clock;

                                        } m_fst;

                                        struct
                                        {
                                        } m_snd;

                                    } m_snd;

                                } m_snd;

                            } m_snd;

                        } m_snd;

                    } m_snd;

                } m_snd;

            } m_snd;

        } m_diffusion;

        CircularBufferGeneric<Number, Size, 262144> m_delay;
        struct
        {
            Vector m_delaySamples;
            Vector m_feedback;
            Vector m_saturatorGain;
        } m_settings;

        struct
        {
            CircularBufferGeneric<Number, Size, 8192> m_history;
            struct
            {
                PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                PackedWanderingRandomLFO<Number, Size, false> m_lane3;
            } m_clock;

        } m_first;

        struct
        {
            struct
            {
                struct
                {
                    struct
                    {
                        Vector m_G;
                        Vector m_s;
                    } m_lowPass;

                } m_highPass;

                struct
                {
                    Vector m_G;
                    Vector m_s;
                } m_lowPass;

            } m_filter;

            struct
            {
                struct
                {
                    Vector m_g;
                } m_highPass;

                struct
                {
                    Vector m_g;
                } m_lowPass;

            } m_snapshot;

        } m_damping;

        struct
        {
            CircularBufferGeneric<Number, Size, 32768> m_history;
            struct
            {
                PackedWanderingRandomLFO<Number, Size, false> m_lane0;
                PackedWanderingRandomLFO<Number, Size, false> m_lane1;
                PackedWanderingRandomLFO<Number, Size, false> m_lane2;
                PackedWanderingRandomLFO<Number, Size, false> m_lane3;
            } m_clock;

        } m_second;

        Vector m_wet;
    };

    struct Input
    {
        Vector m_x;
    };

    template<typename State, typename InputView>
    static Vector Process(State& state, [[maybe_unused]] const InputView& input)
    {
        [[maybe_unused]] const auto v0 = state.m_diffusion.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v1 = state.m_diffusion.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v2 = state.m_diffusion.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v3 = state.m_diffusion.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v4 = Vector{v0[0], v1[1], v2[2], v3[3]};
        const Vector v5 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v7 = Vector::IfLess(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v8 = Vector::IfLess(v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v10 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v7, v8, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v11 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v10);
        const Vector v12 = (v5 * v11);
        const Vector v13 = (-v12);
        const Vector v14 = v13.Floor();
        const Vector v15 = (-v14);
        const Vector v16 = (v15 - v12);
        const std::uint64_t v17 = (static_cast<std::uint64_t>(293ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v18 = Vector::Splat(static_cast<Number>(v17));
        const std::uint64_t v19 = (static_cast<std::uint64_t>(293ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v20 = Vector::Splat(static_cast<Number>(v19));
        const std::uint64_t v21 = (static_cast<std::uint64_t>(293ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v22 = Vector::Splat(static_cast<Number>(v21));
        const std::uint64_t v23 = (static_cast<std::uint64_t>(293ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v24 = Vector::Splat(static_cast<Number>(v23));
        static_assert(Size == 4);
        const Vector v25 = Vector{v18[0], v20[1], v22[2], v24[3]};
        const Vector v26 = (v25 + v15);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v27 = v26.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v28 = state.m_diffusion.m_fst.m_history.ReadAgeOffsets(v27, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v29 = (v28[2] - v28[0]);
        const Vector v31 = (v28[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v32 = Vector::FusedMultiplyAdd(v28[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v31);
        const Vector v33 = Vector::FusedMultiplyAdd(v28[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v32);
        const Vector v34 = (v33 - v28[3]);
        const Vector v35 = (v28[1] - v28[2]);
        const Vector v36 = (v28[3] - v28[0]);
        const Vector v37 = Vector::FusedMultiplyAdd(v35, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v36);
        const Vector v38 = (v16 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v39 = Vector::FusedMultiplyAdd(v16, v37, v34);
        const Vector v40 = Vector::FusedMultiplyAdd(v16, v39, v29);
        const Vector v41 = Vector::FusedMultiplyAdd(v38, v40, v28[1]);
        const Vector v43 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), input.m_x, v41);
        const Vector v44 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v43, input.m_x);
        state.m_diffusion.m_fst.m_history.Push(v44);
        [[maybe_unused]] const auto v45 = state.m_diffusion.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v46 = state.m_diffusion.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v47 = state.m_diffusion.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v48 = state.m_diffusion.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v49 = Vector{v45[0], v46[1], v47[2], v48[3]};
        const Vector v50 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v52 = Vector::IfLess(v49, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v49, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v53 = Vector::IfLess(v49, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v49, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v55 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v52, v53, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v56 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v55);
        const Vector v57 = (v50 * v56);
        const Vector v58 = (-v57);
        const Vector v59 = v58.Floor();
        const Vector v60 = (-v59);
        const Vector v61 = (v60 - v57);
        const std::uint64_t v62 = (static_cast<std::uint64_t>(487ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v63 = Vector::Splat(static_cast<Number>(v62));
        const std::uint64_t v64 = (static_cast<std::uint64_t>(487ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v65 = Vector::Splat(static_cast<Number>(v64));
        const std::uint64_t v66 = (static_cast<std::uint64_t>(487ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v67 = Vector::Splat(static_cast<Number>(v66));
        const std::uint64_t v68 = (static_cast<std::uint64_t>(487ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v69 = Vector::Splat(static_cast<Number>(v68));
        static_assert(Size == 4);
        const Vector v70 = Vector{v63[0], v65[1], v67[2], v69[3]};
        const Vector v71 = (v70 + v60);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v72 = v71.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v73 = state.m_diffusion.m_snd.m_fst.m_history.ReadAgeOffsets(v72, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v74 = (v73[2] - v73[0]);
        const Vector v76 = (v73[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v77 = Vector::FusedMultiplyAdd(v73[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v76);
        const Vector v78 = Vector::FusedMultiplyAdd(v73[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v77);
        const Vector v79 = (v78 - v73[3]);
        const Vector v80 = (v73[1] - v73[2]);
        const Vector v81 = (v73[3] - v73[0]);
        const Vector v82 = Vector::FusedMultiplyAdd(v80, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v81);
        const Vector v83 = (v61 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v84 = Vector::FusedMultiplyAdd(v61, v82, v79);
        const Vector v85 = Vector::FusedMultiplyAdd(v61, v84, v74);
        const Vector v86 = Vector::FusedMultiplyAdd(v83, v85, v73[1]);
        const Vector v88 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v43, v86);
        const Vector v89 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v88, v43);
        state.m_diffusion.m_snd.m_fst.m_history.Push(v89);
        [[maybe_unused]] const auto v90 = state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v91 = state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v92 = state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v93 = state.m_diffusion.m_snd.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v94 = Vector{v90[0], v91[1], v92[2], v93[3]};
        const Vector v95 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v97 = Vector::IfLess(v94, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v94, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v98 = Vector::IfLess(v94, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v94, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v100 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v97, v98, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v101 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v100);
        const Vector v102 = (v95 * v101);
        const Vector v103 = (-v102);
        const Vector v104 = v103.Floor();
        const Vector v105 = (-v104);
        const Vector v106 = (v105 - v102);
        const std::uint64_t v107 = (static_cast<std::uint64_t>(907ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v108 = Vector::Splat(static_cast<Number>(v107));
        const std::uint64_t v109 = (static_cast<std::uint64_t>(907ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v110 = Vector::Splat(static_cast<Number>(v109));
        const std::uint64_t v111 = (static_cast<std::uint64_t>(907ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v112 = Vector::Splat(static_cast<Number>(v111));
        const std::uint64_t v113 = (static_cast<std::uint64_t>(907ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v114 = Vector::Splat(static_cast<Number>(v113));
        static_assert(Size == 4);
        const Vector v115 = Vector{v108[0], v110[1], v112[2], v114[3]};
        const Vector v116 = (v115 + v105);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v117 = v116.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v118 = state.m_diffusion.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v117, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v119 = (v118[2] - v118[0]);
        const Vector v121 = (v118[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v122 = Vector::FusedMultiplyAdd(v118[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v121);
        const Vector v123 = Vector::FusedMultiplyAdd(v118[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v122);
        const Vector v124 = (v123 - v118[3]);
        const Vector v125 = (v118[1] - v118[2]);
        const Vector v126 = (v118[3] - v118[0]);
        const Vector v127 = Vector::FusedMultiplyAdd(v125, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v126);
        const Vector v128 = (v106 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v129 = Vector::FusedMultiplyAdd(v106, v127, v124);
        const Vector v130 = Vector::FusedMultiplyAdd(v106, v129, v119);
        const Vector v131 = Vector::FusedMultiplyAdd(v128, v130, v118[1]);
        const Vector v133 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v88, v131);
        const Vector v134 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v133, v88);
        state.m_diffusion.m_snd.m_snd.m_fst.m_history.Push(v134);
        [[maybe_unused]] const auto v135 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v136 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v137 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v138 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v139 = Vector{v135[0], v136[1], v137[2], v138[3]};
        const Vector v140 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v142 = Vector::IfLess(v139, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v139, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v143 = Vector::IfLess(v139, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v139, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v145 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v142, v143, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v146 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v145);
        const Vector v147 = (v140 * v146);
        const Vector v148 = (-v147);
        const Vector v149 = v148.Floor();
        const Vector v150 = (-v149);
        const Vector v151 = (v150 - v147);
        const std::uint64_t v152 = (static_cast<std::uint64_t>(1433ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v153 = Vector::Splat(static_cast<Number>(v152));
        const std::uint64_t v154 = (static_cast<std::uint64_t>(1433ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v155 = Vector::Splat(static_cast<Number>(v154));
        const std::uint64_t v156 = (static_cast<std::uint64_t>(1433ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v157 = Vector::Splat(static_cast<Number>(v156));
        const std::uint64_t v158 = (static_cast<std::uint64_t>(1433ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v159 = Vector::Splat(static_cast<Number>(v158));
        static_assert(Size == 4);
        const Vector v160 = Vector{v153[0], v155[1], v157[2], v159[3]};
        const Vector v161 = (v160 + v150);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v162 = v161.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v163 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v162, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v164 = (v163[2] - v163[0]);
        const Vector v166 = (v163[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v167 = Vector::FusedMultiplyAdd(v163[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v166);
        const Vector v168 = Vector::FusedMultiplyAdd(v163[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v167);
        const Vector v169 = (v168 - v163[3]);
        const Vector v170 = (v163[1] - v163[2]);
        const Vector v171 = (v163[3] - v163[0]);
        const Vector v172 = Vector::FusedMultiplyAdd(v170, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v171);
        const Vector v173 = (v151 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v174 = Vector::FusedMultiplyAdd(v151, v172, v169);
        const Vector v175 = Vector::FusedMultiplyAdd(v151, v174, v164);
        const Vector v176 = Vector::FusedMultiplyAdd(v173, v175, v163[1]);
        const Vector v178 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v133, v176);
        const Vector v179 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v178, v133);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.Push(v179);
        [[maybe_unused]] const auto v180 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v181 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v182 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v183 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v184 = Vector{v180[0], v181[1], v182[2], v183[3]};
        const Vector v185 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v187 = Vector::IfLess(v184, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v184, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v188 = Vector::IfLess(v184, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v184, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v190 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v187, v188, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v191 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v190);
        const Vector v192 = (v185 * v191);
        const Vector v193 = (-v192);
        const Vector v194 = v193.Floor();
        const Vector v195 = (-v194);
        const Vector v196 = (v195 - v192);
        const std::uint64_t v197 = (static_cast<std::uint64_t>(4493ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v198 = Vector::Splat(static_cast<Number>(v197));
        const std::uint64_t v199 = (static_cast<std::uint64_t>(4493ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v200 = Vector::Splat(static_cast<Number>(v199));
        const std::uint64_t v201 = (static_cast<std::uint64_t>(4493ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v202 = Vector::Splat(static_cast<Number>(v201));
        const std::uint64_t v203 = (static_cast<std::uint64_t>(4493ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v204 = Vector::Splat(static_cast<Number>(v203));
        static_assert(Size == 4);
        const Vector v205 = Vector{v198[0], v200[1], v202[2], v204[3]};
        const Vector v206 = (v205 + v195);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v207 = v206.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v208 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v207, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v209 = (v208[2] - v208[0]);
        const Vector v211 = (v208[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v212 = Vector::FusedMultiplyAdd(v208[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v211);
        const Vector v213 = Vector::FusedMultiplyAdd(v208[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v212);
        const Vector v214 = (v213 - v208[3]);
        const Vector v215 = (v208[1] - v208[2]);
        const Vector v216 = (v208[3] - v208[0]);
        const Vector v217 = Vector::FusedMultiplyAdd(v215, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v216);
        const Vector v218 = (v196 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v219 = Vector::FusedMultiplyAdd(v196, v217, v214);
        const Vector v220 = Vector::FusedMultiplyAdd(v196, v219, v209);
        const Vector v221 = Vector::FusedMultiplyAdd(v218, v220, v208[1]);
        const Vector v223 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v178, v221);
        const Vector v224 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v223, v178);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v224);
        [[maybe_unused]] const auto v225 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v226 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v227 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v228 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v229 = Vector{v225[0], v226[1], v227[2], v228[3]};
        const Vector v230 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v232 = Vector::IfLess(v229, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v229, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v233 = Vector::IfLess(v229, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v229, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v235 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v232, v233, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v236 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v235);
        const Vector v237 = (v230 * v236);
        const Vector v238 = (-v237);
        const Vector v239 = v238.Floor();
        const Vector v240 = (-v239);
        const Vector v241 = (v240 - v237);
        const std::uint64_t v242 = (static_cast<std::uint64_t>(6899ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v243 = Vector::Splat(static_cast<Number>(v242));
        const std::uint64_t v244 = (static_cast<std::uint64_t>(6899ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v245 = Vector::Splat(static_cast<Number>(v244));
        const std::uint64_t v246 = (static_cast<std::uint64_t>(6899ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v247 = Vector::Splat(static_cast<Number>(v246));
        const std::uint64_t v248 = (static_cast<std::uint64_t>(6899ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v249 = Vector::Splat(static_cast<Number>(v248));
        static_assert(Size == 4);
        const Vector v250 = Vector{v243[0], v245[1], v247[2], v249[3]};
        const Vector v251 = (v250 + v240);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v252 = v251.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v253 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v252, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v254 = (v253[2] - v253[0]);
        const Vector v256 = (v253[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v257 = Vector::FusedMultiplyAdd(v253[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v256);
        const Vector v258 = Vector::FusedMultiplyAdd(v253[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v257);
        const Vector v259 = (v258 - v253[3]);
        const Vector v260 = (v253[1] - v253[2]);
        const Vector v261 = (v253[3] - v253[0]);
        const Vector v262 = Vector::FusedMultiplyAdd(v260, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v261);
        const Vector v263 = (v241 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v264 = Vector::FusedMultiplyAdd(v241, v262, v259);
        const Vector v265 = Vector::FusedMultiplyAdd(v241, v264, v254);
        const Vector v266 = Vector::FusedMultiplyAdd(v263, v265, v253[1]);
        const Vector v268 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v223, v266);
        const Vector v269 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v268, v223);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v269);
        [[maybe_unused]] const auto v270 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v271 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v272 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v273 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v274 = Vector{v270[0], v271[1], v272[2], v273[3]};
        const Vector v275 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v277 = Vector::IfLess(v274, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v274, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v278 = Vector::IfLess(v274, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v274, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v280 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v277, v278, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v281 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v280);
        const Vector v282 = (v275 * v281);
        const Vector v283 = (-v282);
        const Vector v284 = v283.Floor();
        const Vector v285 = (-v284);
        const Vector v286 = (v285 - v282);
        const std::uint64_t v287 = (static_cast<std::uint64_t>(9811ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v288 = Vector::Splat(static_cast<Number>(v287));
        const std::uint64_t v289 = (static_cast<std::uint64_t>(9811ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v290 = Vector::Splat(static_cast<Number>(v289));
        const std::uint64_t v291 = (static_cast<std::uint64_t>(9811ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v292 = Vector::Splat(static_cast<Number>(v291));
        const std::uint64_t v293 = (static_cast<std::uint64_t>(9811ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v294 = Vector::Splat(static_cast<Number>(v293));
        static_assert(Size == 4);
        const Vector v295 = Vector{v288[0], v290[1], v292[2], v294[3]};
        const Vector v296 = (v295 + v285);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v297 = v296.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v298 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v297, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v299 = (v298[2] - v298[0]);
        const Vector v301 = (v298[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v302 = Vector::FusedMultiplyAdd(v298[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v301);
        const Vector v303 = Vector::FusedMultiplyAdd(v298[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v302);
        const Vector v304 = (v303 - v298[3]);
        const Vector v305 = (v298[1] - v298[2]);
        const Vector v306 = (v298[3] - v298[0]);
        const Vector v307 = Vector::FusedMultiplyAdd(v305, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v306);
        const Vector v308 = (v286 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v309 = Vector::FusedMultiplyAdd(v286, v307, v304);
        const Vector v310 = Vector::FusedMultiplyAdd(v286, v309, v299);
        const Vector v311 = Vector::FusedMultiplyAdd(v308, v310, v298[1]);
        const Vector v313 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v268, v311);
        const Vector v314 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v313, v268);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v314);
        [[maybe_unused]] const auto v315 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v316 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v317 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v318 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v319 = Vector{v315[0], v316[1], v317[2], v318[3]};
        const Vector v320 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v322 = Vector::IfLess(v319, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v319, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v323 = Vector::IfLess(v319, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v319, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v325 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v322, v323, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v326 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v325);
        const Vector v327 = (v320 * v326);
        const Vector v328 = (-v327);
        const Vector v329 = v328.Floor();
        const Vector v330 = (-v329);
        const Vector v331 = (v330 - v327);
        const std::uint64_t v332 = (static_cast<std::uint64_t>(14033ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v333 = Vector::Splat(static_cast<Number>(v332));
        const std::uint64_t v334 = (static_cast<std::uint64_t>(14033ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v335 = Vector::Splat(static_cast<Number>(v334));
        const std::uint64_t v336 = (static_cast<std::uint64_t>(14033ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v337 = Vector::Splat(static_cast<Number>(v336));
        const std::uint64_t v338 = (static_cast<std::uint64_t>(14033ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v339 = Vector::Splat(static_cast<Number>(v338));
        static_assert(Size == 4);
        const Vector v340 = Vector{v333[0], v335[1], v337[2], v339[3]};
        const Vector v341 = (v340 + v330);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v342 = v341.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v343 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v342, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v344 = (v343[2] - v343[0]);
        const Vector v346 = (v343[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v347 = Vector::FusedMultiplyAdd(v343[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v346);
        const Vector v348 = Vector::FusedMultiplyAdd(v343[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v347);
        const Vector v349 = (v348 - v343[3]);
        const Vector v350 = (v343[1] - v343[2]);
        const Vector v351 = (v343[3] - v343[0]);
        const Vector v352 = Vector::FusedMultiplyAdd(v350, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v351);
        const Vector v353 = (v331 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v354 = Vector::FusedMultiplyAdd(v331, v352, v349);
        const Vector v355 = Vector::FusedMultiplyAdd(v331, v354, v344);
        const Vector v356 = Vector::FusedMultiplyAdd(v353, v355, v343[1]);
        const Vector v358 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v313, v356);
        const Vector v359 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v358, v313);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v359);
        const Vector v360 = state.m_wet;
        const Vector v361 = state.m_settings.m_feedback;
        const Vector v362 = (v360 * v361);
        const Vector v363 = (v358 + v362);
        state.m_delay.Push(v363);
        const Vector v364 = state.m_settings.m_delaySamples;
        const Vector v365 = (-v364);
        const Vector v366 = v365.Floor();
        const Vector v367 = (-v366);
        const Vector v368 = (v367 - v364);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v369 = v367.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v370 = state.m_delay.ReadAgeOffsets(v369, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v371 = (v370[2] - v370[0]);
        const Vector v373 = (v370[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v374 = Vector::FusedMultiplyAdd(v370[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v373);
        const Vector v375 = Vector::FusedMultiplyAdd(v370[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v374);
        const Vector v376 = (v375 - v370[3]);
        const Vector v377 = (v370[1] - v370[2]);
        const Vector v378 = (v370[3] - v370[0]);
        const Vector v379 = Vector::FusedMultiplyAdd(v377, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v378);
        const Vector v380 = (v368 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v381 = Vector::FusedMultiplyAdd(v368, v379, v376);
        const Vector v382 = Vector::FusedMultiplyAdd(v368, v381, v371);
        const Vector v383 = Vector::FusedMultiplyAdd(v380, v382, v370[1]);
        const Vector v385 = Vector::Splat(v383[0]);
        const Vector v386 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb9611ap-24f : 0x172c234f72c235p-53)) * v385);
        const Vector v388 = Vector::Splat(v383[1]);
        const Vector v389 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb08d3ep-24f : 0x1611a7b9611a7cp-53)) * v388);
        const Vector v390 = (v386 - v389);
        const Vector v392 = Vector::Splat(v383[0]);
        const Vector v393 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb08d3ep-24f : 0x1611a7b9611a7cp-53)) * v392);
        const Vector v395 = Vector::Splat(v383[1]);
        const Vector v396 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb9611ap-24f : 0x172c234f72c235p-53)) * v395);
        const Vector v397 = (v393 + v396);
        const Vector v399 = Vector::Splat(v383[2]);
        const Vector v400 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd95bc6p-24f : 0x1b2b78c13521d0p-53)) * v399);
        const Vector v402 = Vector::Splat(v383[3]);
        const Vector v403 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x873ecbp-24f : 0x10e7d95bc609a9p-53)) * v402);
        const Vector v404 = (v400 - v403);
        const Vector v406 = Vector::Splat(v383[2]);
        const Vector v407 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x873ecbp-24f : 0x10e7d95bc609a9p-53)) * v406);
        const Vector v409 = Vector::Splat(v383[3]);
        const Vector v410 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd95bc6p-24f : 0x1b2b78c13521d0p-53)) * v409);
        const Vector v411 = (v407 + v410);
        const Vector v413 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc0e070p-24f : 0x181c0e070381c1p-53)) * v390);
        const Vector v416 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa8542ap-24f : -0x150a8542a150a8p-53)) * v404);
        const Vector v417 = (v413 - v416);
        const Vector v420 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa8542ap-24f : -0x150a8542a150a8p-53)) * v390);
        const Vector v422 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc0e070p-24f : 0x181c0e070381c1p-53)) * v404);
        const Vector v423 = (v420 + v422);
        const Vector v425 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xab8be0p-24f : 0x15717c0a8e83f5p-53)) * v397);
        const Vector v427 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xbe0547p-24f : 0x17c0a8e83f5718p-53)) * v411);
        const Vector v428 = (v425 - v427);
        const Vector v430 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xbe0547p-24f : 0x17c0a8e83f5718p-53)) * v397);
        const Vector v432 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xab8be0p-24f : 0x15717c0a8e83f5p-53)) * v411);
        const Vector v433 = (v430 + v432);
        static_assert(Size == 4);
        const Vector v434 = Vector{v417[0], v428[1], v423[2], v433[3]};
        [[maybe_unused]] const auto v435 = state.m_first.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v436 = state.m_first.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v437 = state.m_first.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v438 = state.m_first.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v439 = Vector{v435[0], v436[1], v437[2], v438[3]};
        const Vector v440 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v442 = Vector::IfLess(v439, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v439, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v443 = Vector::IfLess(v439, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v439, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v445 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v442, v443, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v446 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v445);
        const Vector v447 = (v440 * v446);
        const Vector v448 = (-v447);
        const Vector v449 = v448.Floor();
        const Vector v450 = (-v449);
        const Vector v451 = (v450 - v447);
        const std::uint64_t v452 = (static_cast<std::uint64_t>(2039ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v453 = Vector::Splat(static_cast<Number>(v452));
        const std::uint64_t v454 = (static_cast<std::uint64_t>(3719ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v455 = Vector::Splat(static_cast<Number>(v454));
        const std::uint64_t v456 = (static_cast<std::uint64_t>(6359ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v457 = Vector::Splat(static_cast<Number>(v456));
        const std::uint64_t v458 = (static_cast<std::uint64_t>(2753ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v459 = Vector::Splat(static_cast<Number>(v458));
        static_assert(Size == 4);
        const Vector v460 = Vector{v453[0], v455[1], v457[2], v459[3]};
        const Vector v461 = (v460 + v450);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v462 = v461.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v463 = state.m_first.m_history.ReadAgeOffsets(v462, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v464 = (v463[2] - v463[0]);
        const Vector v466 = (v463[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v467 = Vector::FusedMultiplyAdd(v463[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v466);
        const Vector v468 = Vector::FusedMultiplyAdd(v463[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v467);
        const Vector v469 = (v468 - v463[3]);
        const Vector v470 = (v463[1] - v463[2]);
        const Vector v471 = (v463[3] - v463[0]);
        const Vector v472 = Vector::FusedMultiplyAdd(v470, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v471);
        const Vector v473 = (v451 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v474 = Vector::FusedMultiplyAdd(v451, v472, v469);
        const Vector v475 = Vector::FusedMultiplyAdd(v451, v474, v464);
        const Vector v476 = Vector::FusedMultiplyAdd(v473, v475, v463[1]);
        const Vector v478 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v434, v476);
        const Vector v479 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v478, v434);
        state.m_first.m_history.Push(v479);
        const Vector v480 = state.m_settings.m_saturatorGain;
        const Vector v481 = (v478 * v480);
        const Vector v482 = (v481 * v481);
        const Vector v483 = (v481 * (v482 + Vector::Splat(27)));
        const Vector v484 = Vector::FusedMultiplyAdd(v481 * Vector::Splat(9), v481, Vector::Splat(27));
        const Vector v485 = (v483 / v484);
        const Vector v486 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v485));
        const Vector v487 = (v486 - state.m_damping.m_filter.m_highPass.m_lowPass.m_s);
        const Vector v488 = Vector::FusedMultiplyAdd(v487, state.m_damping.m_filter.m_highPass.m_lowPass.m_G, state.m_damping.m_filter.m_highPass.m_lowPass.m_s);
        const Vector v489 = Vector::FusedMultiplyAdd(v487, state.m_damping.m_filter.m_highPass.m_lowPass.m_G, v488);
        const Vector v490 = (v478 * v480);
        const Vector v491 = (v490 * v490);
        const Vector v492 = (v490 * (v491 + Vector::Splat(27)));
        const Vector v493 = Vector::FusedMultiplyAdd(v490 * Vector::Splat(9), v490, Vector::Splat(27));
        const Vector v494 = (v492 / v493);
        const Vector v495 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v494));
        const Vector v496 = (v495 - v488);
        const Vector v497 = (v496 - state.m_damping.m_filter.m_lowPass.m_s);
        const Vector v498 = Vector::FusedMultiplyAdd(v497, state.m_damping.m_filter.m_lowPass.m_G, state.m_damping.m_filter.m_lowPass.m_s);
        const Vector v499 = Vector::FusedMultiplyAdd(v497, state.m_damping.m_filter.m_lowPass.m_G, v498);
        state.m_damping.m_filter.m_highPass.m_lowPass.m_s = v489;
        state.m_damping.m_filter.m_lowPass.m_s = v499;
        [[maybe_unused]] const auto v500 = state.m_second.m_clock.m_lane0.NextSpline();
        [[maybe_unused]] const auto v501 = state.m_second.m_clock.m_lane1.NextSpline();
        [[maybe_unused]] const auto v502 = state.m_second.m_clock.m_lane2.NextSpline();
        [[maybe_unused]] const auto v503 = state.m_second.m_clock.m_lane3.NextSpline();
        static_assert(Size == 4);
        const Vector v504 = Vector{v500[0], v501[1], v502[2], v503[3]};
        const Vector v505 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v507 = Vector::IfLess(v504, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v504, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v508 = Vector::IfLess(v504, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v504, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v510 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v507, v508, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v511 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v510);
        const Vector v512 = (v505 * v511);
        const Vector v513 = (-v512);
        const Vector v514 = v513.Floor();
        const Vector v515 = (-v514);
        const Vector v516 = (v515 - v512);
        const std::uint64_t v517 = (static_cast<std::uint64_t>(11923ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v518 = Vector::Splat(static_cast<Number>(v517));
        const std::uint64_t v519 = (static_cast<std::uint64_t>(16879ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v520 = Vector::Splat(static_cast<Number>(v519));
        const std::uint64_t v521 = (static_cast<std::uint64_t>(9041ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v522 = Vector::Splat(static_cast<Number>(v521));
        const std::uint64_t v523 = (static_cast<std::uint64_t>(23447ULL) - static_cast<std::uint64_t>(128ULL));
        const Vector v524 = Vector::Splat(static_cast<Number>(v523));
        static_assert(Size == 4);
        const Vector v525 = Vector{v518[0], v520[1], v522[2], v524[3]};
        const Vector v526 = (v525 + v515);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v527 = v526.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v528 = state.m_second.m_history.ReadAgeOffsets(v527, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v529 = (v528[2] - v528[0]);
        const Vector v531 = (v528[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v532 = Vector::FusedMultiplyAdd(v528[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v531);
        const Vector v533 = Vector::FusedMultiplyAdd(v528[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v532);
        const Vector v534 = (v533 - v528[3]);
        const Vector v535 = (v528[1] - v528[2]);
        const Vector v536 = (v528[3] - v528[0]);
        const Vector v537 = Vector::FusedMultiplyAdd(v535, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v536);
        const Vector v538 = (v516 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v539 = Vector::FusedMultiplyAdd(v516, v537, v534);
        const Vector v540 = Vector::FusedMultiplyAdd(v516, v539, v529);
        const Vector v541 = Vector::FusedMultiplyAdd(v538, v540, v528[1]);
        const Vector v543 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v498, v541);
        const Vector v544 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v543, v498);
        state.m_second.m_history.Push(v544);
        state.m_wet = v543;
        return v543;
    }

};
