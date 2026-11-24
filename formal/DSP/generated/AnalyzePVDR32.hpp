#pragma once

// Generated from DSP.Resynthesis.PVDR.Storage.analyze32. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "CodegenInteger.hpp"
#include <tuple>
struct AnalyzePVDR32
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    template<typename State, typename Input>
    static void Process(State& state, [[maybe_unused]] const Input& input)
    {
        [[maybe_unused]] float loop_v0;
        loop_v0 = 0.0f;
        for (std::size_t v1 = 0; v1 < 2047; ++v1)
        {
            [[maybe_unused]] const auto v2 = state.Magnitude(v1);
            [[maybe_unused]] const auto v3 = state.PreviousMagnitude(v1);
            const bool v4 = (loop_v0 <= v2);
            float v5;
            if (v4)
            {
                v5 = v2;
            }
            else
            {
                v5 = loop_v0;
            }

            const bool v6 = (v5 <= v3);
            float v9;
            if (v6)
            {
                v9 = v3;
            }
            else
            {
                const bool v7 = (loop_v0 <= v2);
                float v8;
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

        const double v12 = static_cast<double>(loop_v0);
        const double v13 = (v12 * v11);
        const float v14 = static_cast<float>(v13);
        const double v15 = static_cast<double>(loop_v0);
        const double v16 = (v15 * 0x10624dd2f1a9fcp-62);
        const float v17 = static_cast<float>(v16);
        struct loop_v18_Type
        {
        };

        [[maybe_unused]] loop_v18_Type loop_v18;
        for (std::size_t v19 = 0; v19 < 2047; ++v19)
        {
            [[maybe_unused]] const auto v20 = state.Magnitude(v19);
            [[maybe_unused]] const auto v21 = state.Gate(v19);
            const bool v22 = (input.m_schmitt == true);
            unsigned v33;
            if (v22)
            {
                const bool v23 = (v20 <= 0.0f);
                unsigned v30;
                if (v23)
                {
                    v30 = 0u;
                }
                else
                {
                    const bool v24 = (v20 < v14);
                    unsigned v29;
                    if (v24)
                    {
                        v29 = 0u;
                    }
                    else
                    {
                        unsigned v28;
                        if (v21 == 0u)
                        {
                            const bool v25 = (v17 < v20);
                            unsigned v26;
                            if (v25)
                            {
                                v26 = 1u;
                            }
                            else
                            {
                                v26 = 0u;
                            }

                            v28 = v26;
                        }
                        else
                        {
                            unsigned v27;
                            if (v21 == 1u)
                            {
                                v27 = 2u;
                            }
                            else
                            {
                                v27 = 2u;
                            }

                            v28 = v27;
                        }

                        v29 = v28;
                    }

                    v30 = v29;
                }

                v33 = v30;
            }
            else
            {
                const bool v31 = (v20 < v14);
                unsigned v32;
                if (v31)
                {
                    v32 = 0u;
                }
                else
                {
                    v32 = 2u;
                }

                v33 = v32;
            }

            state.SetGate(v19, v33);
            if (v33 == 0u)
            {
                [[maybe_unused]] const auto v34 = state.NextPhase();
                const double v35 = static_cast<double>(v34);
                state.Put(v19, {v19, v35, true});
            }
            else
            {
                if (v33 == 1u)
                {
                    [[maybe_unused]] const auto v36 = state.PreviousMagnitude(v19);
                    state.Push({v19, true, v36});
                }
                else
                {
                    [[maybe_unused]] const auto v37 = state.PreviousMagnitude(v19);
                    state.Push({v19, true, v37});
                }

            }

        }

        struct loop_v38_Type
        {
        };

        [[maybe_unused]] loop_v38_Type loop_v38;
        for (std::size_t v39 = 0; v39 < 4094; ++v39)
        {
            [[maybe_unused]] const auto v40 = state.Pop();
            bool v80;
            if (v40.m_present)
            {
                const bool v41 = (v40.m_value.m_previous == true);
                if (v41)
                {
                    [[maybe_unused]] const auto v42 = state.Computed(v40.m_value.m_bin);
                    const bool v43 = (v42 == true);
                    if (v43)
                    {
                    }
                    else
                    {
                        [[maybe_unused]] const auto v44 = state.Omega(v40.m_value.m_bin);
                        const double v45 = (v44 * input.m_hop);
                        state.Put(v40.m_value.m_bin, {v40.m_value.m_bin, v45, false});
                        [[maybe_unused]] const auto v46 = state.Magnitude(v40.m_value.m_bin);
                        state.Push({v40.m_value.m_bin, false, v46});
                    }

                }
                else
                {
                    const size_t v47 = CodegenInteger::Add<size_t>(v40.m_value.m_bin, 1);
                    const bool v48 = (v47 < 2047);
                    bool v50;
                    size_t v51;
                    if (v48)
                    {
                        const size_t v49 = CodegenInteger::Add<size_t>(v40.m_value.m_bin, 1);
                        v50 = true;
                        v51 = v49;
                    }
                    else
                    {
                        v50 = false;
                        v51 = size_t{};
                    }

                    if (v50)
                    {
                        [[maybe_unused]] const auto v52 = state.Computed(v51);
                        const bool v53 = (v52 == true);
                        if (v53)
                        {
                        }
                        else
                        {
                            [[maybe_unused]] const auto v54 = state.Link(v40.m_value.m_bin);
                            [[maybe_unused]] const auto v55 = state.Phase(v51);
                            [[maybe_unused]] const auto v56 = state.Phase(v40.m_value.m_bin);
                            const float v57 = (v55 - v56);
                            const double v58 = static_cast<double>(v57);
                            const bool v59 = (v54.m_root == v40.m_value.m_bin);
                            const bool v60 = (v59 == true);
                            double v62;
                            if (v60)
                            {
                                v62 = v58;
                            }
                            else
                            {
                                const double v61 = (v58 + v54.m_delta);
                                v62 = v61;
                            }

                            state.Put(v51, {v54.m_root, v62, false});
                            [[maybe_unused]] const auto v63 = state.Magnitude(v51);
                            state.Push({v51, false, v63});
                        }

                    }
                    else
                    {
                    }

                    const bool v64 = (0 < v40.m_value.m_bin);
                    bool v66;
                    size_t v67;
                    if (v64)
                    {
                        const size_t v65 = (v40.m_value.m_bin < 1 ? 0 : v40.m_value.m_bin - 1);
                        v66 = true;
                        v67 = v65;
                    }
                    else
                    {
                        v66 = false;
                        v67 = size_t{};
                    }

                    if (v66)
                    {
                        [[maybe_unused]] const auto v68 = state.Computed(v67);
                        const bool v69 = (v68 == true);
                        if (v69)
                        {
                        }
                        else
                        {
                            [[maybe_unused]] const auto v70 = state.Link(v40.m_value.m_bin);
                            [[maybe_unused]] const auto v71 = state.Phase(v67);
                            [[maybe_unused]] const auto v72 = state.Phase(v40.m_value.m_bin);
                            const float v73 = (v71 - v72);
                            const double v74 = static_cast<double>(v73);
                            const bool v75 = (v70.m_root == v40.m_value.m_bin);
                            const bool v76 = (v75 == true);
                            double v78;
                            if (v76)
                            {
                                v78 = v74;
                            }
                            else
                            {
                                const double v77 = (v74 + v70.m_delta);
                                v78 = v77;
                            }

                            state.Put(v67, {v70.m_root, v78, false});
                            [[maybe_unused]] const auto v79 = state.Magnitude(v67);
                            state.Push({v67, false, v79});
                        }

                    }
                    else
                    {
                    }

                }

                v80 = false;
            }
            else
            {
                v80 = true;
            }

            if (v80)
            {
                break;
            }
        }

    }

};
