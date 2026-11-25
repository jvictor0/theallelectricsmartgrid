#pragma once

#include "BitCrush.hpp"
#include "GeneratedNaiveReuppingLadder.hpp"
#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SquiggleBoyShapeBase.hpp"
#include "TPTSVFFilter.hpp"

// Reupping ladder low-pass into the sample rate reducer, then a 4-pole SVF
// high-pass, so the high-pass also removes bass the reducer generates. One
// sample per call for every channel.
//
template <size_t Size>
struct NaiveReuppingLadderShapeGeneric : SquiggleBoyShapeBase
{
    using Vector = MultiChannelNumber<float, Size>;
    using LowPass = GeneratedNaiveReuppingLadderGeneric<float, Size>;

    // Preserve the old half-power knob tuning approximately at low frequencies
    // while exposing the traditional stage corner over the same 12-octave span.
    //
    static constexpr float x_lpCutoffScale = 1.0f / LowPass::x_halfPowerRatio;

    struct Snapshot : PowerResponseSnapshot<Size>
    {
        using Base = PowerResponseSnapshot<Size>;
        using typename Base::Frequency;
        using typename Base::ResponseVector;
        typename LowPass::Snapshot m_lp;
        typename TPTSVF4PoleHighPassFilterGeneric<float, Size>::Snapshot m_hp;

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            return m_lp.MagnitudeSquaredResponses(frequency) * m_hp.MagnitudeSquaredResponses(frequency);
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        snapshot.m_lp = m_lpLadder.MakeSnapshot();
        snapshot.m_hp = m_hp4Pole.MakeSnapshot();
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

    LowPass m_lpLadder;
    TPTSVF4PoleHighPassFilterGeneric<float, Size> m_hp4Pole;
    SampleRateReducerGeneric<Size> m_sampleRateReducer;

    // The base frequency is in cycles per sample.
    //
    struct Input
    {
        Vector m_baseFreq;
        PhaseUtils::ExpParamGeneric<float, Size> m_lpCutoffFactor;
        PhaseUtils::ExpParamGeneric<float, Size> m_hpCutoffFactor;
        PhaseUtils::ZeroedExpParamGeneric<float, Size> m_lpResonance;
        PhaseUtils::ZeroedExpParamGeneric<float, Size> m_hpResonance;
        PhaseUtils::ExpParamGeneric<float, Size> m_saturationGain;
        PhaseUtils::ExpParamGeneric<float, Size> m_sampleRateReducerFreq;

        Input()
            : m_baseFreq(Vector::Splat(PhaseUtils::VOctToNatural(0.0, SampleTimer::x_samplePeriod)))
            , m_lpCutoffFactor(0.25f * x_lpCutoffScale, 1024.0f * x_lpCutoffScale)
            , m_hpCutoffFactor(0.25f, 256.0f)
            , m_saturationGain(0.25f, 5.0f)
            , m_sampleRateReducerFreq(1.0f, 2048.0f)
        {
            m_lpResonance.SetBaseByCenter(0.125);
            m_hpResonance.SetBaseByCenter(0.125);
        }
    };

    Vector Process(const Input& input, const Vector& x)
    {
        Vector hpCutoff = input.m_hpCutoffFactor.m_expParam;
        m_hp4Pole.SetCutoffAndResonance(HighPassFreq(input.m_baseFreq, hpCutoff), input.m_hpResonance.m_expParam);
        m_lpLadder.SetCutoffResonanceAndGain(
            LowPassFreq(input.m_baseFreq, hpCutoff, input.m_lpCutoffFactor.m_expParam,
                LowPass::x_maxCutoff),
            input.m_lpResonance.m_expParam, input.m_saturationGain.m_expParam);

        m_sampleRateReducer.SetFreq(SampleRateReducerFreq(input.m_baseFreq, input.m_sampleRateReducerFreq.m_expParam));
        m_hp4Pole.Process(m_sampleRateReducer.Process(m_lpLadder.Process(x)));
        return m_hp4Pole.GetOutput();
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& baseFreq)
    {
        using Param = SmartGridOneEncoders::Param;

        input.m_baseFreq = baseFreq;
        input.m_lpCutoffFactor.Update(encoders.GetValue<Size>(Param::LPCutoff, track));
        input.m_hpCutoffFactor.Update(encoders.GetValue<Size>(Param::HPCutoff, track));
        input.m_lpResonance.Update(encoders.GetValue<Size>(Param::LPResonance, track));
        input.m_hpResonance.Update(encoders.GetValue<Size>(Param::HPResonance, track));
        input.m_saturationGain.Update(encoders.GetValue<Size>(Param::FilterDrive, track));
        input.m_sampleRateReducerFreq.Update(encoders.GetValue<Size>(Param::SampleRateReduction, track));
    }
};

using TrioNaiveReuppingLadderShape = NaiveReuppingLadderShapeGeneric<3>;
