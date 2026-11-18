#pragma once

// Generated from DSP.MorphingTPTSVF.stepFloat. Do not edit.
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
struct MorphingTPTSVF
{
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

    struct State
    {
        struct
        {
            double m_k;
            double m_a1;
            double m_a2;
            double m_a3;
            double m_ic1;
            double m_ic2;
            double m_lowPass;
            double m_bandPass;
            double m_highPass;
            double m_notch;
            double m_allPass;
        } m_svf;

        double m_cosine;
        double m_sine;
        double m_blend;
        double m_output;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_svf.m_ic2);
        const double v1 = (m_state.m_svf.m_a1 * m_state.m_svf.m_ic1);
        const double v2 = std::fma(m_state.m_svf.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_svf.m_a2, m_state.m_svf.m_ic1, m_state.m_svf.m_ic2);
        const double v4 = std::fma(m_state.m_svf.m_a3, v0, v3);
        const double v5 = (-m_state.m_svf.m_k);
        const double v6 = std::fma(v5, v2, input.m_x);
        const double v7 = (v6 - v4);
        const double v8 = (-m_state.m_svf.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        m_state.m_svf.m_ic1 = v9;
        const double v10 = (-m_state.m_svf.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        m_state.m_svf.m_ic2 = v11;
        const double v12 = (v4 + v7);
        m_state.m_svf.m_notch = v12;
        const double v13 = (-m_state.m_svf.m_k);
        const double v14 = (v13 * 0x10000000000000p-51);
        const double v15 = std::fma(v14, v2, input.m_x);
        m_state.m_svf.m_allPass = v15;
        const double v16 = (v4 * m_state.m_cosine);
        m_state.m_svf.m_lowPass = v4;
        const double v17 = std::fma(v2, m_state.m_sine, v16);
        const double v18 = (v2 * m_state.m_cosine);
        m_state.m_svf.m_bandPass = v2;
        const double v19 = std::fma(v7, m_state.m_sine, v18);
        m_state.m_svf.m_highPass = v7;
        const bool v20 = (0x10000000000000p-53 < m_state.m_blend);
        double v21;
        if (v20)
        {
            v21 = v19;
        }
        else
        {
            v21 = v17;
        }
        m_state.m_output = v21;

    }

    // Response generated from DSP.MorphingTPTSVF.Snapshot.magnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        struct
        {
            double m_g;
            double m_k;
        } m_svf;

        double m_low;
        double m_band;
        double m_high;
    };

    static double MagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_svf.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (snapshot.m_low * v1);
        const double v5 = (snapshot.m_high * v2);
        const double v6 = (v4 - v5);
        const double v7 = (snapshot.m_band * v3);
        const double v8 = (v1 - v2);
        const double v9 = (snapshot.m_svf.m_k * v3);
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

    // Coefficients generated from DSP.MorphingTPTSVF.prepareCoefficientsFloat.
    //

    Snapshot m_responseSnapshot{};

    Snapshot MakeSnapshot() const
    {
        return m_responseSnapshot;
    }

    struct LogicalSpec
    {
        struct
        {
            double m_cutoff;
            double m_q;
        } m_svf;

        double m_blend;
    };

    void ComputeCoefficients(const LogicalSpec& spec)
    {
        const double v0 = Math4096::TanPi(MultiChannelNumber<double, 1>::Splat(spec.m_svf.m_cutoff))[0];
        const double v1 = (1.0 / spec.m_svf.m_q);
        const double v2 = (v0 + v1);
        const double v3 = std::fma(v0, v2, 1.0);
        const double v4 = (1.0 / v3);
        const double v5 = (v0 * v4);
        const double v6 = (v0 * v5);
        const double v7 = (1.0 / 0x10000000000000p-51);
        const bool v8 = (v7 < spec.m_blend);
        double v12;
        if (v8)
        {
            const double v9 = (spec.m_blend - v7);
            const double v10 = (v9 / 0x10000000000000p-51);
            v12 = v10;
        }
        else
        {
            const double v11 = (spec.m_blend / 0x10000000000000p-51);
            v12 = v11;
        }

        const double v13 = Math4096::Cos2pi(MultiChannelNumber<double, 1>::Splat(v12))[0];
        const double v14 = Math4096::Sin2pi(MultiChannelNumber<double, 1>::Splat(v12))[0];
        const bool v15 = (v7 < spec.m_blend);
        double v16;
        if (v15)
        {
            v16 = 0.0;
        }
        else
        {
            v16 = v13;
        }

        const bool v17 = (v7 < spec.m_blend);
        double v18;
        if (v17)
        {
            v18 = v13;
        }
        else
        {
            v18 = v14;
        }

        const bool v19 = (v7 < spec.m_blend);
        double v20;
        if (v19)
        {
            v20 = v14;
        }
        else
        {
            v20 = 0.0;
        }

        const double v21 = spec.m_blend;
        m_state.m_svf.m_k = v1;
        m_state.m_svf.m_a1 = v4;
        m_state.m_svf.m_a2 = v5;
        m_state.m_svf.m_a3 = v6;
        m_state.m_cosine = v13;
        m_state.m_sine = v14;
        m_state.m_blend = v21;
        m_responseSnapshot.m_svf.m_g = v0;
        m_responseSnapshot.m_svf.m_k = v1;
        m_responseSnapshot.m_low = v16;
        m_responseSnapshot.m_band = v18;
        m_responseSnapshot.m_high = v20;
    }

    using NumberType = decltype(State{}.m_svf.m_k);
    static constexpr size_t x_numChannels = 1;

    using UIState = TransferFunctionGeneric<GeneratedResponseSnapshot<MorphingTPTSVF, &MorphingTPTSVF::MagnitudeSquared>>;

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

};
