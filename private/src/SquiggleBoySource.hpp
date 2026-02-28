#pragma once

#include "DualWaveShapingVCO.hpp"
#include "Oversample.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SourceMixer.hpp"
#include "VoiceMachineEnums.hpp"

// Encapsulates source machine selection and dispatch logic.
// Contains all source machines and handles ProcessUBlock dispatch and encoder wiring.
//
struct SquiggleBoySource
{
    using SourceMachine = VoiceMachine::SourceMachine;

    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    // Source machines
    //
    DualWaveShapingVCO m_dualWaveShapingVCO;

    // Output buffers
    //
    float* m_uBlockOutput;
    bool* m_uBlockTop;

    // Thru mode upsampling
    //
    Upsampler m_upsampler;
    float m_upsampledSource[x_uBlockSize];

    struct Input
    {
        SourceMachine m_sourceMachine;
        size_t m_sourceIndex;
        SourceMixer* m_sourceMixer;

        DualWaveShapingVCO::Input m_dualWaveShapingVCOInput;

        Input()
            : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
            , m_sourceIndex(0)
            , m_sourceMixer(nullptr)
        {
        }
    };

    SquiggleBoySource()
        : m_uBlockOutput(nullptr)
        , m_uBlockTop(nullptr)
        , m_upsampler(x_oversample)
    {
    }

    void SetScopeWriters(ScopeWriter* scopeWriter, size_t voiceIx)
    {
        m_dualWaveShapingVCO.m_scopeWriter[0] = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::VCO1));
        m_dualWaveShapingVCO.m_scopeWriter[1] = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::VCO2));
    }

    void ProcessUBlock(Input& input)
    {
        switch (input.m_sourceMachine)
        {
            case SourceMachine::DualWaveShapingVCO:
            {
                m_dualWaveShapingVCO.ProcessUBlock(input.m_dualWaveShapingVCOInput);
                m_uBlockOutput = m_dualWaveShapingVCO.m_uBlockOutput;
                m_uBlockTop = m_dualWaveShapingVCO.m_uBlockTop;
                break;
            }

            case SourceMachine::Thru:
            {
                const float* sourceBlock = input.m_sourceMixer->m_sources[input.m_sourceIndex].m_uBlockOutput;
                m_upsampler.Process(sourceBlock, m_upsampledSource);
                m_uBlockOutput = m_upsampledSource;
                m_uBlockTop = m_dualWaveShapingVCO.m_uBlockTop;
                break;
            }

            default:
                break;
        }
    }

    using Param = SmartGridOneEncoders::Param;

    // Called from SetEncoderParameters to set source-specific encoder params.
    // Takes the encoder bank and voice index, dispatches to the appropriate machine.
    //
    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t voiceIx,
        float baseFreq,
        float wtBlend0,
        float wtBlend1)
    {
        switch (input.m_sourceMachine)
        {
            case SourceMachine::DualWaveShapingVCO:
            {
                SetDualWaveShapingVCOParams(
                    encoders, input.m_dualWaveShapingVCOInput, voiceIx, baseFreq, wtBlend0, wtBlend1);
                break;
            }

            case SourceMachine::Thru:
            {
                // Thru mode uses external audio, no encoder params to set
                //
                input.m_dualWaveShapingVCOInput.m_baseFreq = baseFreq;
                break;
            }

            default:
                break;
        }
    }

    void SetDualWaveShapingVCOParams(
        SmartGridOneEncoders& encoders,
        DualWaveShapingVCO::Input& vcoInput,
        size_t voiceIx,
        float baseFreq,
        float wtBlend0,
        float wtBlend1)
    {
        vcoInput.m_baseFreq = baseFreq;
        vcoInput.m_morphHarmonics[0] = encoders.GetValueNoSlew(Param::Harmonics1, voiceIx);
        vcoInput.m_morphHarmonics[1] = encoders.GetValueNoSlew(Param::Harmonics2, voiceIx);
        vcoInput.m_wtBlend[0] = wtBlend0;
        vcoInput.m_wtBlend[1] = wtBlend1;

        vcoInput.m_v[0] = encoders.GetValueNoSlew(Param::VPSV1, voiceIx);
        vcoInput.m_v[1] = encoders.GetValueNoSlew(Param::VPSV2, voiceIx);
        vcoInput.m_d[0] = encoders.GetValueNoSlew(Param::VPSD1, voiceIx);
        vcoInput.m_d[1] = encoders.GetValueNoSlew(Param::VPSD2, voiceIx);
        vcoInput.m_crossModIndex[0].Update(encoders.GetValueNoSlew(Param::PhaseMod1, voiceIx));
        vcoInput.m_crossModIndex[1].Update(encoders.GetValueNoSlew(Param::PhaseMod2, voiceIx));

        vcoInput.m_fade = encoders.GetValueNoSlew(Param::OscillatorMix, voiceIx);
        vcoInput.m_bitCrushAmount = encoders.GetValueNoSlew(Param::BitReduction, voiceIx);
        vcoInput.m_offsetFreqFactor.Update(encoders.GetValueNoSlew(Param::PitchOffset, voiceIx));
        vcoInput.m_detune.Update(encoders.GetValueNoSlew(Param::OscillatorDetune, voiceIx));
    }
};
