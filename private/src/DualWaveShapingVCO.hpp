#pragma once

#include "BitCrush.hpp"
#include "Math.hpp"
#include "MultiChannelNumber.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "ScopeWriter.hpp"
#include "Slew.hpp"
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
    BitRateReducerGeneric<Size> m_bitRateReducer;
    Vector m_output;

    // Frequencies are in cycles per sample at the processing rate.
    //
    struct Input
    {
        Vector m_v[2];
        Vector m_d[2];
        Vector m_wtBlend[2];
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
            , m_wtBlend{Vector::Splat(0), Vector::Splat(0)}
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
    {
    }

    Vector Process(const Input& input, double deltaT)
    {
        typename VCO::Input vcoInput[2];
        for (int j = 0; j < 2; ++j)
        {
            vcoInput[j].m_v = input.m_v[j];
            vcoInput[j].m_d = input.m_d[j];
            vcoInput[j].m_wtBlend = input.m_wtBlend[j];
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
        m_vco[0].Process(vcoInput[0], deltaT);
        vcoInput[1].m_phaseMod = m_vco[0].m_out * input.m_crossModIndex[1];
        m_vco[1].Process(vcoInput[1], deltaT);

        Vector quarterFade = input.m_fade * 0.25;
        Vector mixed = m_vco[0].m_out * Math::Cos2pi(quarterFade) + m_vco[1].m_out * Math::Cos2pi(quarterFade + 0.75);

        m_bitRateReducer.SetAmount(input.m_bitCrushAmount);
        m_output = m_bitRateReducer.Process(mixed);
        return m_output;
    }
};

using TrioDualWaveShapingVCO = DualWaveShapingVCOGeneric<3>;

// Mono micro-block shell: slews the control-rate inputs into the core at four
// times the base rate.
//
struct DualWaveShapingVCO
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    using Core = DualWaveShapingVCOGeneric<1>;

    Core m_core;

    SampleTop m_top;
    float m_output;

    SampleTop m_uBlockTop[SampleTimer::x_controlFrameRate];
    float m_uBlockOutput[x_uBlockSize];

    ScopeWriterHolder m_scopeWriter[2];

    // Slewed parameters
    //
    ParamSlew m_vSlew[2];
    ParamSlew m_dSlew[2];
    ParamSlew m_wtBlendSlew[2];
    ParamSlew m_baseFreqSlew;
    ParamSlew m_fadeSlew;
    ParamSlew m_bitCrushAmountSlew;
    ParamSlew m_offsetFreqFactorSlew;
    ParamSlew m_detuneSlew;
    ParamSlew m_crossModIndexSlew[2];

    struct Input
    {
        float m_v[2];
        float m_d[2];
        float m_baseFreq;

        float m_morphHarmonics[2];
        float m_wtBlend[2];

        PhaseUtils::ExpParam m_offsetFreqFactor;
        PhaseUtils::ExpParam m_detune;

        PhaseUtils::ZeroedExpParam m_crossModIndex[2];

        float m_fade;

        float m_bitCrushAmount;

        Input()
            : m_v{0.5, 0.5}
            , m_d{0.5, 0.5}
            , m_baseFreq(PhaseUtils::VOctToNatural(0.0, SampleTimer::x_samplePeriod))
            , m_morphHarmonics{0.0, 0.0}
            , m_wtBlend{0, 0}
            , m_offsetFreqFactor(4)
            , m_detune(1.03)
            , m_fade(0)
            , m_bitCrushAmount(0)
        {
        }
    };

    DualWaveShapingVCO()
        : m_output(0)
        , m_vSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
        , m_dSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
        , m_wtBlendSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
        , m_baseFreqSlew(x_oversample)
        , m_fadeSlew(x_oversample)
        , m_bitCrushAmountSlew(x_oversample)
        , m_offsetFreqFactorSlew(x_oversample)
        , m_detuneSlew(x_oversample)
        , m_crossModIndexSlew{ParamSlew(x_oversample), ParamSlew(x_oversample)}
    {
    }

    void ProcessUBlock(const Input& input)
    {
        m_output = 0;

        // Update slew targets
        //
        for (int j = 0; j < 2; ++j)
        {
            m_vSlew[j].Update(input.m_v[j]);
            m_dSlew[j].Update(input.m_d[j]);
            m_wtBlendSlew[j].Update(input.m_wtBlend[j]);
            m_crossModIndexSlew[j].Update(input.m_crossModIndex[j].m_expParam);
        }

        m_baseFreqSlew.Update(input.m_baseFreq);
        m_fadeSlew.Update(input.m_fade);
        m_bitCrushAmountSlew.Update(input.m_bitCrushAmount);
        m_offsetFreqFactorSlew.Update(input.m_offsetFreqFactor.m_expParam);
        m_detuneSlew.Update(input.m_detune.m_expParam);

        SampleTop top[2];
        Core::Input coreInput;

        for (size_t i = 0; i < x_uBlockSize; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                coreInput.m_v[j] = m_vSlew[j].Process();
                coreInput.m_d[j] = m_dSlew[j].Process();
                coreInput.m_wtBlend[j] = m_wtBlendSlew[j].Process();
                coreInput.m_morphHarmonics[j] = input.m_morphHarmonics[j];
                coreInput.m_crossModIndex[j] = m_crossModIndexSlew[j].Process();
            }

            coreInput.m_baseFreq = m_baseFreqSlew.Process() / x_oversample;
            coreInput.m_offsetFreqFactor = m_offsetFreqFactorSlew.Process();
            coreInput.m_detune = m_detuneSlew.Process();
            coreInput.m_fade = m_fadeSlew.Process();
            coreInput.m_bitCrushAmount = m_bitCrushAmountSlew.Process();

            m_uBlockOutput[i] = m_core.Process(coreInput, SampleTimer::x_samplePeriod / x_oversample);

            top[0].AccumulateOversample(m_core.m_vco[0].m_top[0], i, x_oversample);
            top[1].AccumulateOversample(m_core.m_vco[1].m_top[0], i, x_oversample);

            // Base-rate operations every x_oversample samples
            //
            if ((i + 1) % x_oversample == 0)
            {
                size_t baseIndex = i / x_oversample;

                m_scopeWriter[0].Write(baseIndex, m_core.m_vco[0].m_out);
                m_scopeWriter[1].Write(baseIndex, m_core.m_vco[1].m_out);

                if (top[0])
                {
                    m_scopeWriter[0].RecordStart(top[0].GetPosition(baseIndex));
                }

                if (top[1])
                {
                    m_scopeWriter[1].RecordStart(top[1].GetPosition(baseIndex));
                }

                m_uBlockTop[baseIndex] = top[0];
                top[0] = false;
                top[1] = false;
            }
        }
    }

    float Process(const Input& input)
    {
        if (SampleTimer::IsControlFrame())
        {
            ProcessUBlock(input);
        }

        m_output = m_uBlockOutput[SampleTimer::GetUBlockIndex()];
        m_top = m_uBlockTop[SampleTimer::GetUBlockIndex()];

        return m_output;
    }

    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t voiceIx,
        float baseFreq,
        float wtBlend0,
        float wtBlend1)
    {
        using Param = SmartGridOneEncoders::Param;

        input.m_baseFreq = baseFreq;
        input.m_morphHarmonics[0] = encoders.GetValue(Param::Harmonics1, voiceIx);
        input.m_morphHarmonics[1] = encoders.GetValue(Param::Harmonics2, voiceIx);
        input.m_wtBlend[0] = wtBlend0;
        input.m_wtBlend[1] = wtBlend1;

        input.m_v[0] = encoders.GetValue(Param::VPSV1, voiceIx);
        input.m_v[1] = encoders.GetValue(Param::VPSV2, voiceIx);
        input.m_d[0] = encoders.GetValue(Param::VPSD1, voiceIx);
        input.m_d[1] = encoders.GetValue(Param::VPSD2, voiceIx);
        input.m_crossModIndex[0].Update(encoders.GetValue(Param::PhaseMod1, voiceIx));
        input.m_crossModIndex[1].Update(encoders.GetValue(Param::PhaseMod2, voiceIx));

        input.m_fade = encoders.GetValue(Param::OscillatorMix, voiceIx);
        input.m_bitCrushAmount = encoders.GetValue(Param::BitReduction, voiceIx);
        input.m_offsetFreqFactor.Update(encoders.GetValue(Param::PitchOffset, voiceIx));
        input.m_detune.Update(encoders.GetValue(Param::OscillatorDetune, voiceIx));
    }
};
