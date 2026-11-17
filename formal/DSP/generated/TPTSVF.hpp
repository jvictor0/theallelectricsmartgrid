#pragma once

// Generated from DSP.TPTSVF.stepFloat. Do not edit.
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
struct TPTSVF
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
        double m_lowPass;
        double m_bandPass;
        double m_highPass;
        double m_notch;
        double m_allPass;
    };

    struct Input
    {
        double m_x;
    };

    State m_state;

    void Process(Input& input)
    {
        const double v0 = (input.m_x - m_state.m_ic2);
        const double v1 = (m_state.m_a1 * m_state.m_ic1);
        const double v2 = std::fma(m_state.m_a2, v0, v1);
        const double v3 = std::fma(m_state.m_a2, m_state.m_ic1, m_state.m_ic2);
        const double v4 = std::fma(m_state.m_a3, v0, v3);
        const double v5 = (-m_state.m_k);
        const double v6 = std::fma(v5, v2, input.m_x);
        const double v7 = (v6 - v4);
        const double v8 = (-m_state.m_ic1);
        const double v9 = std::fma(v2, 0x10000000000000p-51, v8);
        const double v10 = (-m_state.m_ic2);
        const double v11 = std::fma(v4, 0x10000000000000p-51, v10);
        const double v12 = (v4 + v7);
        const double v13 = (-m_state.m_k);
        const double v14 = (v13 * 0x10000000000000p-51);
        const double v15 = std::fma(v14, v2, input.m_x);
        const double v16 = m_state.m_k;
        const double v17 = m_state.m_a1;
        const double v18 = m_state.m_a2;
        const double v19 = m_state.m_a3;
        const double v20 = v9;
        const double v21 = v11;
        const double v22 = v4;
        const double v23 = v2;
        const double v24 = v7;
        const double v25 = v12;
        const double v26 = v15;
        m_state.m_k = v16;
        m_state.m_a1 = v17;
        m_state.m_a2 = v18;
        m_state.m_a3 = v19;
        m_state.m_ic1 = v20;
        m_state.m_ic2 = v21;
        m_state.m_lowPass = v22;
        m_state.m_bandPass = v23;
        m_state.m_highPass = v24;
        m_state.m_notch = v25;
        m_state.m_allPass = v26;
    }

    // Response generated from DSP.TPTSVF.Snapshot.lowPassMagnitudeSquaredFloat.
    //

    using Frequency = ::ResponseFrequencyGeneric<double>;

    struct Snapshot
    {
        double m_g;
        double m_k;
    };

    static double LowPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (1.0 * v1);
        const double v5 = (0.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.bandPassMagnitudeSquaredFloat.
    //

    static double BandPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
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

    // Response generated from DSP.TPTSVF.Snapshot.highPassMagnitudeSquaredFloat.
    //

    static double HighPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (0.0 * v1);
        const double v5 = (1.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.notchMagnitudeSquaredFloat.
    //

    static double NotchMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (1.0 * v1);
        const double v5 = (1.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 * v3);
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

    // Response generated from DSP.TPTSVF.Snapshot.allPassMagnitudeSquaredFloat.
    //

    static double AllPassMagnitudeSquared(const Snapshot& snapshot, const Frequency& input)
    {
        const double v0 = (snapshot.m_g * input.m_cosHalf);
        const double v1 = (v0 * v0);
        const double v2 = (input.m_sinHalf * input.m_sinHalf);
        const double v3 = (v0 * input.m_sinHalf);
        const double v4 = (1.0 * v1);
        const double v5 = (1.0 * v2);
        const double v6 = (v4 - v5);
        const double v7 = (0.0 - snapshot.m_k);
        const double v8 = (v7 * v3);
        const double v9 = (v1 - v2);
        const double v10 = (snapshot.m_k * v3);
        const double v11 = (v8 * v8);
        const double v12 = std::fma(v6, v6, v11);
        const double v13 = (v8 * v8);
        const double v14 = std::fma(v6, v6, v13);
        const bool v15 = (0.0 < v14);
        double v18;
        if (v15)
        {
            const double v16 = (v10 * v10);
            const double v17 = std::fma(v9, v9, v16);
            v18 = v17;
        }
        else
        {
            v18 = 1.0;
        }

        const double v19 = (v12 / v18);
        return v19;
    }

    // Coefficients generated from DSP.TPTSVF.prepareCoefficientsFloat.
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
        const double v7 = v0;
        const double v8 = v1;
        const double v9 = v1;
        const double v10 = v4;
        const double v11 = v5;
        const double v12 = v6;
        m_state.m_k = v9;
        m_state.m_a1 = v10;
        m_state.m_a2 = v11;
        m_state.m_a3 = v12;
        m_responseSnapshot.m_g = v7;
        m_responseSnapshot.m_k = v8;
    }

    using NumberType = decltype(State{}.m_k);
    static constexpr size_t x_numChannels = 1;

    using LowPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVF, &TPTSVF::LowPassMagnitudeSquared>>;

    using BandPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVF, &TPTSVF::BandPassMagnitudeSquared>>;

    using HighPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVF, &TPTSVF::HighPassMagnitudeSquared>>;

    using NotchUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVF, &TPTSVF::NotchMagnitudeSquared>>;

    using AllPassUIState = TransferFunctionGeneric<GeneratedResponseSnapshot<TPTSVF, &TPTSVF::AllPassMagnitudeSquared>>;

};

struct TPTSVFLowPass : GeneratedFilterView<TPTSVF, &TPTSVF::LowPassMagnitudeSquared>
{
};

struct TPTSVFBandPass : GeneratedFilterView<TPTSVF, &TPTSVF::BandPassMagnitudeSquared>
{
};

struct TPTSVFHighPass : GeneratedFilterView<TPTSVF, &TPTSVF::HighPassMagnitudeSquared>
{
};

struct TPTSVFNotch : GeneratedFilterView<TPTSVF, &TPTSVF::NotchMagnitudeSquared>
{
};

struct TPTSVFAllPass : GeneratedFilterView<TPTSVF, &TPTSVF::AllPassMagnitudeSquared>
{
};
