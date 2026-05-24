#pragma once

#include "VoiceMachineEnums.hpp"
#include "RecordingConfig.hpp"

struct AudioBufferBank;

struct SquiggleBoyVoiceConfig
{
    using SourceMachine = VoiceMachine::SourceMachine;
    using FilterMachine = VoiceMachine::FilterMachine;

    SourceMachine m_sourceMachine;
    FilterMachine m_filterMachine;
    int m_sourceIndex;

    AudioBufferBank* m_audioBufferBank;

    RecordingConfig m_recordingConfig;

    SquiggleBoyVoiceConfig()
        : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
        , m_filterMachine(FilterMachine::Ladder4Pole)
        , m_sourceIndex(0)
        , m_audioBufferBank(nullptr)
    {
    }
};
