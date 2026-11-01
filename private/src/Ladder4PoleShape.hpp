#pragma once

#include "BitCrush.hpp"
#include "LadderFilter.hpp"
#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SquiggleBoyShapeBase.hpp"
#include "StateVariableFilter.hpp"

// 4-pole ladder low-pass into a 4-pole SVF high-pass, then the sample rate
// reducer. One sample per call for every channel.
//
template <size_t Size>
struct Ladder4PoleShapeGeneric : SquiggleBoyShapeBase
{
    using Vector = MultiChannelNumber<float, Size>;

    LadderFilterLPGeneric<float, Size> m_lpLadder;
    LinearSVF4PoleHighPassGeneric<float, Size> m_hp4Pole;
    SampleRateReducerGeneric<Size> m_sampleRateReducer;

    // The base frequency is in cycles per base-rate sample.
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

    Vector Process(const Input& input, const Vector& x)
    {
        Vector hpCutoff = input.m_hpCutoffFactor.m_expParam;
        m_hp4Pole.SetCutoffAndResonance(HighPassFreq(input.m_baseFreq, hpCutoff), input.m_hpResonance.m_expParam);
        m_lpLadder.SetCutoff(LowPassFreq(input.m_baseFreq, hpCutoff, input.m_lpCutoffFactor.m_expParam));
        m_lpLadder.SetResonance(input.m_lpResonance.m_expParam);
        m_lpLadder.SetSaturationGain(input.m_saturationGain.m_expParam);

        m_hp4Pole.Process(m_lpLadder.Process(x));

        m_sampleRateReducer.SetFreq(SampleRateReducerFreq(input.m_baseFreq, input.m_sampleRateReducerFreq.m_expParam));
        return m_sampleRateReducer.Process(m_hp4Pole.GetOutput());
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

using TrioLadder4PoleShape = Ladder4PoleShapeGeneric<3>;
