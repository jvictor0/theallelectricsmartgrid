#pragma once

// Generated from DSP.ConservativeReuppingLadder.Runtime.process. Do not edit.
// This backend is validated by tests; it is not a verified compiler.
//
// Compile with strict floating-point arithmetic and implicit contraction disabled.
//

#include <limits>
#include <array>
#include <cmath>

#include "Math.hpp"

#include "ResponseFrequency.hpp"
#include "GeneratedFilterUI.hpp"
#include "MultiChannelNumber.hpp"
#include <type_traits>

template<typename Number, size_t Size>
struct ConservativeReuppingLadderCoreGeneric
{
    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);
    using Vector = MultiChannelNumber<Number, Size>;

    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        Vector m_alpha;
        struct
        {
            Vector m_gain;
            Vector m_tanhGain;
        } m_saturation;

        Vector m_k;
        Vector m_stage1;
        Vector m_stage2;
        Vector m_stage3;
        Vector m_stage4;
    };

    struct Input
    {
        Vector m_x;
    };

    struct Output
    {
        Vector m_x;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const Vector v0 = (m_state.m_k * m_state.m_stage4);
        const Vector v1 = (input.m_x - v0);
        const Vector v2 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v3 = (v2 * m_state.m_stage1);
        const Vector v4 = Vector::FusedMultiplyAdd(m_state.m_alpha, v1, v3);
        const Vector v5 = (v4 * m_state.m_saturation.m_gain);
        m_state.m_stage1 = v4;
        const Vector v6 = (v5 * v5);
        const Vector v7 = (v5 * (v6 + Vector::Splat(27)));
        const Vector v8 = Vector::FusedMultiplyAdd(v5 * Vector::Splat(9), v5, Vector::Splat(27));
        const Vector v9 = (v7 / v8);
        const Vector v10 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v9));
        const Vector v11 = (v10 / m_state.m_saturation.m_tanhGain);
        const Vector v12 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v13 = (v12 * m_state.m_stage2);
        const Vector v14 = Vector::FusedMultiplyAdd(m_state.m_alpha, v11, v13);
        const Vector v15 = (v14 * m_state.m_saturation.m_gain);
        m_state.m_stage2 = v14;
        const Vector v16 = (v15 * v15);
        const Vector v17 = (v15 * (v16 + Vector::Splat(27)));
        const Vector v18 = Vector::FusedMultiplyAdd(v15 * Vector::Splat(9), v15, Vector::Splat(27));
        const Vector v19 = (v17 / v18);
        const Vector v20 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v19));
        const Vector v21 = (v20 / m_state.m_saturation.m_tanhGain);
        const Vector v22 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v23 = (v22 * m_state.m_stage3);
        const Vector v24 = Vector::FusedMultiplyAdd(m_state.m_alpha, v21, v23);
        const Vector v25 = (v24 * m_state.m_saturation.m_gain);
        m_state.m_stage3 = v24;
        const Vector v26 = (v25 * v25);
        const Vector v27 = (v25 * (v26 + Vector::Splat(27)));
        const Vector v28 = Vector::FusedMultiplyAdd(v25 * Vector::Splat(9), v25, Vector::Splat(27));
        const Vector v29 = (v27 / v28);
        const Vector v30 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v29));
        const Vector v31 = (v30 / m_state.m_saturation.m_tanhGain);
        const Vector v32 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - m_state.m_alpha);
        const Vector v33 = (v32 * m_state.m_stage4);
        const Vector v34 = Vector::FusedMultiplyAdd(m_state.m_alpha, v31, v33);
        const Vector v35 = (v34 * m_state.m_saturation.m_gain);
        m_state.m_stage4 = v34;
        const Vector v36 = (v35 * v35);
        const Vector v37 = (v35 * (v36 + Vector::Splat(27)));
        const Vector v38 = Vector::FusedMultiplyAdd(v35 * Vector::Splat(9), v35, Vector::Splat(27));
        const Vector v39 = (v37 / v38);
        const Vector v40 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v39));
        const Vector v41 = (v40 / m_state.m_saturation.m_tanhGain);
        const Vector v42 = (m_state.m_k + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v43 = (v41 * v42);
        m_output.m_x = v43;
    }

    // Response generated from DSP.ConservativeReuppingLadder.Snapshot.magnitudeSquared.
    //

    using Frequency = ::ResponseFrequencyGeneric<Vector>;

    struct Snapshot
    {
        Vector m_memoryRatio;
        Vector m_slope;
        Vector m_k;
    };

    static Vector MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const Vector v0 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * snapshot.m_memoryRatio);
        const Vector v1 = (input.m_sinHalf * input.m_sinHalf);
        const Vector v2 = Vector::FusedMultiplyAdd(v0, v1, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v3 = (snapshot.m_memoryRatio * input.m_zInverse.m_imag);
        const Vector v4 = (-v3);
        const Vector v5 = (v4 * v4);
        const Vector v6 = (-v5);
        const Vector v7 = Vector::FusedMultiplyAdd(v2, v2, v6);
        const Vector v8 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v2);
        const Vector v9 = (v8 * v4);
        const Vector v10 = (snapshot.m_slope * snapshot.m_slope);
        const Vector v11 = (snapshot.m_k * v10);
        const Vector v12 = (v11 * snapshot.m_slope);
        const Vector v13 = (v9 * v9);
        const Vector v14 = (-v13);
        const Vector v15 = Vector::FusedMultiplyAdd(v7, v7, v14);
        const Vector v16 = Vector::FusedMultiplyAdd(v12, input.m_zInverse.m_real, v15);
        const Vector v17 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) * v7);
        const Vector v18 = (v17 * v9);
        const Vector v19 = Vector::FusedMultiplyAdd(v12, input.m_zInverse.m_imag, v18);
        const Vector v20 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + snapshot.m_k);
        const Vector v21 = (v20 * v10);
        const Vector v22 = (v21 * v10);
        const Vector v23 = (v22 * v22);
        const Vector v24 = (v22 * v22);
        const Vector v25 = (v19 * v19);
        const Vector v26 = Vector::FusedMultiplyAdd(v16, v16, v25);
        const Vector v27 = Vector::IfLess(Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0)), v24, v26, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)));
        const Vector v28 = (v23 / v27);
        return v28;
    }

    // Coefficients generated from DSP.ConservativeReuppingLadder.Runtime.prepareCoefficients.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        Vector m_cutoff;
        Vector m_resonance;
        Vector m_gain;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const Vector v0 = Vector::IfLess(spec.m_cutoff, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)), spec.m_cutoff, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v1 = (v0 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-24f : 0x10000000000000p-53)));
        const Vector v2 = Math4096::Sin2pi(v1);
        const Vector v3 = (v2 * v2);
        const Vector v4 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v3);
        const Vector v5 = v4.Sqrt();
        const Vector v6 = (v2 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v7 = (v5 - v2);
        const Vector v8 = (v6 * v7);
        const Vector v9 = (v2 * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v10 = (v5 + v2);
        const Vector v11 = (v9 * v10);
        const Vector v12 = (v11 + Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)));
        const Vector v13 = Vector::IfLess(v11, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v11);
        const Vector v14 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)) / v13);
        const Vector v15 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) + v14);
        const Vector v16 = (v15 * v15);
        const Vector v17 = (v12 * v12);
        const Vector v18 = (v16 * v16);
        const Vector v19 = Vector::IfLess(v11, Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0x800000p-22f : 0x10000000000000p-51)), v17, v18);
        const Vector v20 = (spec.m_gain * spec.m_gain);
        const Vector v21 = (spec.m_gain * (v20 + Vector::Splat(27)));
        const Vector v22 = Vector::FusedMultiplyAdd(spec.m_gain * Vector::Splat(9), spec.m_gain, Vector::Splat(27));
        const Vector v23 = (v21 / v22);
        const Vector v24 = Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), v23));
        const Vector v25 = (spec.m_gain / v24);
        const Vector v26 = (spec.m_resonance * Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xf9999ap-24f : 0x1f333333333333p-53)));
        const Vector v27 = (v26 * v19);
        const Vector v28 = (v25 * v25);
        const Vector v29 = (v28 * v25);
        const Vector v30 = (v27 / v29);
        const Vector v31 = (Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 1.0f : 1.0)) - v8);
        const Vector v32 = (v31 / v8);
        const Vector v33 = (spec.m_gain / v24);
        const Vector v34 = spec.m_gain;
        m_state.m_alpha = v8;
        m_state.m_saturation.m_gain = v34;
        m_state.m_saturation.m_tanhGain = v24;
        m_state.m_k = v30;
        m_responseSnapshot.m_memoryRatio = v32;
        m_responseSnapshot.m_slope = v33;
        m_responseSnapshot.m_k = v30;
    }

    // Reset generated from DSP.ConservativeReuppingLadder.Runtime.reset.
    //

    void Reset()
    {
        const Vector v0 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v1 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v2 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v3 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        const Vector v4 = Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0.0f : 0.0));
        m_state.m_stage1 = v1;
        m_state.m_stage2 = v2;
        m_state.m_stage3 = v3;
        m_state.m_stage4 = v4;
        m_output.m_x = v0;
    }

    using NumberType = Number;
    static constexpr size_t x_numChannels = Size;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<ConservativeReuppingLadderCoreGeneric<Number, Size>, &ConservativeReuppingLadderCoreGeneric<Number, Size>::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
