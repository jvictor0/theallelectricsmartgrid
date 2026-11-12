#pragma once

#include "BitCrush.hpp"
#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "Saturator.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SquiggleBoyShapeBase.hpp"
#include "StateVariableFilter.hpp"

// Saturated 2-pole SVF low-pass into the sample rate reducer, then a 2-pole
// SVF high-pass, so the high-pass also removes bass the reducer generates. One
// sample per call for every channel.
//
template <size_t Size>
struct SVF2PoleShapeGeneric : SquiggleBoyShapeBase
{
    using Vector = MultiChannelNumber<float, Size>;

    struct Snapshot : FilterResponseSnapshot
    {
        typename LinearStateVariableFilterGeneric<float, Size>::Snapshot m_lp;
        typename LinearStateVariableFilterGeneric<float, Size>::Snapshot m_hp;

        std::complex<float> ComplexResponse(const ResponseFrequency& frequency, size_t) const override
        {
            return m_lp.ComplexResponse(frequency, 0) * m_hp.ComplexResponse(frequency, 0);
        }

        float MagnitudeResponse(const ResponseFrequency& frequency, size_t) const override
        {
            return m_lp.MagnitudeResponse(frequency, 0) * m_hp.MagnitudeResponse(frequency, 0);
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    Snapshot MakeSnapshot(size_t channel) const
    {
        Snapshot snapshot;
        snapshot.m_lp = m_lpSVF.MakeSnapshot(channel);
        snapshot.m_hp = m_hpSVF.MakeSnapshot(channel);
        snapshot.m_hp.m_weights = {0, 0, 1};
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer, size_t channel) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot(channel);
        });
    }

    using Saturator = TanhSaturatorGeneric<float, Size, true>;

    LinearStateVariableFilterGeneric<float, Size> m_lpSVF;
    LinearStateVariableFilterGeneric<float, Size> m_hpSVF;
    SampleRateReducerGeneric<Size> m_sampleRateReducer;

    // Normalized tanh saturation, Tanh(gain * x) / Tanh(gain), with a
    // per-channel gain.
    //
    Vector m_saturationGain;
    Vector m_saturationTanhGain;

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
            , m_lpCutoffFactor(0.25f, 1024.0f)
            , m_hpCutoffFactor(0.25f, 256.0f)
            , m_saturationGain(0.25f, 5.0f)
            , m_sampleRateReducerFreq(1.0f, 2048.0f)
        {
            m_lpResonance.SetBaseByCenter(0.125);
            m_hpResonance.SetBaseByCenter(0.125);
        }
    };

    SVF2PoleShapeGeneric()
        : m_saturationGain(Vector::Splat(0.25))
        , m_saturationTanhGain(Saturator::Tanh(Vector::Splat(0.25)))
    {
    }

    void SetSaturationGain(const Vector& gain)
    {
        if (gain != m_saturationGain)
        {
            m_saturationGain = gain;
            m_saturationTanhGain = Saturator::Tanh(gain);
        }
    }

    Vector Saturate(const Vector& x) const
    {
        return Saturator::Tanh(x * m_saturationGain) / m_saturationTanhGain;
    }

    Vector Process(const Input& input, const Vector& x)
    {
        Vector hpCutoff = input.m_hpCutoffFactor.m_expParam;
        m_hpSVF.SetCutoffAndResonance(HighPassFreq(input.m_baseFreq, hpCutoff), input.m_hpResonance.m_expParam);
        m_lpSVF.SetCutoffAndResonance(
            LowPassFreq(input.m_baseFreq, hpCutoff, input.m_lpCutoffFactor.m_expParam),
            input.m_lpResonance.m_expParam);
        SetSaturationGain(input.m_saturationGain.m_expParam);

        m_sampleRateReducer.SetFreq(SampleRateReducerFreq(input.m_baseFreq, input.m_sampleRateReducerFreq.m_expParam));
        m_lpSVF.Process(Saturate(x));
        m_hpSVF.Process(m_sampleRateReducer.Process(Saturate(m_lpSVF.GetLowPass())));
        return Saturate(m_hpSVF.GetHighPass());
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
        input.m_sampleRateReducerFreq.Update(Vector::Splat(1) - encoders.GetValue<Size>(Param::SampleRateReduction, track));
    }
};

using TrioSVF2PoleShape = SVF2PoleShapeGeneric<3>;
