#pragma once

#include <cstring>

#include "SquiggleBoy.hpp"
#include "TheNonagon.hpp"
#include "VoiceMachineEnums.hpp"
#include "IOTaskThread.hpp"
#include "JuceSon.hpp"
#include "AudioBuffer.hpp"

struct SquiggleBoyConfigGrid : public SmartGrid::Grid
{
    static constexpr int x_numSourceMachines = static_cast<int>(VoiceMachine::SourceMachine::NumSourceMachines);
    static constexpr int x_numFilterMachines = static_cast<int>(VoiceMachine::FilterMachine::NumFilterMachines);

    enum class SampleDirectorySlot
    {
        None,
        One,
    };

    template<typename EnumT, bool Dec, int NumValues>
    struct EnumIncDecCell : SmartGrid::Cell
    {
        SquiggleBoyConfigGrid* m_owner;
        State** m_states;

        EnumIncDecCell(SquiggleBoyConfigGrid* owner, State** states)
            : m_owner(owner)
            , m_states(states)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            TheNonagonSmartGrid::Trio trio = m_owner->ActiveTrio();
            return IsPressed() ? TheNonagonSmartGrid::TrioColor(trio) : TheNonagonSmartGrid::TrioColor(trio).Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            size_t baseIdx = static_cast<size_t>(m_owner->ActiveTrio()) * TheNonagonInternal::x_voicesPerTrio;
            int current = static_cast<int>(m_states[baseIdx]->template Get<EnumT>());
            int next = Dec ? (current - 1 + NumValues) % NumValues : (current + 1) % NumValues;
            EnumT value = static_cast<EnumT>(next);

            for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
            {
                m_states[baseIdx + i]->Set(value);
            }

            m_owner->m_squiggleBoy->UpdateEncodersForMachine();
        }
    };

    struct SourceSelectCell : SmartGrid::Cell
    {
        size_t m_sourceIndex;
        SquiggleBoyConfigGrid* m_owner;

        SourceSelectCell(SquiggleBoyConfigGrid* owner, size_t sourceIndex)
            : m_sourceIndex(sourceIndex)
            , m_owner(owner)
        {
            for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
            {
                m_states[i] = m_owner->m_sourceSelectedStates[i][m_sourceIndex];
            }
        }

        virtual SmartGrid::Color GetColor() override
        {
            size_t trioIdx = static_cast<size_t>(m_owner->ActiveTrio());
            SmartGrid::Color color = m_owner->SourceColor(m_sourceIndex);
            return m_states[trioIdx]->Get<bool>() ? color : color.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            size_t trioIdx = static_cast<size_t>(m_owner->ActiveTrio());
            bool selected = m_states[trioIdx]->Get<bool>();
            m_states[trioIdx]->Set(!selected);
            m_owner->EnforceSourceChannelLimit(trioIdx, m_sourceIndex);
            m_owner->PropagateSourceSelection();
        }

        State* m_states[TheNonagonInternal::x_numTrios];
    };

    struct SourceStateCell : SmartGrid::Cell
    {
        SquiggleBoyConfigGrid* m_owner;
        size_t m_sourceIndex;
        State* m_state;

        SourceStateCell(SquiggleBoyConfigGrid* owner, size_t sourceIndex, State* state)
            : m_owner(owner)
            , m_sourceIndex(sourceIndex)
            , m_state(state)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            SmartGrid::Color color = m_owner->SourceColor(m_sourceIndex);
            bool state = m_state->Get<bool>();
            return state ? color : color.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            m_state->Set(!m_state->Get<bool>());
        }
    };

    struct SourceWidthCell : SmartGrid::Cell
    {
        SquiggleBoyConfigGrid* m_owner;
        size_t m_sourceIndex;
        State* m_state;

        SourceWidthCell(SquiggleBoyConfigGrid* owner, size_t sourceIndex, State* state)
            : m_owner(owner)
            , m_sourceIndex(sourceIndex)
            , m_state(state)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            SmartGrid::Color color = m_owner->SourceColor(m_sourceIndex);
            return m_owner->IsSourceStereo(m_sourceIndex) ? color : color.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            SourceMixer::SourceWidth width = m_state->Get<SourceMixer::SourceWidth>();
            width = width == SourceMixer::SourceWidth::Stereo
                ? SourceMixer::SourceWidth::Mono
                : SourceMixer::SourceWidth::Stereo;
            m_state->Set(width);

            for (size_t trioIdx = 0; trioIdx < TheNonagonInternal::x_numTrios; ++trioIdx)
            {
                m_owner->EnforceSourceChannelLimit(trioIdx, m_sourceIndex);
            }

            m_owner->PropagateSourceSelection();
        }
    };

    struct SampleDirectoryInitCell : SmartGrid::Cell
    {
        SquiggleBoyConfigGrid* m_owner;
        SampleDirectorySlot m_slot;
        size_t m_voiceIndex;

        SampleDirectoryInitCell(SquiggleBoyConfigGrid* owner, SampleDirectorySlot slot, size_t voiceIndex)
            : m_owner(owner)
            , m_slot(slot)
            , m_voiceIndex(voiceIndex)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            TheNonagonSmartGrid::Trio trio = m_owner->ActiveTrio();
            return IsPressed() ? TheNonagonSmartGrid::TrioColor(trio) : TheNonagonSmartGrid::TrioColor(trio).Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            m_owner->RequestDirectoryExplorerInit(m_slot, m_voiceIndex);
        }
    };

    struct RecordingToggleCell : SmartGrid::Cell
    {
        SquiggleBoyConfigGrid* m_owner;

        RecordingToggleCell(SquiggleBoyConfigGrid* owner)
            : m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            bool anyRecording = false;

            anyRecording = m_owner->m_squiggleBoy->m_recordingManager.AnyRecording();

            SmartGrid::Color color = anyRecording ? SmartGrid::Color::Red : SmartGrid::Color::White;
            return IsPressed() ? color : color.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            RecordingManager& recordingManager = m_owner->m_squiggleBoy->m_recordingManager;

            if (recordingManager.AnyRecording())
            {
                recordingManager.StopRecording();
            }
            else
            {
                recordingManager.StartRecording(static_cast<TheNonagonInternal::Trio>(m_owner->ActiveTrio()));
            }
        }
    };

    struct DirectoryExplorerNavCell : SmartGrid::Cell
    {
        SquiggleBoyConfigGrid* m_owner;
        DirectoryExplorer::MessageType m_messageType;
        SmartGrid::Color m_idleColor;

        DirectoryExplorerNavCell(
            SquiggleBoyConfigGrid* owner,
            DirectoryExplorer::MessageType messageType,
            SmartGrid::Color idleColor)
            : m_owner(owner)
            , m_messageType(messageType)
            , m_idleColor(idleColor)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return IsPressed() ? m_idleColor : m_idleColor.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            if (!m_owner->m_ioTaskThread)
            {
                return;
            }

            AudioBufferBank** audioBufferBankSink = nullptr;
            if (m_messageType == DirectoryExplorer::MessageType::Yes
                && m_owner->m_activeSampleDirectorySlot != SampleDirectorySlot::None
                && m_owner->m_activeSampleDirectoryVoice < TheNonagonInternal::x_voicesPerTrio)
            {
                size_t baseIdx = static_cast<size_t>(m_owner->ActiveTrio()) * TheNonagonInternal::x_voicesPerTrio;
                size_t voiceIdx = baseIdx + m_owner->m_activeSampleDirectoryVoice;
                audioBufferBankSink = &m_owner->m_squiggleBoy->m_state[voiceIdx].m_voiceConfig.m_audioBufferBank;
            }

            m_owner->m_ioTaskThread->PushDirectoryExplorerCommand(&m_owner->m_directoryExplorer, m_messageType, audioBufferBankSink);

            if (m_messageType == DirectoryExplorer::MessageType::Yes
                || m_messageType == DirectoryExplorer::MessageType::No)
            {
                m_owner->m_activeSampleDirectorySlot = SampleDirectorySlot::None;
                m_owner->m_activeSampleDirectoryVoice = m_owner->x_noActiveSampleDirectoryVoice;
                return;
            }
        }
    };

    void PropagateSourceSelection()
    {
        for (size_t tIdx = 0; tIdx < TheNonagonInternal::x_numTrios; ++tIdx)
        {
            size_t voiceIdx = 0;

            for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
            {
                if (!m_sourceSelected[tIdx][i])
                {
                    continue;
                }

                AssignSourceChannel(tIdx, voiceIdx, SquiggleBoyVoice::VoiceConfig::SourceAssignment::Left(i));
                ++voiceIdx;

                if (TheNonagonInternal::x_voicesPerTrio <= voiceIdx)
                {
                    break;
                }

                if (IsSourceStereo(i))
                {
                    AssignSourceChannel(tIdx, voiceIdx, SquiggleBoyVoice::VoiceConfig::SourceAssignment::Right(i));
                    ++voiceIdx;
                }

                if (TheNonagonInternal::x_voicesPerTrio <= voiceIdx)
                {
                    break;
                }
            }

            while (voiceIdx < TheNonagonInternal::x_voicesPerTrio)
            {
                AssignSourceChannel(tIdx, voiceIdx, SquiggleBoyVoice::VoiceConfig::SourceAssignment::Silent());
                ++voiceIdx;
            }
        }
    }

    void AssignSourceChannel(
        size_t trioIdx,
        size_t voiceInTrio,
        SquiggleBoyVoice::VoiceConfig::SourceAssignment assignment)
    {
        size_t voiceIdx = trioIdx * TheNonagonInternal::x_voicesPerTrio + voiceInTrio;
        m_squiggleBoy->m_state[voiceIdx].m_voiceConfig.m_sourceAssignment = assignment;
    }

    size_t SourceChannelCount(size_t trioIdx) const
    {
        size_t count = 0;
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            if (m_sourceSelected[trioIdx][i])
            {
                count += SourceChannelWidth(i);
            }
        }

        return count;
    }

    size_t SourceChannelWidth(size_t sourceIndex) const
    {
        return IsSourceStereo(sourceIndex) ? 2 : 1;
    }

    void EnforceSourceChannelLimit(size_t trioIdx, size_t preferredSource)
    {
        size_t count = SourceChannelCount(trioIdx);
        if (count <= TheNonagonInternal::x_voicesPerTrio)
        {
            return;
        }

        for (size_t i = 0; i < SourceMixer::x_numSources && TheNonagonInternal::x_voicesPerTrio < count; ++i)
        {
            if (i == preferredSource || !m_sourceSelected[trioIdx][i])
            {
                continue;
            }

            count -= SourceChannelWidth(i);
            m_sourceSelectedStates[trioIdx][i]->Set(false);
        }

        if (TheNonagonInternal::x_voicesPerTrio < count && preferredSource < SourceMixer::x_numSources)
        {
            m_sourceSelectedStates[trioIdx][preferredSource]->Set(false);
        }
    }

    bool IsSourceStereo(size_t sourceIndex) const
    {
        return m_squiggleBoy
            && sourceIndex < SourceMixer::x_numSources
            && m_squiggleBoy->m_sourceMixerState.m_sources[sourceIndex].m_config.IsStereo();
    }

    SmartGrid::Color SourceColor(size_t sourceIndex) const
    {
        if (!m_squiggleBoy || SourceMixer::x_numSources <= sourceIndex)
        {
            return SmartGrid::Color::White;
        }

        return SourceMixer::GetColor(sourceIndex);
    }

    SquiggleBoyConfigGrid()
        : m_squiggleBoy(nullptr)
        , m_activeTrio(nullptr)
        , m_ioTaskThread(nullptr)
    {
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_sourceMonitor[i] = true;
        }
    }

    TheNonagonSmartGrid::Trio ActiveTrio()
    {
        if (m_activeTrio)
        {
            return *m_activeTrio;
        }

        return TheNonagonSmartGrid::Trio::Fire;
    }

    void Init(SquiggleBoyWithEncoderBank* squiggleBoyWithEncoders, TheNonagonSmartGrid::Trio* activeTrio, SquiggleBoyWithEncoderBank::UIState* squiggleBoyUIState)
    {
        m_squiggleBoy = squiggleBoyWithEncoders;
        m_activeTrio = activeTrio;
        m_directoryExplorer.m_UIState = &squiggleBoyUIState->m_directoryExplorerUIState;

        for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
        {
            for (size_t j = 0; j < SourceMixer::x_numSources; ++j)
            {
                m_sourceSelected[i][j] = j == 0;
            }
        }

        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            m_voiceSourceMachineStates[i] = squiggleBoyWithEncoders->m_stateSaver->Insert(
                "voiceSourceMachine", i, &squiggleBoyWithEncoders->m_state[i].m_voiceConfig.m_sourceMachine);
            m_voiceFilterMachineStates[i] = squiggleBoyWithEncoders->m_stateSaver->Insert(
                "voiceFilterMachine", i, &squiggleBoyWithEncoders->m_state[i].m_voiceConfig.m_filterMachine);
        }

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_deepVocoderSendStates[i] = squiggleBoyWithEncoders->m_stateSaver->Insert(
                "deepVocoderSend", i, &squiggleBoyWithEncoders->m_sourceMixerState.m_deepVocoderSend[i]);
            m_sourceWidthStates[i] = squiggleBoyWithEncoders->m_stateSaver->Insert(
                "sourceWidth", i, &squiggleBoyWithEncoders->m_sourceMixerState.m_sources[i].m_config.m_width);
            m_sourceMonitorStates[i] = squiggleBoyWithEncoders->m_stateSaver->Insert(
                "sourceMonitor", i, &m_sourceMonitor[i]);

            for (size_t j = 0; j < TheNonagonInternal::x_numTrios; ++j)
            {
                m_sourceSelectedStates[j][i] = m_squiggleBoy->m_stateSaver->Insert(
                    "sourceSelected", j, i, &m_sourceSelected[j][i]);
            }
        }

        Put(0, 0, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::SourceMachine, true, x_numSourceMachines>(
            this, m_voiceSourceMachineStates));
        Put(0, 1, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::SourceMachine, false, x_numSourceMachines>(
            this, m_voiceSourceMachineStates));

        Put(0, 6, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::FilterMachine, true, x_numFilterMachines>(
            this, m_voiceFilterMachineStates));
        Put(0, 7, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::FilterMachine, false, x_numFilterMachines>(
            this, m_voiceFilterMachineStates));

        Put(1, 0, new SampleDirectoryInitCell(this, SampleDirectorySlot::One, 0));
        Put(2, 0, new SampleDirectoryInitCell(this, SampleDirectorySlot::One, 1));
        Put(3, 0, new SampleDirectoryInitCell(this, SampleDirectorySlot::One, 2));
        Put(4, 0, new RecordingToggleCell(this));

        Put(6, 6, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Up, SmartGrid::Color::Cyan));
        Put(5, 6, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::No, SmartGrid::Color::Red));
        Put(7, 6, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Yes, SmartGrid::Color::Green));
        Put(5, 7, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Left, SmartGrid::Color::Cyan));
        Put(6, 7, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Down, SmartGrid::Color::Cyan));
        Put(7, 7, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Right, SmartGrid::Color::Cyan));

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            Put(0, 2 + i, new SourceSelectCell(this, i));
            Put(6 + (i % 2), i / 2, new SourceStateCell(
                this,
                i,
                m_sourceMonitorStates[i]));
            Put(6 + (i % 2), 2 + (i / 2), new SourceStateCell(
                this,
                i,
                m_deepVocoderSendStates[i]));
            Put(6 + (i % 2), 4 + (i / 2), new SourceWidthCell(this, i, m_sourceWidthStates[i]));
        }

        SetColors(SmartGrid::Color::White, SmartGrid::Color::Grey.Dim());
        m_squiggleBoy->UpdateEncodersForMachine();
    }

    void RevertToDefault()
    {
        for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
        {
            for (size_t j = 0; j < SourceMixer::x_numSources; ++j)
            {
                m_sourceSelectedStates[i][j]->Set(j == 0);
            }
        }

        if (m_squiggleBoy)
        {
            for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
            {
                m_sourceWidthStates[i]->Set(SourceMixer::SourceWidth::Mono);
                m_sourceMonitorStates[i]->Set(true);
            }
        }

        if (m_squiggleBoy)
        {
            for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
            {
                AudioBufferBank* bank = m_squiggleBoy->m_state[i].m_voiceConfig.m_audioBufferBank;
                if (bank)
                {
                    bank->m_directoryName.clear();
                }
            }
        }

        m_activeSampleDirectorySlot = SampleDirectorySlot::None;
        m_activeSampleDirectoryVoice = x_noActiveSampleDirectoryVoice;

        PropagateSourceSelection();
    }

    JSON ToJSON(JsonArena& a)
    {
        JSON rootJ = a.Object();

        if (!m_squiggleBoy)
        {
            return rootJ;
        }

        JSON dirsJ = a.Array();

        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            AudioBufferBank* bank = m_squiggleBoy->m_state[i].m_voiceConfig.m_audioBufferBank;
            const char* rel = (bank && !bank->m_directoryName.empty()) ? bank->m_directoryName.c_str() : "";
            dirsJ.AppendNew(a.String(rel));
        }

        rootJ.SetNew("sampleDirectoryRelative", dirsJ);

        JSON sourceWidthJ = a.Array();
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            bool stereo = m_squiggleBoy->m_sourceMixerState.m_sources[i].m_config.IsStereo();
            sourceWidthJ.AppendNew(a.Boolean(stereo));
        }

        rootJ.SetNew("sourceStereo", sourceWidthJ);

        JSON sourceSelectedJ = a.Array();
        for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
        {
            JSON trioJ = a.Array();
            for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
            {
                trioJ.AppendNew(a.Boolean(m_sourceSelected[trio][source]));
            }

            sourceSelectedJ.AppendNew(trioJ);
        }

        rootJ.SetNew("sourceSelected", sourceSelectedJ);
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON dirsJ = rootJ.Get("sampleDirectoryRelative");

        JSON sourceWidthJ = rootJ.Get("sourceStereo");
        if (!sourceWidthJ.IsNull() && m_squiggleBoy)
        {
            for (size_t i = 0; i < sourceWidthJ.Size() && i < SourceMixer::x_numSources; ++i)
            {
                SourceMixer::SourceWidth width = sourceWidthJ.GetAt(i).BooleanValue()
                    ? SourceMixer::SourceWidth::Stereo
                    : SourceMixer::SourceWidth::Mono;
                m_sourceWidthStates[i]->Set(width);
            }
        }

        JSON sourceSelectedJ = rootJ.Get("sourceSelected");
        if (!sourceSelectedJ.IsNull())
        {
            for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
            {
                for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
                {
                    m_sourceSelectedStates[trio][source]->Set(false);
                }
            }

            for (size_t trio = 0; trio < sourceSelectedJ.Size() && trio < TheNonagonInternal::x_numTrios; ++trio)
            {
                JSON trioJ = sourceSelectedJ.GetAt(trio);
                for (size_t source = 0; source < trioJ.Size() && source < SourceMixer::x_numSources; ++source)
                {
                    m_sourceSelectedStates[trio][source]->Set(trioJ.GetAt(source).BooleanValue());
                }

                EnforceSourceChannelLimit(trio, SourceMixer::x_numSources);
            }
        }
        else
        {
            for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
            {
                for (size_t source = 0; source < SourceMixer::x_numSources; ++source)
                {
                    m_sourceSelectedStates[trio][source]->Set(false);
                }
            }
        }

        // Older patches stored monitors here instead of in StateSaver.
        //
        JSON sourceMonitorJ = rootJ.Get("sourceMonitor");
        if (!sourceMonitorJ.IsNull())
        {
            for (size_t i = 0; i < sourceMonitorJ.Size() && i < SourceMixer::x_numSources; ++i)
            {
                m_sourceMonitorStates[i]->Set(sourceMonitorJ.GetAt(i).BooleanValue());
            }
        }

        if (dirsJ.IsNull() || !m_ioTaskThread || !m_squiggleBoy)
        {
            return;
        }

        for (size_t i = 0; i < dirsJ.Size() && i < SquiggleBoy::x_numVoices; ++i)
        {
            JSON entryJ = dirsJ.GetAt(i);
            const char* rel = entryJ.StringValue();

            AudioBufferBank*& bankRef = m_squiggleBoy->m_state[i].m_voiceConfig.m_audioBufferBank;

            if (!rel || rel[0] == '\0')
            {
                if (bankRef)
                {
                    delete bankRef;
                    bankRef = nullptr;
                }

                continue;
            }

            m_ioTaskThread->PushLoadAudioBufferBankFromDirectory(
                rel,
                &bankRef);
        }
    }

    bool RequestDirectoryExplorerInit(SampleDirectorySlot slot, size_t voiceIndex)
    {
        if (slot == SampleDirectorySlot::None)
        {
            return false;
        }

        if (TheNonagonInternal::x_voicesPerTrio <= voiceIndex)
        {
            return false;
        }

        size_t baseIdx = static_cast<size_t>(ActiveTrio()) * TheNonagonInternal::x_voicesPerTrio;
        size_t voiceIdx = baseIdx + voiceIndex;
        AudioBufferBank* bank = m_squiggleBoy->m_state[voiceIdx].m_voiceConfig.m_audioBufferBank;

        const char* relativeForInit = (bank && !bank->m_directoryName.empty()) ? bank->m_directoryName.c_str() : "";

        const bool pushed = m_ioTaskThread->PushInitializeDirectoryExplorer(
            relativeForInit,
            &m_directoryExplorer);

        if (pushed)
        {
            m_activeSampleDirectorySlot = slot;
            m_activeSampleDirectoryVoice = voiceIndex;
        }

        return pushed;
    }

    SquiggleBoyWithEncoderBank* m_squiggleBoy;
    TheNonagonSmartGrid::Trio* m_activeTrio;
    IoTaskThread* m_ioTaskThread;

    DirectoryExplorer m_directoryExplorer;

    static constexpr size_t x_noActiveSampleDirectoryVoice = TheNonagonInternal::x_voicesPerTrio;

    SampleDirectorySlot m_activeSampleDirectorySlot{SampleDirectorySlot::None};
    size_t m_activeSampleDirectoryVoice{x_noActiveSampleDirectoryVoice};

    bool m_sourceSelected[TheNonagonInternal::x_numTrios][SourceMixer::x_numSources];
    bool m_sourceMonitor[SourceMixer::x_numSources];
    State* m_voiceSourceMachineStates[SquiggleBoy::x_numVoices];
    State* m_voiceFilterMachineStates[SquiggleBoy::x_numVoices];
    State* m_deepVocoderSendStates[SourceMixer::x_numSources];
    State* m_sourceWidthStates[SourceMixer::x_numSources];
    State* m_sourceMonitorStates[SourceMixer::x_numSources];
    State* m_sourceSelectedStates[TheNonagonInternal::x_numTrios][SourceMixer::x_numSources];
};
