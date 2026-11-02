#pragma once

#include "AudioBuffer.hpp"
#include "DualWaveShapingVCO.hpp"
#include "DualSampleSource.hpp"
#include "MultiChannelNumber.hpp"
#include "PhysicalModelingSource.hpp"
#include "SampleTimer.hpp"
#include "SampleTop.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SourceMixer.hpp"
#include "SquiggleBoyVoiceConfig.hpp"
#include "VoiceMachineEnums.hpp"

struct TheoryOfTime;

// Encapsulates source machine selection and dispatch logic. Contains all
// source machines and handles Process dispatch and encoder wiring. One sample
// per call for every channel; channel i is voice track * Size + i.
//
template <size_t Size>
struct SquiggleBoySourceGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using SourceMachine = VoiceMachine::SourceMachine;
    using DualWaveShapingVCOType = DualWaveShapingVCOGeneric<Size>;
    using PhysicalModelingType = PhysicalModelingSourceGeneric<Size>;
    using SampleSourceType = SampleSourceGeneric<Size>;

    // Source machines
    //
    DualWaveShapingVCOType m_dualWaveShapingVCO;
    PhysicalModelingType m_physicalModeling;
    SampleSourceType m_sampleSource;

    Vector m_output;
    SampleTop m_top[Size];

    struct Input
    {
        using SourceAssignment = SquiggleBoyVoiceConfig::SourceAssignment;

        SourceMachine m_sourceMachine;
        SourceAssignment m_sourceAssignment[Size];
        SourceMixer* m_sourceMixer;
        TheoryOfTime* m_theoryOfTime;

        typename DualWaveShapingVCOType::Input m_dualWaveShapingVCOInput;
        typename PhysicalModelingType::Input m_physicalModelingInput;
        typename SampleSourceType::Input m_sampleSourceInput;

        AudioBufferBank* m_audioBufferBank[Size];

        Input()
            : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
            , m_sourceMixer(nullptr)
            , m_theoryOfTime(nullptr)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_sourceAssignment[i] = SourceAssignment::Left(0);
                m_audioBufferBank[i] = nullptr;
            }
        }
    };

    SquiggleBoySourceGeneric()
        : m_physicalModeling(42)
        , m_output(Vector::Splat(0))
    {
        ClearTops();
    }

    void SetScopeWriters(ScopeWriter* scopeWriter, size_t track)
    {
        m_dualWaveShapingVCO.SetScopeWriters(scopeWriter, track);
    }

    void ClearTops()
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_top[i] = false;
        }
    }

    Vector Process(Input& input)
    {
        ClearTops();
        switch (input.m_sourceMachine)
        {
            case SourceMachine::DualWaveShapingVCO:
            {
                m_output = m_dualWaveShapingVCO.Process(input.m_dualWaveShapingVCOInput);
                for (size_t i = 0; i < Size; ++i)
                {
                    m_top[i] = m_dualWaveShapingVCO.m_vco[0].m_top[i];
                }

                break;
            }

            case SourceMachine::Thru:
            {
                for (size_t i = 0; i < Size; ++i)
                {
                    m_output[i] = GetThruSample(input, i);
                }

                break;
            }

            case SourceMachine::PhysicalModeling:
            {
                input.m_physicalModelingInput.m_ahdInput.SetSamplePosition(static_cast<float>(SampleTimer::GetUBlockIndex()));
                m_output = m_physicalModeling.ProcessWithAHD(input.m_physicalModelingInput);
                break;
            }

            case SourceMachine::Sample:
            {
                for (size_t i = 0; i < Size; ++i)
                {
                    m_sampleSource.SetAudioBufferBank(i, input.m_audioBufferBank[i]);
                }

                input.m_sampleSourceInput.m_theoryOfTime = input.m_theoryOfTime;
                m_output = m_sampleSource.Process(input.m_sampleSourceInput, SampleTimer::GetUBlockIndex());
                for (size_t i = 0; i < Size; ++i)
                {
                    m_top[i] = m_sampleSource.m_top[i];
                }

                break;
            }

            default:
                break;
        }

        return m_output;
    }

    float GetThruSample(Input& input, size_t channel)
    {
        using SourceAssignment = SquiggleBoyVoiceConfig::SourceAssignment;
        using Channel = SourceAssignment::Channel;

        if (!input.m_sourceMixer)
        {
            return 0.0f;
        }

        const SourceAssignment& assignment = input.m_sourceAssignment[channel];
        if (assignment.m_channel == Channel::Silent)
        {
            return 0.0f;
        }

        if (SourceMixer::x_numSources <= assignment.m_sourceIndex)
        {
            return 0.0f;
        }

        size_t lane = assignment.m_channel == Channel::Right ? 1 : 0;
        return input.m_sourceMixer->m_sources[assignment.m_sourceIndex].m_uBlockOutput[lane][SampleTimer::GetUBlockIndex()];
    }

    // Called from SetEncoderParameters to set source-specific encoder params.
    // Dispatches to the active machine.
    //
    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& baseFreq,
        const Vector& wtBlend0,
        const Vector& wtBlend1)
    {
        switch (input.m_sourceMachine)
        {
            case SourceMachine::DualWaveShapingVCO:
            {
                m_dualWaveShapingVCO.SetEncoderParams(
                    encoders, input.m_dualWaveShapingVCOInput, track, baseFreq, wtBlend0, wtBlend1);
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
                m_physicalModeling.SetEncoderParams(encoders, input.m_physicalModelingInput, track, baseFreq);
                break;
            }

            case SourceMachine::Sample:
            {
                m_sampleSource.SetEncoderParams(encoders, input.m_sampleSourceInput, track);
                break;
            }

            default:
                break;
        }
    }
};

using SquiggleBoySource = SquiggleBoySourceGeneric<1>;
using TrioSquiggleBoySource = SquiggleBoySourceGeneric<3>;
