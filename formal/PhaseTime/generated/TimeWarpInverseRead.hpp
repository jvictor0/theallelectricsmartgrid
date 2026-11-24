#pragma once

// Generated from PhaseTime.InverseRuntime.readPayloadFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct TimeWarpInverseRead
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State, typename Input>
    static double Process(State& state, [[maybe_unused]] const Input& input)
    {
        [[maybe_unused]] const auto v0 = state.LoadFrontier();
        bool v2;
        if (v0.m_present)
        {
            const bool v1 = (input.m_phase == v0.m_value.m_phase);
            v2 = v1;
        }
        else
        {
            v2 = false;
        }

        const bool v3 = (v2 == true);
        bool v122;
        double v123;
        if (v3)
        {
            bool v9;
            double v10;
            if (v0.m_present)
            {
                const bool v4 = (v0.m_value.m_time <= input.m_cutoff);
                bool v5;
                if (v4)
                {
                    v5 = true;
                }
                else
                {
                    v5 = false;
                }

                const bool v6 = (v5 == true);
                bool v7;
                double v8;
                if (v6)
                {
                    v7 = true;
                    v8 = v0.m_value.m_time;
                }
                else
                {
                    v7 = false;
                    v8 = double{};
                }

                v9 = v7;
                v10 = v8;
            }
            else
            {
                v9 = false;
                v10 = double{};
            }

            v122 = v9;
            v123 = v10;
        }
        else
        {
            const double v11 = std::floor(input.m_phase);
            const std::int64_t v12 = static_cast<std::int64_t>(v11);
            [[maybe_unused]] const auto v13 = state.LoadBase(v12);
            bool v120;
            double v121;
            if (v13.m_present)
            {
                [[maybe_unused]] const auto v14 = state.LoadCut(v12);
                bool v17;
                if (v14.m_present)
                {
                    const bool v15 = (v14.m_value <= input.m_phase);
                    bool v16;
                    if (v15)
                    {
                        v16 = true;
                    }
                    else
                    {
                        v16 = false;
                    }

                    v17 = v16;
                }
                else
                {
                    v17 = false;
                }

                const bool v18 = (v17 == true);
                double v37;
                bool v38;
                double v39;
                bool v40;
                double v41;
                if (v18)
                {
                    [[maybe_unused]] const auto v19 = state.LoadStartDelta(v12);
                    [[maybe_unused]] const auto v20 = state.LoadEndDelta(v12);
                    double v21;
                    if (v14.m_present)
                    {
                        v21 = v14.m_value;
                    }
                    else
                    {
                        v21 = input.m_phase;
                    }

                    bool v24;
                    double v25;
                    if (v19.m_present)
                    {
                        const double v22 = static_cast<double>(v19.m_value);
                        const double v23 = (v13.m_value + v22);
                        v24 = true;
                        v25 = v23;
                    }
                    else
                    {
                        v24 = false;
                        v25 = double{};
                    }

                    bool v28;
                    double v29;
                    if (v20.m_present)
                    {
                        const double v26 = static_cast<double>(v20.m_value);
                        const double v27 = (v13.m_value + v26);
                        v28 = true;
                        v29 = v27;
                    }
                    else
                    {
                        v28 = false;
                        v29 = double{};
                    }

                    v37 = v21;
                    v38 = v24;
                    v39 = v25;
                    v40 = v28;
                    v41 = v29;
                }
                else
                {
                    [[maybe_unused]] const auto v30 = state.LoadSeed();
                    [[maybe_unused]] const auto v31 = state.LoadFallback(v12);
                    double v36;
                    if (v30.m_present)
                    {
                        const bool v32 = (v30.m_value.m_index == v12);
                        double v34;
                        if (v32)
                        {
                            v34 = v30.m_value.m_phase;
                        }
                        else
                        {
                            const double v33 = static_cast<double>(v12);
                            v34 = v33;
                        }

                        v36 = v34;
                    }
                    else
                    {
                        const double v35 = static_cast<double>(v12);
                        v36 = v35;
                    }

                    v37 = v36;
                    v38 = true;
                    v39 = v13.m_value;
                    v40 = v31.m_present;
                    v41 = v31.m_value;
                }

                bool v118;
                double v119;
                if (v38)
                {
                    const bool v42 = (v40 == true);
                    double v76;
                    bool v77;
                    double v78;
                    if (v42)
                    {
                        const double v43 = std::floor(input.m_phase);
                        const std::int64_t v44 = static_cast<std::int64_t>(v43);
                        const std::int64_t v45 = CodegenInteger::FromIndex(1);
                        const std::int64_t v46 = (v44 + v45);
                        const double v47 = static_cast<double>(v46);
                        v76 = v47;
                        v77 = v40;
                        v78 = v41;
                    }
                    else
                    {
                        bool v58;
                        if (v0.m_present)
                        {
                            const bool v48 = (v37 <= v0.m_value.m_phase);
                            bool v49;
                            if (v48)
                            {
                                v49 = true;
                            }
                            else
                            {
                                v49 = false;
                            }

                            bool v57;
                            if (v49)
                            {
                                const double v50 = std::floor(input.m_phase);
                                const std::int64_t v51 = static_cast<std::int64_t>(v50);
                                const std::int64_t v52 = CodegenInteger::FromIndex(1);
                                const std::int64_t v53 = (v51 + v52);
                                const double v54 = static_cast<double>(v53);
                                const bool v55 = (v0.m_value.m_phase < v54);
                                bool v56;
                                if (v55)
                                {
                                    v56 = true;
                                }
                                else
                                {
                                    v56 = false;
                                }

                                v57 = v56;
                            }
                            else
                            {
                                v57 = false;
                            }

                            v58 = v57;
                        }
                        else
                        {
                            v58 = false;
                        }

                        const bool v59 = (v58 == true);
                        double v73;
                        bool v74;
                        double v75;
                        if (v59)
                        {
                            double v65;
                            if (v0.m_present)
                            {
                                v65 = v0.m_value.m_phase;
                            }
                            else
                            {
                                const double v60 = std::floor(input.m_phase);
                                const std::int64_t v61 = static_cast<std::int64_t>(v60);
                                const std::int64_t v62 = CodegenInteger::FromIndex(1);
                                const std::int64_t v63 = (v61 + v62);
                                const double v64 = static_cast<double>(v63);
                                v65 = v64;
                            }

                            bool v66;
                            double v67;
                            if (v0.m_present)
                            {
                                v66 = true;
                                v67 = v0.m_value.m_time;
                            }
                            else
                            {
                                v66 = false;
                                v67 = double{};
                            }

                            v73 = v65;
                            v74 = v66;
                            v75 = v67;
                        }
                        else
                        {
                            const double v68 = std::floor(input.m_phase);
                            const std::int64_t v69 = static_cast<std::int64_t>(v68);
                            const std::int64_t v70 = CodegenInteger::FromIndex(1);
                            const std::int64_t v71 = (v69 + v70);
                            const double v72 = static_cast<double>(v71);
                            v73 = v72;
                            v74 = false;
                            v75 = double{};
                        }

                        v76 = v73;
                        v77 = v74;
                        v78 = v75;
                    }

                    double v79;
                    if (v77)
                    {
                        v79 = v78;
                    }
                    else
                    {
                        v79 = v39;
                    }

                    const bool v80 = (input.m_phase < v37);
                    bool v81;
                    if (v80)
                    {
                        v81 = true;
                    }
                    else
                    {
                        v81 = false;
                    }

                    const bool v82 = !v81;
                    bool v86;
                    if (v82)
                    {
                        const bool v83 = (input.m_cutoff < v39);
                        bool v84;
                        if (v83)
                        {
                            v84 = true;
                        }
                        else
                        {
                            v84 = false;
                        }

                        const bool v85 = !v84;
                        v86 = v85;
                    }
                    else
                    {
                        v86 = false;
                    }

                    const bool v87 = !v86;
                    const bool v88 = (v87 == true);
                    bool v116;
                    double v117;
                    if (v88)
                    {
                        v116 = false;
                        v117 = double{};
                    }
                    else
                    {
                        const bool v89 = (input.m_phase == v37);
                        const bool v90 = (v89 == true);
                        bool v114;
                        double v115;
                        if (v90)
                        {
                            v114 = true;
                            v115 = v39;
                        }
                        else
                        {
                            const bool v91 = !v77;
                            bool v99;
                            if (v91)
                            {
                                v99 = true;
                            }
                            else
                            {
                                const bool v92 = (v76 < input.m_phase);
                                bool v93;
                                if (v92)
                                {
                                    v93 = true;
                                }
                                else
                                {
                                    v93 = false;
                                }

                                const bool v94 = !v93;
                                bool v97;
                                if (v94)
                                {
                                    const bool v95 = (v37 < v76);
                                    bool v96;
                                    if (v95)
                                    {
                                        v96 = true;
                                    }
                                    else
                                    {
                                        v96 = false;
                                    }

                                    v97 = v96;
                                }
                                else
                                {
                                    v97 = false;
                                }

                                const bool v98 = !v97;
                                v99 = v98;
                            }

                            const bool v100 = (v99 == true);
                            bool v112;
                            double v113;
                            if (v100)
                            {
                                v112 = false;
                                v113 = double{};
                            }
                            else
                            {
                                const double v101 = (input.m_phase - v37);
                                const double v102 = (v76 - v37);
                                const double v103 = (v101 / v102);
                                const double v104 = (v79 - v39);
                                const double v105 = (v103 * v104);
                                const double v106 = (v39 + v105);
                                const bool v107 = (v106 <= input.m_cutoff);
                                bool v108;
                                if (v107)
                                {
                                    v108 = true;
                                }
                                else
                                {
                                    v108 = false;
                                }

                                const bool v109 = (v108 == true);
                                bool v110;
                                double v111;
                                if (v109)
                                {
                                    v110 = true;
                                    v111 = v106;
                                }
                                else
                                {
                                    v110 = false;
                                    v111 = double{};
                                }

                                v112 = v110;
                                v113 = v111;
                            }

                            v114 = v112;
                            v115 = v113;
                        }

                        v116 = v114;
                        v117 = v115;
                    }

                    v118 = v116;
                    v119 = v117;
                }
                else
                {
                    v118 = false;
                    v119 = double{};
                }

                v120 = v118;
                v121 = v119;
            }
            else
            {
                v120 = false;
                v121 = double{};
            }

            v122 = v120;
            v123 = v121;
        }

        double v124;
        if (v122)
        {
            v124 = v123;
        }
        else
        {
            v124 = input.m_missing;
        }

        return v124;
    }

};
