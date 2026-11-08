#pragma once

#include "BitCrush.hpp"
#include "GangedRandomLFO.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "VectorPhaseShaper.hpp"

// Two cross phase-modulated vector phase shapers, crossfaded and bit crushed.
// Processes one sample per call for every channel.
//
template <size_t Size>
struct DualWaveShapingVCOGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using VCO = VectorPhaseShaperGeneric<Size>;

    VCO m_vco[2];
    GangedRandomLFO<Size> m_morphRandom[2];
    GangedRandomLFOInput m_morphRandomInput = GangedRandomLFOInput::Standard(6.0, 0.1f);
    BitRateReducerGeneric<Size> m_bitRateReducer;
    Vector m_output;

    // Encoder mappings
    //
    PhaseUtils::ExpParamGeneric<float, Size> m_offsetFreqFactorParam;
    PhaseUtils::ExpParamGeneric<float, Size> m_detuneParam;
    PhaseUtils::ZeroedExpParamGeneric<float, Size> m_crossModIndexParam[2];

    ScopeWriterHolderGeneric<Size> m_scopeWriter[2];

    // Frequencies are in cycles per sample at the processing rate.
    //
    struct Input
    {
        Vector m_v[2];
        Vector m_d[2];
        Vector m_morphHarmonics[2];
        Vector m_crossModIndex[2];
        Vector m_baseFreq;
        Vector m_offsetFreqFactor;
        Vector m_detune;
        Vector m_fade;
        Vector m_bitCrushAmount;

        Input()
            : m_v{Vector::Splat(0.5), Vector::Splat(0.5)}
            , m_d{Vector::Splat(0.5), Vector::Splat(0.5)}
            , m_morphHarmonics{Vector::Splat(0), Vector::Splat(0)}
            , m_crossModIndex{Vector::Splat(0), Vector::Splat(0)}
            , m_baseFreq(Vector::Splat(0))
            , m_offsetFreqFactor(Vector::Splat(1))
            , m_detune(Vector::Splat(1))
            , m_fade(Vector::Splat(0))
            , m_bitCrushAmount(Vector::Splat(0))
        {
        }
    };

    DualWaveShapingVCOGeneric()
        : m_output(Vector::Splat(0))
        , m_offsetFreqFactorParam(4.0f)
        , m_detuneParam(1.03f)
    {
    }

    void SetScopeWriters(ScopeWriterInput<Size>* scopeWriter, size_t track)
    {
        m_scopeWriter[0] = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::VCO1), track));
        m_scopeWriter[1] = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::VCO2), track));
    }

    Vector Process(const Input& input)
    {
        typename VCO::Input vcoInput[2];
        for (int j = 0; j < 2; ++j)
        {
            vcoInput[j].m_v = input.m_v[j];
            vcoInput[j].m_d = input.m_d[j];
            vcoInput[j].m_wtBlend = m_morphRandom[j].Process(m_morphRandomInput);
        }

        vcoInput[0].m_freq = input.m_baseFreq * input.m_detune;
        vcoInput[1].m_freq = input.m_baseFreq * input.m_offsetFreqFactor / input.m_detune;
        for (int j = 0; j < 2; ++j)
        {
            vcoInput[j].m_morphHarmonics = PhaseUtils::ExpParamGeneric<float, Size>::ComputeRange(
                vcoInput[j].m_freq,
                Vector::Splat(vcoInput[j].m_maxFreq),
                input.m_morphHarmonics[j]);
        }

        vcoInput[0].m_phaseMod = input.m_crossModIndex[0] * m_vco[1].m_out;
        m_vco[0].Process(vcoInput[0]);
        vcoInput[1].m_phaseMod = m_vco[0].m_out * input.m_crossModIndex[1];
        m_vco[1].Process(vcoInput[1]);

        Vector quarterFade = input.m_fade * 0.25;
        Vector mixed = m_vco[0].m_out * Math::Cos2pi(quarterFade) + m_vco[1].m_out * Math::Cos2pi(quarterFade + 0.75);

        m_bitRateReducer.SetAmount(input.m_bitCrushAmount);
        m_output = m_bitRateReducer.Process(mixed);
        WriteScopes();
        return m_output;
    }

    void WriteScopes()
    {
        for (size_t j = 0; j < 2; ++j)
        {
            m_scopeWriter[j].Write(m_vco[j].m_out);
            for (size_t i = 0; i < Size; ++i)
            {
                if (m_vco[j].m_top[i])
                {
                    m_scopeWriter[j].RecordStartAtVoice(i, m_vco[j].m_top[i].m_offset);
                }
            }
        }
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& baseFreq)
    {
        using Param = SmartGridOneEncoders::Param;

        input.m_baseFreq = baseFreq;
        input.m_morphHarmonics[0] = encoders.GetValue<Size>(Param::Harmonics1, track);
        input.m_morphHarmonics[1] = encoders.GetValue<Size>(Param::Harmonics2, track);

        input.m_v[0] = encoders.GetValue<Size>(Param::VPSV1, track);
        input.m_v[1] = encoders.GetValue<Size>(Param::VPSV2, track);
        input.m_d[0] = encoders.GetValue<Size>(Param::VPSD1, track);
        input.m_d[1] = encoders.GetValue<Size>(Param::VPSD2, track);
        input.m_crossModIndex[0] = m_crossModIndexParam[0].Update(encoders.GetValue<Size>(Param::PhaseMod1, track));
        input.m_crossModIndex[1] = m_crossModIndexParam[1].Update(encoders.GetValue<Size>(Param::PhaseMod2, track));

        input.m_fade = encoders.GetValue<Size>(Param::OscillatorMix, track);
        input.m_bitCrushAmount = encoders.GetValue<Size>(Param::BitReduction, track);
        input.m_offsetFreqFactor = m_offsetFreqFactorParam.Update(encoders.GetValue<Size>(Param::PitchOffset, track));
        input.m_detune = m_detuneParam.Update(encoders.GetValue<Size>(Param::OscillatorDetune, track));
    }
};

using DualWaveShapingVCO = DualWaveShapingVCOGeneric<1>;
using TrioDualWaveShapingVCO = DualWaveShapingVCOGeneric<3>;
