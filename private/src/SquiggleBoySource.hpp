#pragma once

#include "DualWaveShapingVCO.hpp"
#include "DualSampleSource.hpp"
#include "Oversample.hpp"
#include "PhysicalModelingSource.hpp"
#include "SampleTimer.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SourceMixer.hpp"
#include "VoiceMachineEnums.hpp"

struct TheoryOfTime;

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
    PhysicalModelingSource m_physicalModeling;
    DualSampleSource m_dualSample;

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
        TheoryOfTime* m_theoryOfTime;

        DualWaveShapingVCO::Input m_dualWaveShapingVCOInput;
        PhysicalModelingSource::Input m_physicalModelingInput;
        DualSampleSource::Input m_dualSampleInput;

        AudioBufferBank* m_audioBufferBank0;
        AudioBufferBank* m_audioBufferBank1;

        Input()
            : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
            , m_sourceIndex(0)
            , m_sourceMixer(nullptr)
            , m_theoryOfTime(nullptr)
            , m_audioBufferBank0(nullptr)
            , m_audioBufferBank1(nullptr)
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

            case SourceMachine::PhysicalModeling:
            {
                m_physicalModeling.ProcessUBlock(input.m_physicalModelingInput);
                m_uBlockOutput = m_physicalModeling.m_uBlockOutput;
                m_uBlockTop = m_physicalModeling.m_uBlockTop;
                break;
            }

            case SourceMachine::DualSample:
            {        
                m_dualSample.SetAudioBufferBanks(input.m_audioBufferBank0, input.m_audioBufferBank1);
                input.m_dualSampleInput.m_theoryOfTime = input.m_theoryOfTime;
                m_dualSample.ProcessUBlock(input.m_dualSampleInput);
                m_uBlockOutput = m_dualSample.m_uBlockOutput;
                m_uBlockTop = m_dualSample.m_uBlockTop;
                break;
            }

            default:
                break;
        }
    }

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
                m_dualWaveShapingVCO.SetEncoderParams(
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

            case SourceMachine::PhysicalModeling:
            {
                m_physicalModeling.SetEncoderParams(encoders, input.m_physicalModelingInput, voiceIx, baseFreq);
                break;
            }

            case SourceMachine::DualSample:
            {
                m_dualSample.SetEncoderParams(encoders, input.m_dualSampleInput, voiceIx, baseFreq);
                break;
            }

            default:
                break;
        }
    }
};
