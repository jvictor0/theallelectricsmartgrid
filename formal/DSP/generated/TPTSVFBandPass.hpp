#pragma once

// Generated from DSP.TPTSVF.Runtime.processBandPassFloat. Do not edit.
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
struct TPTSVFBandPass
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        double m_k;
        double m_a1;
        double m_a2;
        double m_a3;
        double m_ic1;
        double m_ic2;
    };

    struct Input
    {
        double m_x;
    };

    struct Output
    {
        double m_bandPass;
    };

    State m_state;
    Output m_output;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_ic2);
        const double v1 = (m_state.m_a1 * m_state.m_ic1);
        const double v2 = std::fma(m_state.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_a2, m_state.m_ic1, m_state.m_ic2);
        const double v4 = std::fma(m_state.m_a3, v0, v3);
        const double v8 = (-m_state.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        m_state.m_ic1 = v9;
        m_output.m_bandPass = v2;
        const double v10 = (-m_state.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        m_state.m_ic2 = v11;
    }

    // Response generated from DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        double m_g;
        double m_k;
    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (0.0 * v1);
        const double v5 = (0.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (1.0 * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_k * v3);
        const double v10 = (v7 * v7);
        const double v11 = std::fma(v6, v6, v10);
        const double v12 = (v7 * v7);
        const double v13 = std::fma(v6, v6, v12);
        const bool v14 = (0.0 < v13);
        double v17;
        if (v14)
        {
            const double v15 = (v9 * v9);
            const double v16 = std::fma(v8, v8, v15);
            v17 = v16;
        }
        else
        {
            v17 = 1.0;
        }

        const double v18 = (v11 / v17);
        return v18;
    }

    // Coefficients generated from DSP.TPTSVF.Runtime.prepareCoefficientsFloat.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        double m_cutoff;
        double m_q;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_cutoff))[0];
        const double v1 = (1.0 / spec.m_q);
        const double v2 = (v0 + v1);
        const double v3 = std::fma(v0, v2, 1.0);
        const double v4 = (1.0 / v3);
        const double v5 = (v0 * v4);
        const double v6 = (v0 * v5);
        m_state.m_k = v1;
        m_state.m_a1 = v4;
        m_state.m_a2 = v5;
        m_state.m_a3 = v6;
        m_responseSnapshot.m_g = v0;
        m_responseSnapshot.m_k = v1;
    }

    // Prepared coefficients generated from DSP.TPTSVF.Runtime.prepareFromSnapshotFloat.
    //

    void ComputeCoefficientsFromSnapshot(const Snapshot& spec)
    {
        const double v0 = (spec.m_g + spec.m_k);
        const double v1 = std::fma(spec.m_g, v0, 1.0);
        const double v2 = (1.0 / v1);
        const double v3 = (spec.m_g * v2);
        const double v4 = (spec.m_g * v3);
        const double v5 = spec.m_g;
        const double v6 = spec.m_k;
        const double v7 = spec.m_k;
        m_state.m_k = v7;
        m_state.m_a1 = v2;
        m_state.m_a2 = v3;
        m_state.m_a3 = v4;
        m_responseSnapshot.m_g = v5;
        m_responseSnapshot.m_k = v6;
    }

    using NumberType = decltype(State{}.m_k);
    static constexpr size_t x_numChannels = 1;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVFBandPass, &TPTSVFBandPass::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
