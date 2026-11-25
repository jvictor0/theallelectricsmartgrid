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
                PackedWanderingRandomLFO<Number, Size, false> m_clock;
            } m_fst;

            struct
            {
                struct
                {
                    CircularBufferGeneric<Number, Size, 2048> m_history;
                    PackedWanderingRandomLFO<Number, Size, false> m_clock;
                } m_fst;

                struct
                {
                    struct
                    {
                        CircularBufferGeneric<Number, Size, 2048> m_history;
                        PackedWanderingRandomLFO<Number, Size, false> m_clock;
                    } m_fst;

                    struct
                    {
                        struct
                        {
                            CircularBufferGeneric<Number, Size, 2048> m_history;
                            PackedWanderingRandomLFO<Number, Size, false> m_clock;
                        } m_fst;

                        struct
                        {
                            struct
                            {
                                CircularBufferGeneric<Number, Size, 8192> m_history;
                                PackedWanderingRandomLFO<Number, Size, false> m_clock;
                            } m_fst;

                            struct
                            {
                                struct
                                {
                                    CircularBufferGeneric<Number, Size, 8192> m_history;
                                    PackedWanderingRandomLFO<Number, Size, false> m_clock;
                                } m_fst;

                                struct
                                {
                                    struct
                                    {
                                        CircularBufferGeneric<Number, Size, 16384> m_history;
                                        PackedWanderingRandomLFO<Number, Size, false> m_clock;
                                    } m_fst;

                                    struct
                                    {
                                        struct
                                        {
                                            CircularBufferGeneric<Number, Size, 16384> m_history;
                                            PackedWanderingRandomLFO<Number, Size, false> m_clock;
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
            PackedWanderingRandomLFO<Number, Size, false> m_clock;
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
            PackedWanderingRandomLFO<Number, Size, false> m_clock;
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
        [[maybe_unused]] const auto v0 = state.m_diffusion.m_fst.m_clock.NextSpline();
        const Vector v1 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v3 = Vector::IfLess(v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v4 = Vector::IfLess(v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v0, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v6 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v3, v4, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v7 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v6);
        const Vector v8 = (v1 * v7);
        const Vector v9 = (-v8);
        const Vector v10 = v9.Floor();
        const Vector v11 = (-v10);
        const Vector v12 = (v11 - v8);
        const std::uint64_t v13 = CodegenInteger::ToWord(v11[0]);
        const std::uint64_t v14 = (static_cast<std::uint64_t>(293ULL) + v13);
        const std::uint64_t v15 = (v14 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v16 = (static_cast<std::uint64_t>(293ULL) + v13);
        const std::uint64_t v17 = (v16 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v18 = (static_cast<std::uint64_t>(293ULL) + v13);
        const std::uint64_t v19 = (v18 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v20 = (static_cast<std::uint64_t>(293ULL) + v13);
        const std::uint64_t v21 = (v20 - static_cast<std::uint64_t>(128ULL));
        const bool v22 = (static_cast<std::uint64_t>(293ULL) == static_cast<std::uint64_t>(293ULL));
        bool v27;
        if (v22)
        {
            const bool v23 = (static_cast<std::uint64_t>(293ULL) == static_cast<std::uint64_t>(293ULL));
            bool v26;
            if (v23)
            {
                const bool v24 = (static_cast<std::uint64_t>(293ULL) == static_cast<std::uint64_t>(293ULL));
                bool v25;
                if (v24)
                {
                    v25 = true;
                }
                else
                {
                    v25 = false;
                }

                v26 = v25;
            }
            else
            {
                v26 = false;
            }

            v27 = v26;
        }
        else
        {
            v27 = false;
        }

        const bool v28 = (v27 == true);
        Vector v64;
        if (v28)
        {
            [[maybe_unused]] const auto v29 = state.m_diffusion.m_fst.m_history.ReadWrappedAge(v15);
            const std::uint64_t v30 = (v15 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v31 = state.m_diffusion.m_fst.m_history.ReadWrappedAge(v30);
            const std::uint64_t v32 = (v15 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v33 = state.m_diffusion.m_fst.m_history.ReadWrappedAge(v32);
            const std::uint64_t v34 = (v15 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v35 = state.m_diffusion.m_fst.m_history.ReadWrappedAge(v34);
            const Vector v36 = (v33 - v29);
            const Vector v38 = (v33 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v39 = Vector::FusedMultiplyAdd(v31, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v38);
            const Vector v40 = Vector::FusedMultiplyAdd(v29, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v39);
            const Vector v41 = (v40 - v35);
            const Vector v42 = (v31 - v33);
            const Vector v43 = (v35 - v29);
            const Vector v44 = Vector::FusedMultiplyAdd(v42, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v43);
            const Vector v45 = (v12 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v46 = Vector::FusedMultiplyAdd(v12, v44, v41);
            const Vector v47 = Vector::FusedMultiplyAdd(v12, v46, v36);
            const Vector v48 = Vector::FusedMultiplyAdd(v45, v47, v31);
            v64 = v48;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v49 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v15), static_cast<std::uint32_t>(v17), static_cast<std::uint32_t>(v19), static_cast<std::uint32_t>(v21)};
            [[maybe_unused]] const auto v50 = state.m_diffusion.m_fst.m_history.ReadAgeOffsets(v49, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v51 = (v50[2] - v50[0]);
            const Vector v53 = (v50[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v54 = Vector::FusedMultiplyAdd(v50[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v53);
            const Vector v55 = Vector::FusedMultiplyAdd(v50[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v54);
            const Vector v56 = (v55 - v50[3]);
            const Vector v57 = (v50[1] - v50[2]);
            const Vector v58 = (v50[3] - v50[0]);
            const Vector v59 = Vector::FusedMultiplyAdd(v57, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v58);
            const Vector v60 = (v12 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v61 = Vector::FusedMultiplyAdd(v12, v59, v56);
            const Vector v62 = Vector::FusedMultiplyAdd(v12, v61, v51);
            const Vector v63 = Vector::FusedMultiplyAdd(v60, v62, v50[1]);
            v64 = v63;
        }

        const Vector v66 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), input.m_x, v64);
        const Vector v67 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v66, input.m_x);
        state.m_diffusion.m_fst.m_history.Push(v67);
        [[maybe_unused]] const auto v68 = state.m_diffusion.m_snd.m_fst.m_clock.NextSpline();
        const Vector v69 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v71 = Vector::IfLess(v68, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v68, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v72 = Vector::IfLess(v68, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v68, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v74 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v71, v72, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v75 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v74);
        const Vector v76 = (v69 * v75);
        const Vector v77 = (-v76);
        const Vector v78 = v77.Floor();
        const Vector v79 = (-v78);
        const Vector v80 = (v79 - v76);
        const std::uint64_t v81 = CodegenInteger::ToWord(v79[0]);
        const std::uint64_t v82 = (static_cast<std::uint64_t>(487ULL) + v81);
        const std::uint64_t v83 = (v82 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v84 = (static_cast<std::uint64_t>(487ULL) + v81);
        const std::uint64_t v85 = (v84 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v86 = (static_cast<std::uint64_t>(487ULL) + v81);
        const std::uint64_t v87 = (v86 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v88 = (static_cast<std::uint64_t>(487ULL) + v81);
        const std::uint64_t v89 = (v88 - static_cast<std::uint64_t>(128ULL));
        const bool v90 = (static_cast<std::uint64_t>(487ULL) == static_cast<std::uint64_t>(487ULL));
        bool v95;
        if (v90)
        {
            const bool v91 = (static_cast<std::uint64_t>(487ULL) == static_cast<std::uint64_t>(487ULL));
            bool v94;
            if (v91)
            {
                const bool v92 = (static_cast<std::uint64_t>(487ULL) == static_cast<std::uint64_t>(487ULL));
                bool v93;
                if (v92)
                {
                    v93 = true;
                }
                else
                {
                    v93 = false;
                }

                v94 = v93;
            }
            else
            {
                v94 = false;
            }

            v95 = v94;
        }
        else
        {
            v95 = false;
        }

        const bool v96 = (v95 == true);
        Vector v132;
        if (v96)
        {
            [[maybe_unused]] const auto v97 = state.m_diffusion.m_snd.m_fst.m_history.ReadWrappedAge(v83);
            const std::uint64_t v98 = (v83 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v99 = state.m_diffusion.m_snd.m_fst.m_history.ReadWrappedAge(v98);
            const std::uint64_t v100 = (v83 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v101 = state.m_diffusion.m_snd.m_fst.m_history.ReadWrappedAge(v100);
            const std::uint64_t v102 = (v83 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v103 = state.m_diffusion.m_snd.m_fst.m_history.ReadWrappedAge(v102);
            const Vector v104 = (v101 - v97);
            const Vector v106 = (v101 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v107 = Vector::FusedMultiplyAdd(v99, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v106);
            const Vector v108 = Vector::FusedMultiplyAdd(v97, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v107);
            const Vector v109 = (v108 - v103);
            const Vector v110 = (v99 - v101);
            const Vector v111 = (v103 - v97);
            const Vector v112 = Vector::FusedMultiplyAdd(v110, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v111);
            const Vector v113 = (v80 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v114 = Vector::FusedMultiplyAdd(v80, v112, v109);
            const Vector v115 = Vector::FusedMultiplyAdd(v80, v114, v104);
            const Vector v116 = Vector::FusedMultiplyAdd(v113, v115, v99);
            v132 = v116;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v117 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v83), static_cast<std::uint32_t>(v85), static_cast<std::uint32_t>(v87), static_cast<std::uint32_t>(v89)};
            [[maybe_unused]] const auto v118 = state.m_diffusion.m_snd.m_fst.m_history.ReadAgeOffsets(v117, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v119 = (v118[2] - v118[0]);
            const Vector v121 = (v118[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v122 = Vector::FusedMultiplyAdd(v118[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v121);
            const Vector v123 = Vector::FusedMultiplyAdd(v118[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v122);
            const Vector v124 = (v123 - v118[3]);
            const Vector v125 = (v118[1] - v118[2]);
            const Vector v126 = (v118[3] - v118[0]);
            const Vector v127 = Vector::FusedMultiplyAdd(v125, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v126);
            const Vector v128 = (v80 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v129 = Vector::FusedMultiplyAdd(v80, v127, v124);
            const Vector v130 = Vector::FusedMultiplyAdd(v80, v129, v119);
            const Vector v131 = Vector::FusedMultiplyAdd(v128, v130, v118[1]);
            v132 = v131;
        }

        const Vector v134 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v66, v132);
        const Vector v135 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v134, v66);
        state.m_diffusion.m_snd.m_fst.m_history.Push(v135);
        [[maybe_unused]] const auto v136 = state.m_diffusion.m_snd.m_snd.m_fst.m_clock.NextSpline();
        const Vector v137 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v139 = Vector::IfLess(v136, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v136, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v140 = Vector::IfLess(v136, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v136, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v142 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v139, v140, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v143 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v142);
        const Vector v144 = (v137 * v143);
        const Vector v145 = (-v144);
        const Vector v146 = v145.Floor();
        const Vector v147 = (-v146);
        const Vector v148 = (v147 - v144);
        const std::uint64_t v149 = CodegenInteger::ToWord(v147[0]);
        const std::uint64_t v150 = (static_cast<std::uint64_t>(907ULL) + v149);
        const std::uint64_t v151 = (v150 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v152 = (static_cast<std::uint64_t>(907ULL) + v149);
        const std::uint64_t v153 = (v152 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v154 = (static_cast<std::uint64_t>(907ULL) + v149);
        const std::uint64_t v155 = (v154 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v156 = (static_cast<std::uint64_t>(907ULL) + v149);
        const std::uint64_t v157 = (v156 - static_cast<std::uint64_t>(128ULL));
        const bool v158 = (static_cast<std::uint64_t>(907ULL) == static_cast<std::uint64_t>(907ULL));
        bool v163;
        if (v158)
        {
            const bool v159 = (static_cast<std::uint64_t>(907ULL) == static_cast<std::uint64_t>(907ULL));
            bool v162;
            if (v159)
            {
                const bool v160 = (static_cast<std::uint64_t>(907ULL) == static_cast<std::uint64_t>(907ULL));
                bool v161;
                if (v160)
                {
                    v161 = true;
                }
                else
                {
                    v161 = false;
                }

                v162 = v161;
            }
            else
            {
                v162 = false;
            }

            v163 = v162;
        }
        else
        {
            v163 = false;
        }

        const bool v164 = (v163 == true);
        Vector v200;
        if (v164)
        {
            [[maybe_unused]] const auto v165 = state.m_diffusion.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v151);
            const std::uint64_t v166 = (v151 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v167 = state.m_diffusion.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v166);
            const std::uint64_t v168 = (v151 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v169 = state.m_diffusion.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v168);
            const std::uint64_t v170 = (v151 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v171 = state.m_diffusion.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v170);
            const Vector v172 = (v169 - v165);
            const Vector v174 = (v169 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v175 = Vector::FusedMultiplyAdd(v167, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v174);
            const Vector v176 = Vector::FusedMultiplyAdd(v165, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v175);
            const Vector v177 = (v176 - v171);
            const Vector v178 = (v167 - v169);
            const Vector v179 = (v171 - v165);
            const Vector v180 = Vector::FusedMultiplyAdd(v178, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v179);
            const Vector v181 = (v148 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v182 = Vector::FusedMultiplyAdd(v148, v180, v177);
            const Vector v183 = Vector::FusedMultiplyAdd(v148, v182, v172);
            const Vector v184 = Vector::FusedMultiplyAdd(v181, v183, v167);
            v200 = v184;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v185 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v151), static_cast<std::uint32_t>(v153), static_cast<std::uint32_t>(v155), static_cast<std::uint32_t>(v157)};
            [[maybe_unused]] const auto v186 = state.m_diffusion.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v185, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v187 = (v186[2] - v186[0]);
            const Vector v189 = (v186[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v190 = Vector::FusedMultiplyAdd(v186[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v189);
            const Vector v191 = Vector::FusedMultiplyAdd(v186[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v190);
            const Vector v192 = (v191 - v186[3]);
            const Vector v193 = (v186[1] - v186[2]);
            const Vector v194 = (v186[3] - v186[0]);
            const Vector v195 = Vector::FusedMultiplyAdd(v193, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v194);
            const Vector v196 = (v148 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v197 = Vector::FusedMultiplyAdd(v148, v195, v192);
            const Vector v198 = Vector::FusedMultiplyAdd(v148, v197, v187);
            const Vector v199 = Vector::FusedMultiplyAdd(v196, v198, v186[1]);
            v200 = v199;
        }

        const Vector v202 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v134, v200);
        const Vector v203 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v202, v134);
        state.m_diffusion.m_snd.m_snd.m_fst.m_history.Push(v203);
        [[maybe_unused]] const auto v204 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_clock.NextSpline();
        const Vector v205 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v207 = Vector::IfLess(v204, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v204, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v208 = Vector::IfLess(v204, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v204, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v210 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v207, v208, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v211 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v210);
        const Vector v212 = (v205 * v211);
        const Vector v213 = (-v212);
        const Vector v214 = v213.Floor();
        const Vector v215 = (-v214);
        const Vector v216 = (v215 - v212);
        const std::uint64_t v217 = CodegenInteger::ToWord(v215[0]);
        const std::uint64_t v218 = (static_cast<std::uint64_t>(1433ULL) + v217);
        const std::uint64_t v219 = (v218 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v220 = (static_cast<std::uint64_t>(1433ULL) + v217);
        const std::uint64_t v221 = (v220 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v222 = (static_cast<std::uint64_t>(1433ULL) + v217);
        const std::uint64_t v223 = (v222 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v224 = (static_cast<std::uint64_t>(1433ULL) + v217);
        const std::uint64_t v225 = (v224 - static_cast<std::uint64_t>(128ULL));
        const bool v226 = (static_cast<std::uint64_t>(1433ULL) == static_cast<std::uint64_t>(1433ULL));
        bool v231;
        if (v226)
        {
            const bool v227 = (static_cast<std::uint64_t>(1433ULL) == static_cast<std::uint64_t>(1433ULL));
            bool v230;
            if (v227)
            {
                const bool v228 = (static_cast<std::uint64_t>(1433ULL) == static_cast<std::uint64_t>(1433ULL));
                bool v229;
                if (v228)
                {
                    v229 = true;
                }
                else
                {
                    v229 = false;
                }

                v230 = v229;
            }
            else
            {
                v230 = false;
            }

            v231 = v230;
        }
        else
        {
            v231 = false;
        }

        const bool v232 = (v231 == true);
        Vector v268;
        if (v232)
        {
            [[maybe_unused]] const auto v233 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v219);
            const std::uint64_t v234 = (v219 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v235 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v234);
            const std::uint64_t v236 = (v219 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v237 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v236);
            const std::uint64_t v238 = (v219 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v239 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v238);
            const Vector v240 = (v237 - v233);
            const Vector v242 = (v237 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v243 = Vector::FusedMultiplyAdd(v235, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v242);
            const Vector v244 = Vector::FusedMultiplyAdd(v233, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v243);
            const Vector v245 = (v244 - v239);
            const Vector v246 = (v235 - v237);
            const Vector v247 = (v239 - v233);
            const Vector v248 = Vector::FusedMultiplyAdd(v246, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v247);
            const Vector v249 = (v216 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v250 = Vector::FusedMultiplyAdd(v216, v248, v245);
            const Vector v251 = Vector::FusedMultiplyAdd(v216, v250, v240);
            const Vector v252 = Vector::FusedMultiplyAdd(v249, v251, v235);
            v268 = v252;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v253 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v219), static_cast<std::uint32_t>(v221), static_cast<std::uint32_t>(v223), static_cast<std::uint32_t>(v225)};
            [[maybe_unused]] const auto v254 = state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v253, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v255 = (v254[2] - v254[0]);
            const Vector v257 = (v254[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v258 = Vector::FusedMultiplyAdd(v254[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v257);
            const Vector v259 = Vector::FusedMultiplyAdd(v254[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v258);
            const Vector v260 = (v259 - v254[3]);
            const Vector v261 = (v254[1] - v254[2]);
            const Vector v262 = (v254[3] - v254[0]);
            const Vector v263 = Vector::FusedMultiplyAdd(v261, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v262);
            const Vector v264 = (v216 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v265 = Vector::FusedMultiplyAdd(v216, v263, v260);
            const Vector v266 = Vector::FusedMultiplyAdd(v216, v265, v255);
            const Vector v267 = Vector::FusedMultiplyAdd(v264, v266, v254[1]);
            v268 = v267;
        }

        const Vector v270 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v202, v268);
        const Vector v271 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v270, v202);
        state.m_diffusion.m_snd.m_snd.m_snd.m_fst.m_history.Push(v271);
        [[maybe_unused]] const auto v272 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.NextSpline();
        const Vector v273 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v275 = Vector::IfLess(v272, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v272, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v276 = Vector::IfLess(v272, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v272, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v278 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v275, v276, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v279 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v278);
        const Vector v280 = (v273 * v279);
        const Vector v281 = (-v280);
        const Vector v282 = v281.Floor();
        const Vector v283 = (-v282);
        const Vector v284 = (v283 - v280);
        const std::uint64_t v285 = CodegenInteger::ToWord(v283[0]);
        const std::uint64_t v286 = (static_cast<std::uint64_t>(4493ULL) + v285);
        const std::uint64_t v287 = (v286 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v288 = (static_cast<std::uint64_t>(4493ULL) + v285);
        const std::uint64_t v289 = (v288 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v290 = (static_cast<std::uint64_t>(4493ULL) + v285);
        const std::uint64_t v291 = (v290 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v292 = (static_cast<std::uint64_t>(4493ULL) + v285);
        const std::uint64_t v293 = (v292 - static_cast<std::uint64_t>(128ULL));
        const bool v294 = (static_cast<std::uint64_t>(4493ULL) == static_cast<std::uint64_t>(4493ULL));
        bool v299;
        if (v294)
        {
            const bool v295 = (static_cast<std::uint64_t>(4493ULL) == static_cast<std::uint64_t>(4493ULL));
            bool v298;
            if (v295)
            {
                const bool v296 = (static_cast<std::uint64_t>(4493ULL) == static_cast<std::uint64_t>(4493ULL));
                bool v297;
                if (v296)
                {
                    v297 = true;
                }
                else
                {
                    v297 = false;
                }

                v298 = v297;
            }
            else
            {
                v298 = false;
            }

            v299 = v298;
        }
        else
        {
            v299 = false;
        }

        const bool v300 = (v299 == true);
        Vector v336;
        if (v300)
        {
            [[maybe_unused]] const auto v301 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v287);
            const std::uint64_t v302 = (v287 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v303 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v302);
            const std::uint64_t v304 = (v287 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v305 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v304);
            const std::uint64_t v306 = (v287 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v307 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v306);
            const Vector v308 = (v305 - v301);
            const Vector v310 = (v305 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v311 = Vector::FusedMultiplyAdd(v303, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v310);
            const Vector v312 = Vector::FusedMultiplyAdd(v301, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v311);
            const Vector v313 = (v312 - v307);
            const Vector v314 = (v303 - v305);
            const Vector v315 = (v307 - v301);
            const Vector v316 = Vector::FusedMultiplyAdd(v314, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v315);
            const Vector v317 = (v284 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v318 = Vector::FusedMultiplyAdd(v284, v316, v313);
            const Vector v319 = Vector::FusedMultiplyAdd(v284, v318, v308);
            const Vector v320 = Vector::FusedMultiplyAdd(v317, v319, v303);
            v336 = v320;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v321 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v287), static_cast<std::uint32_t>(v289), static_cast<std::uint32_t>(v291), static_cast<std::uint32_t>(v293)};
            [[maybe_unused]] const auto v322 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v321, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v323 = (v322[2] - v322[0]);
            const Vector v325 = (v322[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v326 = Vector::FusedMultiplyAdd(v322[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v325);
            const Vector v327 = Vector::FusedMultiplyAdd(v322[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v326);
            const Vector v328 = (v327 - v322[3]);
            const Vector v329 = (v322[1] - v322[2]);
            const Vector v330 = (v322[3] - v322[0]);
            const Vector v331 = Vector::FusedMultiplyAdd(v329, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v330);
            const Vector v332 = (v284 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v333 = Vector::FusedMultiplyAdd(v284, v331, v328);
            const Vector v334 = Vector::FusedMultiplyAdd(v284, v333, v323);
            const Vector v335 = Vector::FusedMultiplyAdd(v332, v334, v322[1]);
            v336 = v335;
        }

        const Vector v338 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v270, v336);
        const Vector v339 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v338, v270);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v339);
        [[maybe_unused]] const auto v340 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.NextSpline();
        const Vector v341 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v343 = Vector::IfLess(v340, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v340, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v344 = Vector::IfLess(v340, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v340, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v346 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v343, v344, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v347 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v346);
        const Vector v348 = (v341 * v347);
        const Vector v349 = (-v348);
        const Vector v350 = v349.Floor();
        const Vector v351 = (-v350);
        const Vector v352 = (v351 - v348);
        const std::uint64_t v353 = CodegenInteger::ToWord(v351[0]);
        const std::uint64_t v354 = (static_cast<std::uint64_t>(6899ULL) + v353);
        const std::uint64_t v355 = (v354 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v356 = (static_cast<std::uint64_t>(6899ULL) + v353);
        const std::uint64_t v357 = (v356 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v358 = (static_cast<std::uint64_t>(6899ULL) + v353);
        const std::uint64_t v359 = (v358 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v360 = (static_cast<std::uint64_t>(6899ULL) + v353);
        const std::uint64_t v361 = (v360 - static_cast<std::uint64_t>(128ULL));
        const bool v362 = (static_cast<std::uint64_t>(6899ULL) == static_cast<std::uint64_t>(6899ULL));
        bool v367;
        if (v362)
        {
            const bool v363 = (static_cast<std::uint64_t>(6899ULL) == static_cast<std::uint64_t>(6899ULL));
            bool v366;
            if (v363)
            {
                const bool v364 = (static_cast<std::uint64_t>(6899ULL) == static_cast<std::uint64_t>(6899ULL));
                bool v365;
                if (v364)
                {
                    v365 = true;
                }
                else
                {
                    v365 = false;
                }

                v366 = v365;
            }
            else
            {
                v366 = false;
            }

            v367 = v366;
        }
        else
        {
            v367 = false;
        }

        const bool v368 = (v367 == true);
        Vector v404;
        if (v368)
        {
            [[maybe_unused]] const auto v369 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v355);
            const std::uint64_t v370 = (v355 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v371 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v370);
            const std::uint64_t v372 = (v355 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v373 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v372);
            const std::uint64_t v374 = (v355 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v375 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v374);
            const Vector v376 = (v373 - v369);
            const Vector v378 = (v373 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v379 = Vector::FusedMultiplyAdd(v371, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v378);
            const Vector v380 = Vector::FusedMultiplyAdd(v369, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v379);
            const Vector v381 = (v380 - v375);
            const Vector v382 = (v371 - v373);
            const Vector v383 = (v375 - v369);
            const Vector v384 = Vector::FusedMultiplyAdd(v382, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v383);
            const Vector v385 = (v352 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v386 = Vector::FusedMultiplyAdd(v352, v384, v381);
            const Vector v387 = Vector::FusedMultiplyAdd(v352, v386, v376);
            const Vector v388 = Vector::FusedMultiplyAdd(v385, v387, v371);
            v404 = v388;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v389 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v355), static_cast<std::uint32_t>(v357), static_cast<std::uint32_t>(v359), static_cast<std::uint32_t>(v361)};
            [[maybe_unused]] const auto v390 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v389, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v391 = (v390[2] - v390[0]);
            const Vector v393 = (v390[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v394 = Vector::FusedMultiplyAdd(v390[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v393);
            const Vector v395 = Vector::FusedMultiplyAdd(v390[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v394);
            const Vector v396 = (v395 - v390[3]);
            const Vector v397 = (v390[1] - v390[2]);
            const Vector v398 = (v390[3] - v390[0]);
            const Vector v399 = Vector::FusedMultiplyAdd(v397, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v398);
            const Vector v400 = (v352 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v401 = Vector::FusedMultiplyAdd(v352, v399, v396);
            const Vector v402 = Vector::FusedMultiplyAdd(v352, v401, v391);
            const Vector v403 = Vector::FusedMultiplyAdd(v400, v402, v390[1]);
            v404 = v403;
        }

        const Vector v406 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v338, v404);
        const Vector v407 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v406, v338);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v407);
        [[maybe_unused]] const auto v408 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.NextSpline();
        const Vector v409 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v411 = Vector::IfLess(v408, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v408, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v412 = Vector::IfLess(v408, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v408, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v414 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v411, v412, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v415 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v414);
        const Vector v416 = (v409 * v415);
        const Vector v417 = (-v416);
        const Vector v418 = v417.Floor();
        const Vector v419 = (-v418);
        const Vector v420 = (v419 - v416);
        const std::uint64_t v421 = CodegenInteger::ToWord(v419[0]);
        const std::uint64_t v422 = (static_cast<std::uint64_t>(9811ULL) + v421);
        const std::uint64_t v423 = (v422 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v424 = (static_cast<std::uint64_t>(9811ULL) + v421);
        const std::uint64_t v425 = (v424 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v426 = (static_cast<std::uint64_t>(9811ULL) + v421);
        const std::uint64_t v427 = (v426 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v428 = (static_cast<std::uint64_t>(9811ULL) + v421);
        const std::uint64_t v429 = (v428 - static_cast<std::uint64_t>(128ULL));
        const bool v430 = (static_cast<std::uint64_t>(9811ULL) == static_cast<std::uint64_t>(9811ULL));
        bool v435;
        if (v430)
        {
            const bool v431 = (static_cast<std::uint64_t>(9811ULL) == static_cast<std::uint64_t>(9811ULL));
            bool v434;
            if (v431)
            {
                const bool v432 = (static_cast<std::uint64_t>(9811ULL) == static_cast<std::uint64_t>(9811ULL));
                bool v433;
                if (v432)
                {
                    v433 = true;
                }
                else
                {
                    v433 = false;
                }

                v434 = v433;
            }
            else
            {
                v434 = false;
            }

            v435 = v434;
        }
        else
        {
            v435 = false;
        }

        const bool v436 = (v435 == true);
        Vector v472;
        if (v436)
        {
            [[maybe_unused]] const auto v437 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v423);
            const std::uint64_t v438 = (v423 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v439 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v438);
            const std::uint64_t v440 = (v423 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v441 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v440);
            const std::uint64_t v442 = (v423 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v443 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v442);
            const Vector v444 = (v441 - v437);
            const Vector v446 = (v441 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v447 = Vector::FusedMultiplyAdd(v439, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v446);
            const Vector v448 = Vector::FusedMultiplyAdd(v437, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v447);
            const Vector v449 = (v448 - v443);
            const Vector v450 = (v439 - v441);
            const Vector v451 = (v443 - v437);
            const Vector v452 = Vector::FusedMultiplyAdd(v450, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v451);
            const Vector v453 = (v420 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v454 = Vector::FusedMultiplyAdd(v420, v452, v449);
            const Vector v455 = Vector::FusedMultiplyAdd(v420, v454, v444);
            const Vector v456 = Vector::FusedMultiplyAdd(v453, v455, v439);
            v472 = v456;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v457 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v423), static_cast<std::uint32_t>(v425), static_cast<std::uint32_t>(v427), static_cast<std::uint32_t>(v429)};
            [[maybe_unused]] const auto v458 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v457, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v459 = (v458[2] - v458[0]);
            const Vector v461 = (v458[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v462 = Vector::FusedMultiplyAdd(v458[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v461);
            const Vector v463 = Vector::FusedMultiplyAdd(v458[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v462);
            const Vector v464 = (v463 - v458[3]);
            const Vector v465 = (v458[1] - v458[2]);
            const Vector v466 = (v458[3] - v458[0]);
            const Vector v467 = Vector::FusedMultiplyAdd(v465, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v466);
            const Vector v468 = (v420 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v469 = Vector::FusedMultiplyAdd(v420, v467, v464);
            const Vector v470 = Vector::FusedMultiplyAdd(v420, v469, v459);
            const Vector v471 = Vector::FusedMultiplyAdd(v468, v470, v458[1]);
            v472 = v471;
        }

        const Vector v474 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v406, v472);
        const Vector v475 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v474, v406);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v475);
        [[maybe_unused]] const auto v476 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_clock.NextSpline();
        const Vector v477 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v479 = Vector::IfLess(v476, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v476, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v480 = Vector::IfLess(v476, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v476, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v482 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v479, v480, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v483 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v482);
        const Vector v484 = (v477 * v483);
        const Vector v485 = (-v484);
        const Vector v486 = v485.Floor();
        const Vector v487 = (-v486);
        const Vector v488 = (v487 - v484);
        const std::uint64_t v489 = CodegenInteger::ToWord(v487[0]);
        const std::uint64_t v490 = (static_cast<std::uint64_t>(14033ULL) + v489);
        const std::uint64_t v491 = (v490 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v492 = (static_cast<std::uint64_t>(14033ULL) + v489);
        const std::uint64_t v493 = (v492 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v494 = (static_cast<std::uint64_t>(14033ULL) + v489);
        const std::uint64_t v495 = (v494 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v496 = (static_cast<std::uint64_t>(14033ULL) + v489);
        const std::uint64_t v497 = (v496 - static_cast<std::uint64_t>(128ULL));
        const bool v498 = (static_cast<std::uint64_t>(14033ULL) == static_cast<std::uint64_t>(14033ULL));
        bool v503;
        if (v498)
        {
            const bool v499 = (static_cast<std::uint64_t>(14033ULL) == static_cast<std::uint64_t>(14033ULL));
            bool v502;
            if (v499)
            {
                const bool v500 = (static_cast<std::uint64_t>(14033ULL) == static_cast<std::uint64_t>(14033ULL));
                bool v501;
                if (v500)
                {
                    v501 = true;
                }
                else
                {
                    v501 = false;
                }

                v502 = v501;
            }
            else
            {
                v502 = false;
            }

            v503 = v502;
        }
        else
        {
            v503 = false;
        }

        const bool v504 = (v503 == true);
        Vector v540;
        if (v504)
        {
            [[maybe_unused]] const auto v505 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v491);
            const std::uint64_t v506 = (v491 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v507 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v506);
            const std::uint64_t v508 = (v491 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v509 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v508);
            const std::uint64_t v510 = (v491 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v511 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadWrappedAge(v510);
            const Vector v512 = (v509 - v505);
            const Vector v514 = (v509 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v515 = Vector::FusedMultiplyAdd(v507, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v514);
            const Vector v516 = Vector::FusedMultiplyAdd(v505, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v515);
            const Vector v517 = (v516 - v511);
            const Vector v518 = (v507 - v509);
            const Vector v519 = (v511 - v505);
            const Vector v520 = Vector::FusedMultiplyAdd(v518, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v519);
            const Vector v521 = (v488 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v522 = Vector::FusedMultiplyAdd(v488, v520, v517);
            const Vector v523 = Vector::FusedMultiplyAdd(v488, v522, v512);
            const Vector v524 = Vector::FusedMultiplyAdd(v521, v523, v507);
            v540 = v524;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v525 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v491), static_cast<std::uint32_t>(v493), static_cast<std::uint32_t>(v495), static_cast<std::uint32_t>(v497)};
            [[maybe_unused]] const auto v526 = state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.ReadAgeOffsets(v525, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v527 = (v526[2] - v526[0]);
            const Vector v529 = (v526[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v530 = Vector::FusedMultiplyAdd(v526[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v529);
            const Vector v531 = Vector::FusedMultiplyAdd(v526[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v530);
            const Vector v532 = (v531 - v526[3]);
            const Vector v533 = (v526[1] - v526[2]);
            const Vector v534 = (v526[3] - v526[0]);
            const Vector v535 = Vector::FusedMultiplyAdd(v533, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v534);
            const Vector v536 = (v488 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v537 = Vector::FusedMultiplyAdd(v488, v535, v532);
            const Vector v538 = Vector::FusedMultiplyAdd(v488, v537, v527);
            const Vector v539 = Vector::FusedMultiplyAdd(v536, v538, v526[1]);
            v540 = v539;
        }

        const Vector v542 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v474, v540);
        const Vector v543 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v542, v474);
        state.m_diffusion.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_snd.m_fst.m_history.Push(v543);
        const Vector v544 = state.m_wet;
        const Vector v545 = state.m_settings.m_feedback;
        const Vector v546 = (v544 * v545);
        const Vector v547 = (v542 + v546);
        state.m_delay.Push(v547);
        const Vector v548 = state.m_settings.m_delaySamples;
        const Vector v549 = (-v548);
        const Vector v550 = v549.Floor();
        const Vector v551 = (-v550);
        const Vector v552 = (v551 - v548);
        static_assert(Size == 4);
        const MultiChannelNumber<std::uint32_t, 4> v553 = v551.template Convert<std::uint32_t>();
        [[maybe_unused]] const auto v554 = state.m_delay.ReadAgeOffsets(v553, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
        const Vector v555 = (v554[2] - v554[0]);
        const Vector v557 = (v554[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
        const Vector v558 = Vector::FusedMultiplyAdd(v554[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v557);
        const Vector v559 = Vector::FusedMultiplyAdd(v554[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v558);
        const Vector v560 = (v559 - v554[3]);
        const Vector v561 = (v554[1] - v554[2]);
        const Vector v562 = (v554[3] - v554[0]);
        const Vector v563 = Vector::FusedMultiplyAdd(v561, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v562);
        const Vector v564 = (v552 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v565 = Vector::FusedMultiplyAdd(v552, v563, v560);
        const Vector v566 = Vector::FusedMultiplyAdd(v552, v565, v555);
        const Vector v567 = Vector::FusedMultiplyAdd(v564, v566, v554[1]);
        const Vector v569 = Vector::Splat(v567[0]);
        const Vector v570 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb9611ap-24f : 0x172c234f72c235p-53)) * v569);
        const Vector v572 = Vector::Splat(v567[1]);
        const Vector v573 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb08d3ep-24f : 0x1611a7b9611a7cp-53)) * v572);
        const Vector v574 = (v570 - v573);
        const Vector v576 = Vector::Splat(v567[0]);
        const Vector v577 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb08d3ep-24f : 0x1611a7b9611a7cp-53)) * v576);
        const Vector v579 = Vector::Splat(v567[1]);
        const Vector v580 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xb9611ap-24f : 0x172c234f72c235p-53)) * v579);
        const Vector v581 = (v577 + v580);
        const Vector v583 = Vector::Splat(v567[2]);
        const Vector v584 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd95bc6p-24f : 0x1b2b78c13521d0p-53)) * v583);
        const Vector v586 = Vector::Splat(v567[3]);
        const Vector v587 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x873ecbp-24f : 0x10e7d95bc609a9p-53)) * v586);
        const Vector v588 = (v584 - v587);
        const Vector v590 = Vector::Splat(v567[2]);
        const Vector v591 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x873ecbp-24f : 0x10e7d95bc609a9p-53)) * v590);
        const Vector v593 = Vector::Splat(v567[3]);
        const Vector v594 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xd95bc6p-24f : 0x1b2b78c13521d0p-53)) * v593);
        const Vector v595 = (v591 + v594);
        const Vector v597 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc0e070p-24f : 0x181c0e070381c1p-53)) * v574);
        const Vector v600 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa8542ap-24f : -0x150a8542a150a8p-53)) * v588);
        const Vector v601 = (v597 - v600);
        const Vector v604 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa8542ap-24f : -0x150a8542a150a8p-53)) * v574);
        const Vector v606 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc0e070p-24f : 0x181c0e070381c1p-53)) * v588);
        const Vector v607 = (v604 + v606);
        const Vector v609 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xab8be0p-24f : 0x15717c0a8e83f5p-53)) * v581);
        const Vector v611 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xbe0547p-24f : 0x17c0a8e83f5718p-53)) * v595);
        const Vector v612 = (v609 - v611);
        const Vector v614 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xbe0547p-24f : 0x17c0a8e83f5718p-53)) * v581);
        const Vector v616 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xab8be0p-24f : 0x15717c0a8e83f5p-53)) * v595);
        const Vector v617 = (v614 + v616);
        static_assert(Size == 4);
        const Vector v618 = Vector{v601[0], v612[1], v607[2], v617[3]};
        [[maybe_unused]] const auto v619 = state.m_first.m_clock.NextSpline();
        const Vector v620 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v622 = Vector::IfLess(v619, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v619, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v623 = Vector::IfLess(v619, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v619, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v625 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v622, v623, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v626 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v625);
        const Vector v627 = (v620 * v626);
        const Vector v628 = (-v627);
        const Vector v629 = v628.Floor();
        const Vector v630 = (-v629);
        const Vector v631 = (v630 - v627);
        const std::uint64_t v632 = CodegenInteger::ToWord(v630[0]);
        const std::uint64_t v633 = (static_cast<std::uint64_t>(2039ULL) + v632);
        const std::uint64_t v634 = (v633 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v635 = (static_cast<std::uint64_t>(3719ULL) + v632);
        const std::uint64_t v636 = (v635 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v637 = (static_cast<std::uint64_t>(6359ULL) + v632);
        const std::uint64_t v638 = (v637 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v639 = (static_cast<std::uint64_t>(2753ULL) + v632);
        const std::uint64_t v640 = (v639 - static_cast<std::uint64_t>(128ULL));
        const bool v641 = (static_cast<std::uint64_t>(2039ULL) == static_cast<std::uint64_t>(3719ULL));
        bool v646;
        if (v641)
        {
            const bool v642 = (static_cast<std::uint64_t>(2039ULL) == static_cast<std::uint64_t>(6359ULL));
            bool v645;
            if (v642)
            {
                const bool v643 = (static_cast<std::uint64_t>(2039ULL) == static_cast<std::uint64_t>(2753ULL));
                bool v644;
                if (v643)
                {
                    v644 = true;
                }
                else
                {
                    v644 = false;
                }

                v645 = v644;
            }
            else
            {
                v645 = false;
            }

            v646 = v645;
        }
        else
        {
            v646 = false;
        }

        const bool v647 = (v646 == true);
        Vector v683;
        if (v647)
        {
            [[maybe_unused]] const auto v648 = state.m_first.m_history.ReadWrappedAge(v634);
            const std::uint64_t v649 = (v634 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v650 = state.m_first.m_history.ReadWrappedAge(v649);
            const std::uint64_t v651 = (v634 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v652 = state.m_first.m_history.ReadWrappedAge(v651);
            const std::uint64_t v653 = (v634 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v654 = state.m_first.m_history.ReadWrappedAge(v653);
            const Vector v655 = (v652 - v648);
            const Vector v657 = (v652 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v658 = Vector::FusedMultiplyAdd(v650, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v657);
            const Vector v659 = Vector::FusedMultiplyAdd(v648, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v658);
            const Vector v660 = (v659 - v654);
            const Vector v661 = (v650 - v652);
            const Vector v662 = (v654 - v648);
            const Vector v663 = Vector::FusedMultiplyAdd(v661, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v662);
            const Vector v664 = (v631 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v665 = Vector::FusedMultiplyAdd(v631, v663, v660);
            const Vector v666 = Vector::FusedMultiplyAdd(v631, v665, v655);
            const Vector v667 = Vector::FusedMultiplyAdd(v664, v666, v650);
            v683 = v667;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v668 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v634), static_cast<std::uint32_t>(v636), static_cast<std::uint32_t>(v638), static_cast<std::uint32_t>(v640)};
            [[maybe_unused]] const auto v669 = state.m_first.m_history.ReadAgeOffsets(v668, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v670 = (v669[2] - v669[0]);
            const Vector v672 = (v669[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v673 = Vector::FusedMultiplyAdd(v669[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v672);
            const Vector v674 = Vector::FusedMultiplyAdd(v669[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v673);
            const Vector v675 = (v674 - v669[3]);
            const Vector v676 = (v669[1] - v669[2]);
            const Vector v677 = (v669[3] - v669[0]);
            const Vector v678 = Vector::FusedMultiplyAdd(v676, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v677);
            const Vector v679 = (v631 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v680 = Vector::FusedMultiplyAdd(v631, v678, v675);
            const Vector v681 = Vector::FusedMultiplyAdd(v631, v680, v670);
            const Vector v682 = Vector::FusedMultiplyAdd(v679, v681, v669[1]);
            v683 = v682;
        }

        const Vector v685 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v618, v683);
        const Vector v686 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v685, v618);
        state.m_first.m_history.Push(v686);
        const Vector v687 = state.m_settings.m_saturatorGain;
        const Vector v688 = (v685 * v687);
        const Vector v689 = (v688 * v688);
        const Vector v690 = (v688 * (v689 + Vector::Splat(27)));
        const Vector v691 = Vector::FusedMultiplyAdd(v688 * Vector::Splat(9), v688, Vector::Splat(27));
        const Vector v692 = (v690 / v691);
        const Vector v693 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v692));
        const Vector v694 = (v693 - state.m_damping.m_filter.m_highPass.m_lowPass.m_s);
        const Vector v695 = Vector::FusedMultiplyAdd(v694, state.m_damping.m_filter.m_highPass.m_lowPass.m_G, state.m_damping.m_filter.m_highPass.m_lowPass.m_s);
        const Vector v696 = Vector::FusedMultiplyAdd(v694, state.m_damping.m_filter.m_highPass.m_lowPass.m_G, v695);
        const Vector v697 = (v685 * v687);
        const Vector v698 = (v697 * v697);
        const Vector v699 = (v697 * (v698 + Vector::Splat(27)));
        const Vector v700 = Vector::FusedMultiplyAdd(v697 * Vector::Splat(9), v697, Vector::Splat(27));
        const Vector v701 = (v699 / v700);
        const Vector v702 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v701));
        const Vector v703 = (v702 - v695);
        const Vector v704 = (v703 - state.m_damping.m_filter.m_lowPass.m_s);
        const Vector v705 = Vector::FusedMultiplyAdd(v704, state.m_damping.m_filter.m_lowPass.m_G, state.m_damping.m_filter.m_lowPass.m_s);
        const Vector v706 = Vector::FusedMultiplyAdd(v704, state.m_damping.m_filter.m_lowPass.m_G, v705);
        state.m_damping.m_filter.m_highPass.m_lowPass.m_s = v696;
        state.m_damping.m_filter.m_lowPass.m_s = v706;
        [[maybe_unused]] const auto v707 = state.m_second.m_clock.NextSpline();
        const Vector v708 = Vector::Splat(static_cast<Number>(static_cast<std::uint64_t>(128ULL)));
        const Vector v710 = Vector::IfLess(v707, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v707, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v711 = Vector::IfLess(v707, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)), v707, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v713 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)), v710, v711, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x800000p-23f : -0x10000000000000p-52)));
        const Vector v714 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v713);
        const Vector v715 = (v708 * v714);
        const Vector v716 = (-v715);
        const Vector v717 = v716.Floor();
        const Vector v718 = (-v717);
        const Vector v719 = (v718 - v715);
        const std::uint64_t v720 = CodegenInteger::ToWord(v718[0]);
        const std::uint64_t v721 = (static_cast<std::uint64_t>(11923ULL) + v720);
        const std::uint64_t v722 = (v721 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v723 = (static_cast<std::uint64_t>(16879ULL) + v720);
        const std::uint64_t v724 = (v723 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v725 = (static_cast<std::uint64_t>(9041ULL) + v720);
        const std::uint64_t v726 = (v725 - static_cast<std::uint64_t>(128ULL));
        const std::uint64_t v727 = (static_cast<std::uint64_t>(23447ULL) + v720);
        const std::uint64_t v728 = (v727 - static_cast<std::uint64_t>(128ULL));
        const bool v729 = (static_cast<std::uint64_t>(11923ULL) == static_cast<std::uint64_t>(16879ULL));
        bool v734;
        if (v729)
        {
            const bool v730 = (static_cast<std::uint64_t>(11923ULL) == static_cast<std::uint64_t>(9041ULL));
            bool v733;
            if (v730)
            {
                const bool v731 = (static_cast<std::uint64_t>(11923ULL) == static_cast<std::uint64_t>(23447ULL));
                bool v732;
                if (v731)
                {
                    v732 = true;
                }
                else
                {
                    v732 = false;
                }

                v733 = v732;
            }
            else
            {
                v733 = false;
            }

            v734 = v733;
        }
        else
        {
            v734 = false;
        }

        const bool v735 = (v734 == true);
        Vector v771;
        if (v735)
        {
            [[maybe_unused]] const auto v736 = state.m_second.m_history.ReadWrappedAge(v722);
            const std::uint64_t v737 = (v722 - static_cast<std::uint64_t>(1ULL));
            [[maybe_unused]] const auto v738 = state.m_second.m_history.ReadWrappedAge(v737);
            const std::uint64_t v739 = (v722 - static_cast<std::uint64_t>(2ULL));
            [[maybe_unused]] const auto v740 = state.m_second.m_history.ReadWrappedAge(v739);
            const std::uint64_t v741 = (v722 - static_cast<std::uint64_t>(3ULL));
            [[maybe_unused]] const auto v742 = state.m_second.m_history.ReadWrappedAge(v741);
            const Vector v743 = (v740 - v736);
            const Vector v745 = (v740 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v746 = Vector::FusedMultiplyAdd(v738, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v745);
            const Vector v747 = Vector::FusedMultiplyAdd(v736, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v746);
            const Vector v748 = (v747 - v742);
            const Vector v749 = (v738 - v740);
            const Vector v750 = (v742 - v736);
            const Vector v751 = Vector::FusedMultiplyAdd(v749, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v750);
            const Vector v752 = (v719 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v753 = Vector::FusedMultiplyAdd(v719, v751, v748);
            const Vector v754 = Vector::FusedMultiplyAdd(v719, v753, v743);
            const Vector v755 = Vector::FusedMultiplyAdd(v752, v754, v738);
            v771 = v755;
        }
        else
        {
            const MultiChannelNumber<std::uint32_t, 4> v756 = MultiChannelNumber<std::uint32_t, 4>{static_cast<std::uint32_t>(v722), static_cast<std::uint32_t>(v724), static_cast<std::uint32_t>(v726), static_cast<std::uint32_t>(v728)};
            [[maybe_unused]] const auto v757 = state.m_second.m_history.ReadAgeOffsets(v756, std::array<std::uint64_t, 4>{{static_cast<std::uint64_t>(0ULL), static_cast<std::uint64_t>(1ULL), static_cast<std::uint64_t>(2ULL), static_cast<std::uint64_t>(3ULL)}});
            const Vector v758 = (v757[2] - v757[0]);
            const Vector v760 = (v757[2] * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-21f : 0x10000000000000p-50)));
            const Vector v761 = Vector::FusedMultiplyAdd(v757[1], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0xa00000p-21f : -0x14000000000000p-50)), v760);
            const Vector v762 = Vector::FusedMultiplyAdd(v757[0], Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v761);
            const Vector v763 = (v762 - v757[3]);
            const Vector v764 = (v757[1] - v757[2]);
            const Vector v765 = (v757[3] - v757[0]);
            const Vector v766 = Vector::FusedMultiplyAdd(v764, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xc00000p-22f : 0x18000000000000p-51)), v765);
            const Vector v767 = (v719 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
            const Vector v768 = Vector::FusedMultiplyAdd(v719, v766, v763);
            const Vector v769 = Vector::FusedMultiplyAdd(v719, v768, v758);
            const Vector v770 = Vector::FusedMultiplyAdd(v767, v769, v757[1]);
            v771 = v770;
        }

        const Vector v773 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? -0x99999ap-24f : -0x13333333333333p-53)), v705, v771);
        const Vector v774 = Vector::FusedMultiplyAdd(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x99999ap-24f : 0x13333333333333p-53)), v773, v705);
        state.m_second.m_history.Push(v774);
        state.m_wet = v773;
        return v773;
    }

};
