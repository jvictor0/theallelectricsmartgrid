#pragma once

// Generated from DSP.Resynthesis.PVDR.Storage.analyze64. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct AnalyzePVDR64
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State, typename Input>
    static void Process(State& state, [[maybe_unused]] const Input& input)
    {
        [[maybe_unused]] double loop_v0;
        loop_v0 = 0.0;
        for (std::size_t v1 = 0; v1 < 2047; ++v1)
        {
            [[maybe_unused]] const auto v2 = state.Magnitude(v1);
            [[maybe_unused]] const auto v3 = state.PreviousMagnitude(v1);
            const bool v4 = (loop_v0 <= v2);
            double v5;
            if (v4)
            {
                v5 = v2;
            }
            else
            {
                v5 = loop_v0;
            }

            const bool v6 = (v5 <= v3);
            double v9;
            if (v6)
            {
                v9 = v3;
            }
            else
            {
                const bool v7 = (loop_v0 <= v2);
                double v8;
                if (v7)
                {
                    v8 = v2;
                }
                else
                {
                    v8 = loop_v0;
                }

                v9 = v8;
            }

            loop_v0 = v9;
        }

        const bool v10 = (input.m_schmitt == true);
        double v11;
        if (v10)
        {
            v11 = 0x10763f01e8e5aep-64;
        }
        else
        {
            v11 = input.m_fixedFloor;
        }

        const double v12 = (loop_v0 * v11);
        const double v13 = (loop_v0 * 0x10624dd2f1a9fcp-62);
        struct loop_v14_Type
        {
        };

        [[maybe_unused]] loop_v14_Type loop_v14;
        for (std::size_t v15 = 0; v15 < 2047; ++v15)
        {
            [[maybe_unused]] const auto v16 = state.Magnitude(v15);
            [[maybe_unused]] const auto v17 = state.Gate(v15);
            const bool v18 = (input.m_schmitt == true);
            unsigned v29;
            if (v18)
            {
                const bool v19 = (v16 <= 0.0);
                unsigned v26;
                if (v19)
                {
                    v26 = 0u;
                }
                else
                {
                    const bool v20 = (v16 < v12);
                    unsigned v25;
                    if (v20)
                    {
                        v25 = 0u;
                    }
                    else
                    {
                        unsigned v24;
                        if (v17 == 0u)
                        {
                            const bool v21 = (v13 < v16);
                            unsigned v22;
                            if (v21)
                            {
                                v22 = 1u;
                            }
                            else
                            {
                                v22 = 0u;
                            }

                            v24 = v22;
                        }
                        else
                        {
                            unsigned v23;
                            if (v17 == 1u)
                            {
                                v23 = 2u;
                            }
                            else
                            {
                                v23 = 2u;
                            }

                            v24 = v23;
                        }

                        v25 = v24;
                    }

                    v26 = v25;
                }

                v29 = v26;
            }
            else
            {
                const bool v27 = (v16 < v12);
                unsigned v28;
                if (v27)
                {
                    v28 = 0u;
                }
                else
                {
                    v28 = 2u;
                }

                v29 = v28;
            }

            state.SetGate(v15, v29);
            if (v29 == 0u)
            {
                [[maybe_unused]] const auto v30 = state.NextPhase();
                state.Put(v15, {v15, v30, true});
            }
            else
            {
                if (v29 == 1u)
                {
                    [[maybe_unused]] const auto v31 = state.PreviousMagnitude(v15);
                    state.Push({v15, true, v31});
                }
                else
                {
                    [[maybe_unused]] const auto v32 = state.PreviousMagnitude(v15);
                    state.Push({v15, true, v32});
                }

            }

        }

        struct loop_v33_Type
        {
        };

        [[maybe_unused]] loop_v33_Type loop_v33;
        for (std::size_t v34 = 0; v34 < 4094; ++v34)
        {
            [[maybe_unused]] const auto v35 = state.Pop();
            bool v73;
            if (v35.m_present)
            {
                const bool v36 = (v35.m_value.m_previous == true);
                if (v36)
                {
                    [[maybe_unused]] const auto v37 = state.Computed(v35.m_value.m_bin);
                    const bool v38 = (v37 == true);
                    if (v38)
                    {
                    }
                    else
                    {
                        [[maybe_unused]] const auto v39 = state.Omega(v35.m_value.m_bin);
                        const double v40 = (v39 * input.m_hop);
                        state.Put(v35.m_value.m_bin, {v35.m_value.m_bin, v40, false});
                        [[maybe_unused]] const auto v41 = state.Magnitude(v35.m_value.m_bin);
                        state.Push({v35.m_value.m_bin, false, v41});
                    }

                }
                else
                {
                    const size_t v42 = CodegenInteger::Add<size_t>(v35.m_value.m_bin, 1);
                    const bool v43 = (v42 < 2047);
                    bool v45;
                    size_t v46;
                    if (v43)
                    {
                        const size_t v44 = CodegenInteger::Add<size_t>(v35.m_value.m_bin, 1);
                        v45 = true;
                        v46 = v44;
                    }
                    else
                    {
                        v45 = false;
                        v46 = size_t{};
                    }

                    if (v45)
                    {
                        [[maybe_unused]] const auto v47 = state.Computed(v46);
                        const bool v48 = (v47 == true);
                        if (v48)
                        {
                        }
                        else
                        {
                            [[maybe_unused]] const auto v49 = state.Link(v35.m_value.m_bin);
                            [[maybe_unused]] const auto v50 = state.Phase(v46);
                            [[maybe_unused]] const auto v51 = state.Phase(v35.m_value.m_bin);
                            const double v52 = (v50 - v51);
                            const bool v53 = (v49.m_root == v35.m_value.m_bin);
                            const bool v54 = (v53 == true);
                            double v56;
                            if (v54)
                            {
                                v56 = v52;
                            }
                            else
                            {
                                const double v55 = (v52 + v49.m_delta);
                                v56 = v55;
                            }

                            state.Put(v46, {v49.m_root, v56, false});
                            [[maybe_unused]] const auto v57 = state.Magnitude(v46);
                            state.Push({v46, false, v57});
                        }

                    }
                    else
                    {
                    }

                    const bool v58 = (0 < v35.m_value.m_bin);
                    bool v60;
                    size_t v61;
                    if (v58)
                    {
                        const size_t v59 = (v35.m_value.m_bin < 1 ? 0 : v35.m_value.m_bin - 1);
                        v60 = true;
                        v61 = v59;
                    }
                    else
                    {
                        v60 = false;
                        v61 = size_t{};
                    }

                    if (v60)
                    {
                        [[maybe_unused]] const auto v62 = state.Computed(v61);
                        const bool v63 = (v62 == true);
                        if (v63)
                        {
                        }
                        else
                        {
                            [[maybe_unused]] const auto v64 = state.Link(v35.m_value.m_bin);
                            [[maybe_unused]] const auto v65 = state.Phase(v61);
                            [[maybe_unused]] const auto v66 = state.Phase(v35.m_value.m_bin);
                            const double v67 = (v65 - v66);
                            const bool v68 = (v64.m_root == v35.m_value.m_bin);
                            const bool v69 = (v68 == true);
                            double v71;
                            if (v69)
                            {
                                v71 = v67;
                            }
                            else
                            {
                                const double v70 = (v67 + v64.m_delta);
                                v71 = v70;
                            }

                            state.Put(v61, {v64.m_root, v71, false});
                            [[maybe_unused]] const auto v72 = state.Magnitude(v61);
                            state.Push({v61, false, v72});
                        }

                    }
                    else
                    {
                    }

                }

                v73 = false;
            }
            else
            {
                v73 = true;
            }

            if (v73)
            {
                break;
            }
        }

    }

};
