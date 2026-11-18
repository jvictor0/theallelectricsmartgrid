#pragma once

// Generated from DSP.ConservativeReuppingLadder.stepFloat. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"
#include "MultiChannelNumber.hpp"

#include "ResponseFrequency.hpp"
#include "GeneratedFilterUI.hpp"
struct ConservativeReuppingLadderCore
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        double m_alpha;
        struct
        {
            double m_gain;
            double m_tanhGain;
        } m_saturation;

        double m_k;
        double m_stage1;
        double m_stage2;
        double m_stage3;
        double m_stage4;
        double m_input;
        double m_output;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (m_state.m_k * m_state.m_stage4);
        const double v1 = (input.m_x - v0);
        const double v2 = (1.0 - m_state.m_alpha);
        const double v3 = (v2 * m_state.m_stage1);
        const double v4 = std::fma(m_state.m_alpha, v1, v3);
        m_state.m_input = v1;
        const double v5 = (v4 * m_state.m_saturation.m_gain);
        const double v6 = (v4 * m_state.m_saturation.m_gain);
        const double v7 = (v5 * v6);
        const double v8 = (v4 * m_state.m_saturation.m_gain);
        const double v9 = (v7 + 0x1b000000000000p-48);
        const double v10 = (v8 * v9);
        const double v11 = (v4 * m_state.m_saturation.m_gain);
        const double v12 = (v11 * 0x12000000000000p-49);
        const double v13 = (v4 * m_state.m_saturation.m_gain);
        m_state.m_stage1 = v4;
        const double v14 = std::fma(v12, v13, 0x1b000000000000p-48);
        const double v15 = (v10 / v14);
        const double v16 = (-1.0);
        const bool v17 = (v16 < v15);
        double v19;
        if (v17)
        {
            v19 = v15;
        }
        else
        {
            const double v18 = (-1.0);
            v19 = v18;
        }

        const bool v20 = (v19 < 1.0);
        double v25;
        if (v20)
        {
            const double v21 = (-1.0);
            const bool v22 = (v21 < v15);
            double v24;
            if (v22)
            {
                v24 = v15;
            }
            else
            {
                const double v23 = (-1.0);
                v24 = v23;
            }

            v25 = v24;
        }
        else
        {
            v25 = 1.0;
        }

        const double v26 = (v25 / m_state.m_saturation.m_tanhGain);
        const double v27 = (1.0 - m_state.m_alpha);
        const double v28 = (v27 * m_state.m_stage2);
        const double v29 = std::fma(m_state.m_alpha, v26, v28);
        const double v30 = (v29 * m_state.m_saturation.m_gain);
        const double v31 = (v29 * m_state.m_saturation.m_gain);
        const double v32 = (v30 * v31);
        const double v33 = (v29 * m_state.m_saturation.m_gain);
        const double v34 = (v32 + 0x1b000000000000p-48);
        const double v35 = (v33 * v34);
        const double v36 = (v29 * m_state.m_saturation.m_gain);
        const double v37 = (v36 * 0x12000000000000p-49);
        const double v38 = (v29 * m_state.m_saturation.m_gain);
        m_state.m_stage2 = v29;
        const double v39 = std::fma(v37, v38, 0x1b000000000000p-48);
        const double v40 = (v35 / v39);
        const double v41 = (-1.0);
        const bool v42 = (v41 < v40);
        double v44;
        if (v42)
        {
            v44 = v40;
        }
        else
        {
            const double v43 = (-1.0);
            v44 = v43;
        }

        const bool v45 = (v44 < 1.0);
        double v50;
        if (v45)
        {
            const double v46 = (-1.0);
            const bool v47 = (v46 < v40);
            double v49;
            if (v47)
            {
                v49 = v40;
            }
            else
            {
                const double v48 = (-1.0);
                v49 = v48;
            }

            v50 = v49;
        }
        else
        {
            v50 = 1.0;
        }

        const double v51 = (v50 / m_state.m_saturation.m_tanhGain);
        const double v52 = (1.0 - m_state.m_alpha);
        const double v53 = (v52 * m_state.m_stage3);
        const double v54 = std::fma(m_state.m_alpha, v51, v53);
        const double v55 = (v54 * m_state.m_saturation.m_gain);
        const double v56 = (v54 * m_state.m_saturation.m_gain);
        const double v57 = (v55 * v56);
        const double v58 = (v54 * m_state.m_saturation.m_gain);
        const double v59 = (v57 + 0x1b000000000000p-48);
        const double v60 = (v58 * v59);
        const double v61 = (v54 * m_state.m_saturation.m_gain);
        const double v62 = (v61 * 0x12000000000000p-49);
        const double v63 = (v54 * m_state.m_saturation.m_gain);
        m_state.m_stage3 = v54;
        const double v64 = std::fma(v62, v63, 0x1b000000000000p-48);
        const double v65 = (v60 / v64);
        const double v66 = (-1.0);
        const bool v67 = (v66 < v65);
        double v69;
        if (v67)
        {
            v69 = v65;
        }
        else
        {
            const double v68 = (-1.0);
            v69 = v68;
        }

        const bool v70 = (v69 < 1.0);
        double v75;
        if (v70)
        {
            const double v71 = (-1.0);
            const bool v72 = (v71 < v65);
            double v74;
            if (v72)
            {
                v74 = v65;
            }
            else
            {
                const double v73 = (-1.0);
                v74 = v73;
            }

            v75 = v74;
        }
        else
        {
            v75 = 1.0;
        }

        const double v76 = (v75 / m_state.m_saturation.m_tanhGain);
        const double v77 = (1.0 - m_state.m_alpha);
        const double v78 = (v77 * m_state.m_stage4);
        const double v79 = std::fma(m_state.m_alpha, v76, v78);
        const double v80 = (v79 * m_state.m_saturation.m_gain);
        const double v81 = (v79 * m_state.m_saturation.m_gain);
        const double v82 = (v80 * v81);
        const double v83 = (v79 * m_state.m_saturation.m_gain);
        const double v84 = (v82 + 0x1b000000000000p-48);
        const double v85 = (v83 * v84);
        const double v86 = (v79 * m_state.m_saturation.m_gain);
        const double v87 = (v86 * 0x12000000000000p-49);
        const double v88 = (v79 * m_state.m_saturation.m_gain);
        m_state.m_stage4 = v79;
        const double v89 = std::fma(v87, v88, 0x1b000000000000p-48);
        const double v90 = (v85 / v89);
        const double v91 = (-1.0);
        const bool v92 = (v91 < v90);
        double v94;
        if (v92)
        {
            v94 = v90;
        }
        else
        {
            const double v93 = (-1.0);
            v94 = v93;
        }

        const bool v95 = (v94 < 1.0);
        double v100;
        if (v95)
        {
            const double v96 = (-1.0);
            const bool v97 = (v96 < v90);
            double v99;
            if (v97)
            {
                v99 = v90;
            }
            else
            {
                const double v98 = (-1.0);
                v99 = v98;
            }

            v100 = v99;
        }
        else
        {
            v100 = 1.0;
        }

        const double v101 = (v100 / m_state.m_saturation.m_tanhGain);
        const double v102 = (m_state.m_k + 1.0);
        const double v103 = (v101 * v102);
        m_state.m_output = v103;
    }

    // Response generated from DSP.ConservativeReuppingLadder.Snapshot.magnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        double m_memoryRatio;
        double m_slope;
        double m_k;
    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (0x10000000000000p-51 * snapshot.m_memoryRatio);
        const double v1 = (input.m_sinHalf * input.m_sinHalf);
        const double v2 = std::fma(v0, v1, 1.0);
        const double v3 = (snapshot.m_memoryRatio * input.m_zInverse.m_imag);
        const double v4 = (-v3);
        const double v5 = (v4 * v4);
        const double v6 = (-v5);
        const double v7 = std::fma(v2, v2, v6);
        const double v8 = (0x10000000000000p-51 * v2);
        const double v9 = (v8 * v4);
        const double v10 = (snapshot.m_slope * snapshot.m_slope);
        const double v11 = (snapshot.m_k * v10);
        const double v12 = (v11 * snapshot.m_slope);
        const double v13 = (v9 * v9);
        const double v14 = (-v13);
        const double v15 = std::fma(v7, v7, v14);
        const double v16 = std::fma(v12, input.m_zInverse.m_real, v15);
        const double v17 = (0x10000000000000p-51 * v7);
        const double v18 = (v17 * v9);
        const double v19 = std::fma(v12, input.m_zInverse.m_imag, v18);
        const double v20 = (1.0 + snapshot.m_k);
        const double v21 = (v20 * v10);
        const double v22 = (v21 * v10);
        const double v23 = (v22 * v22);
        const double v24 = (v22 * v22);
        const bool v25 = (0.0 < v24);
        double v28;
        if (v25)
        {
            const double v26 = (v19 * v19);
            const double v27 = std::fma(v16, v16, v26);
            v28 = v27;
        }
        else
        {
            v28 = 1.0;
        }

        const double v29 = (v23 / v28);
        return v29;
    }

    // Coefficients generated from DSP.ConservativeReuppingLadder.prepareCoefficientsFloat.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        double m_cutoff;
        double m_resonance;
        double m_gain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const bool v0 = (spec.m_cutoff < 0x10000000000000p-53);
        double v1;
        if (v0)
        {
            v1 = spec.m_cutoff;
        }
        else
        {
            v1 = 0x10000000000000p-53;
        }

        const double v2 = (v1 * 0x10000000000000p-53);
        const double v3 = Math4096::Sin2pi(MultiChannelNumber<double, 1>::Splat(v2))[0];
        const double v4 = (v3 * v3);
        const double v5 = (1.0 + v4);
        const double v6 = std::sqrt(v5);
        const double v7 = (v3 * 0x10000000000000p-51);
        const double v8 = (v6 - v3);
        const double v9 = (v7 * v8);
        const double v10 = (v3 * 0x10000000000000p-51);
        const double v11 = (v6 + v3);
        const double v12 = (v10 * v11);
        const double v13 = (v12 + 0x10000000000000p-51);
        const bool v14 = (v12 < 0x10000000000000p-51);
        double v15;
        if (v14)
        {
            v15 = 0x10000000000000p-51;
        }
        else
        {
            v15 = v12;
        }

        const double v16 = (0x10000000000000p-51 / v15);
        const double v17 = (1.0 + v16);
        const double v18 = (v17 * v17);
        const bool v19 = (v12 < 0x10000000000000p-51);
        double v22;
        if (v19)
        {
            const double v20 = (v13 * v13);
            v22 = v20;
        }
        else
        {
            const double v21 = (v18 * v18);
            v22 = v21;
        }

        const double v23 = (spec.m_gain * spec.m_gain);
        const double v24 = (v23 + 0x1b000000000000p-48);
        const double v25 = (spec.m_gain * v24);
        const double v26 = (spec.m_gain * 0x12000000000000p-49);
        const double v27 = std::fma(v26, spec.m_gain, 0x1b000000000000p-48);
        const double v28 = (v25 / v27);
        const double v29 = (-1.0);
        const bool v30 = (v29 < v28);
        double v32;
        if (v30)
        {
            v32 = v28;
        }
        else
        {
            const double v31 = (-1.0);
            v32 = v31;
        }

        const bool v33 = (v32 < 1.0);
        double v38;
        if (v33)
        {
            const double v34 = (-1.0);
            const bool v35 = (v34 < v28);
            double v37;
            if (v35)
            {
                v37 = v28;
            }
            else
            {
                const double v36 = (-1.0);
                v37 = v36;
            }

            v38 = v37;
        }
        else
        {
            v38 = 1.0;
        }

        const double v39 = (spec.m_gain / v38);
        const double v40 = (spec.m_resonance * 0x1f333333333333p-53);
        const double v41 = (v40 * v22);
        const double v42 = (v39 * v39);
        const double v43 = (v42 * v39);
        const double v44 = (v41 / v43);
        const double v45 = (1.0 - v9);
        const double v46 = (v45 / v9);
        const double v47 = (spec.m_gain / v38);
        const double v48 = spec.m_gain;
        m_state.m_alpha = v9;
        m_state.m_saturation.m_gain = v48;
        m_state.m_saturation.m_tanhGain = v38;
        m_state.m_k = v44;
        m_responseSnapshot.m_memoryRatio = v46;
        m_responseSnapshot.m_slope = v47;
        m_responseSnapshot.m_k = v44;
    }

    // Reset generated from DSP.ConservativeReuppingLadder.resetFloat.
    //

    void Reset()
    {
        const double v0 = 0.0;
        const double v1 = 0.0;
        const double v2 = 0.0;
        const double v3 = 0.0;
        const double v4 = 0.0;
        const double v5 = 0.0;
        m_state.m_stage1 = v0;
        m_state.m_stage2 = v1;
        m_state.m_stage3 = v2;
        m_state.m_stage4 = v3;
        m_state.m_input = v4;
        m_state.m_output = v5;
    }

    using NumberType = decltype(State{}.m_alpha);
    static constexpr size_t x_numChannels = 1;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<ConservativeReuppingLadderCore, &ConservativeReuppingLadderCore::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
