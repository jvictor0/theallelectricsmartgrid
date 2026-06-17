#pragma once

#include <cstddef>

#include "VoiceMachineEnums.hpp"
#include "RecordingConfig.hpp"

struct AudioBufferBank;

struct SquiggleBoyVoiceConfig
{
    using SourceMachine = VoiceMachine::SourceMachine;
    using FilterMachine = VoiceMachine::FilterMachine;

    struct SourceAssignment
    {
        enum class Channel
        {
            Silent,
            Left,
            Right,
        };

        Channel m_channel;
        size_t m_sourceIndex;

        SourceAssignment()
            : m_channel(Channel::Left)
            , m_sourceIndex(0)
        {
        }

        static SourceAssignment Silent()
        {
            SourceAssignment result;
            result.m_channel = Channel::Silent;
            result.m_sourceIndex = 0;
            return result;
        }

        static SourceAssignment Left(size_t sourceIndex)
        {
            SourceAssignment result;
            result.m_channel = Channel::Left;
            result.m_sourceIndex = sourceIndex;
            return result;
        }

        static SourceAssignment Right(size_t sourceIndex)
        {
            SourceAssignment result;
            result.m_channel = Channel::Right;
            result.m_sourceIndex = sourceIndex;
            return result;
        }
    };

    SourceMachine m_sourceMachine;
    FilterMachine m_filterMachine;
    SourceAssignment m_sourceAssignment;

    AudioBufferBank* m_audioBufferBank;

    RecordingConfig m_recordingConfig;

    SquiggleBoyVoiceConfig()
        : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
        , m_filterMachine(FilterMachine::Ladder4Pole)
        , m_sourceAssignment(SourceAssignment::Left(0))
        , m_audioBufferBank(nullptr)
    {
    }
};
