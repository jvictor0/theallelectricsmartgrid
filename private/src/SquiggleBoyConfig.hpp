#pragma once

#include <cstring>
#include <filesystem>
#include <string>

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
        using VoiceConfig = SquiggleBoyVoice::VoiceConfig;

        EnumT (VoiceConfig::* m_memberPtr);
        SquiggleBoyConfigGrid* m_owner;

        EnumIncDecCell(SquiggleBoyConfigGrid* owner, EnumT (VoiceConfig::* memberPtr))
            : m_memberPtr(memberPtr)
            , m_owner(owner)
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
            auto& config = m_owner->m_squiggleBoy->m_state[baseIdx].m_voiceConfig;
            int current = static_cast<int>(config.*m_memberPtr);
            int next = Dec ? (current - 1 + NumValues) % NumValues : (current + 1) % NumValues;
            EnumT value = static_cast<EnumT>(next);

            for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
            {
                m_owner->m_squiggleBoy->m_state[baseIdx + i].m_voiceConfig.*m_memberPtr = value;
            }

            m_owner->m_squiggleBoy->UpdateEncodersForMachine();
        }
    };

    struct SourceSelectCell : SmartGrid::Cell
    {
        int m_sourceIndex;
        SquiggleBoyConfigGrid* m_owner;

        SourceSelectCell(SquiggleBoyConfigGrid* owner, int sourceIndex)
            : m_sourceIndex(sourceIndex)
            , m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            size_t trioIdx = static_cast<size_t>(m_owner->ActiveTrio());
            return m_owner->m_sourceSelected[trioIdx][m_sourceIndex] ? SmartGrid::Color::White : SmartGrid::Color::Grey.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            size_t trioIdx = static_cast<size_t>(m_owner->ActiveTrio());
            m_owner->m_sourceSelected[trioIdx][m_sourceIndex] = !m_owner->m_sourceSelected[trioIdx][m_sourceIndex];

            int numSelected = 0;
            for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
            {
                if (m_owner->m_sourceSelected[trioIdx][i])
                {
                    ++numSelected;
                }
            }

            if (TheNonagonInternal::x_voicesPerTrio < numSelected)
            {
                m_owner->m_sourceSelected[trioIdx][m_sourceIndex == 0 ? 1 : 0] = false;
                --numSelected;
            }
            else if (numSelected == 0)
            {
                m_owner->m_sourceSelected[trioIdx][m_sourceIndex] = true;
                ++numSelected;
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
            int numSelected = 0;
            for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
            {
                if (m_sourceSelected[tIdx][i])
                {
                    ++numSelected;
                }
            }

            for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
            {
                int sourceIndex = i % numSelected;
                for (size_t j = 0; j < SourceMixer::x_numSources; ++j)
                {
                    if (m_sourceSelected[tIdx][j])
                    {
                        if (sourceIndex == 0)
                        {
                            m_squiggleBoy->m_state[tIdx * TheNonagonInternal::x_voicesPerTrio + i].m_voiceConfig.m_sourceIndex = j;
                            break;
                        }
            
                        --sourceIndex;
                    }
                }
            }
        }
    }

    SquiggleBoyConfigGrid()
        : m_squiggleBoy(nullptr)
        , m_activeTrio(nullptr)
        , m_ioTaskThread(nullptr)
    {
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

        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            squiggleBoyWithEncoders->m_stateSaver->Insert("voiceSourceMachine", i, &squiggleBoyWithEncoders->m_state[i].m_voiceConfig.m_sourceMachine);
            squiggleBoyWithEncoders->m_stateSaver->Insert("voiceFilterMachine", i, &squiggleBoyWithEncoders->m_state[i].m_voiceConfig.m_filterMachine);
        }

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            squiggleBoyWithEncoders->m_stateSaver->Insert("monitor", i, &squiggleBoyWithEncoders->m_mixerState.m_monitor[TheNonagonInternal::x_numVoices + i]);
            squiggleBoyWithEncoders->m_stateSaver->Insert("deepVocoderSend", i, &squiggleBoyWithEncoders->m_sourceMixerState.m_deepVocoderSend[i]);

            for (size_t j = 0; j < TheNonagonInternal::x_numTrios; ++j)
            {
                m_squiggleBoy->m_stateSaver->Insert("sourceSelected", j, i, &m_sourceSelected[j][i]);
            }
        }

        for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
        {
            for (size_t j = 0; j < SourceMixer::x_numSources; ++j)
            {
                m_sourceSelected[i][j] = j == 0;
            }
        }

        Put(0, 0, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::SourceMachine, true, x_numSourceMachines>(
            this, &SquiggleBoyVoice::VoiceConfig::m_sourceMachine));
        Put(0, 1, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::SourceMachine, false, x_numSourceMachines>(
            this, &SquiggleBoyVoice::VoiceConfig::m_sourceMachine));

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            Put(0, 2 + i, new SourceSelectCell(this, i));
        }

        Put(0, 6, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::FilterMachine, true, x_numFilterMachines>(
            this, &SquiggleBoyVoice::VoiceConfig::m_filterMachine));
        Put(0, 7, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::FilterMachine, false, x_numFilterMachines>(
            this, &SquiggleBoyVoice::VoiceConfig::m_filterMachine));

        Put(1, 0, new SampleDirectoryInitCell(this, SampleDirectorySlot::One, 0));
        Put(2, 0, new SampleDirectoryInitCell(this, SampleDirectorySlot::One, 1));
        Put(3, 0, new SampleDirectoryInitCell(this, SampleDirectorySlot::One, 2));

        Put(6, 6, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Up, SmartGrid::Color::Cyan));
        Put(5, 6, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::No, SmartGrid::Color::Red));
        Put(7, 6, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Yes, SmartGrid::Color::Green));
        Put(5, 7, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Left, SmartGrid::Color::Cyan));
        Put(6, 7, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Down, SmartGrid::Color::Cyan));
        Put(7, 7, new DirectoryExplorerNavCell(this, DirectoryExplorer::MessageType::Right, SmartGrid::Color::Cyan));

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            Put(6 + (i % 2), i / 2, new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Grey.Dim(),
                SmartGrid::Color::White,
                &m_squiggleBoy->m_mixerState.m_monitor[TheNonagonInternal::x_numVoices + i],
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Toggle));

            Put(6 + (i % 2), 2 + (i / 2), new SmartGrid::StateCell<bool>(
                SmartGrid::Color::Grey.Dim(),
                SmartGrid::Color::White,
                &m_squiggleBoy->m_sourceMixerState.m_deepVocoderSend[i],
                true,
                false,
                SmartGrid::StateCell<bool>::Mode::Toggle));
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
                m_sourceSelected[i][j] = (j == 0);
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

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();

        if (!m_squiggleBoy)
        {
            return rootJ;
        }

        JSON dirsJ = JSON::Array();

        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            AudioBufferBank* bank = m_squiggleBoy->m_state[i].m_voiceConfig.m_audioBufferBank;
            const char* rel = (bank && !bank->m_directoryName.empty()) ? bank->m_directoryName.c_str() : "";
            dirsJ.AppendNew(JSON::String(rel));
        }

        rootJ.SetNew("sampleDirectoryRelative", dirsJ);
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON dirsJ = rootJ.Get("sampleDirectoryRelative");

        if (dirsJ.IsNull())
        {
            return;
        }

        if (!m_ioTaskThread || !m_squiggleBoy)
        {
            return;
        }

        const bool haveRoot = !m_sampleDirectoryRootAbsolute.empty();

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

            if (!haveRoot)
            {
                continue;
            }

            std::filesystem::path absolutePath = m_sampleDirectoryRootAbsolute;
            absolutePath /= std::filesystem::path(rel);
            std::string absoluteString = absolutePath.string();

            m_ioTaskThread->PushLoadAudioBufferBankFromDirectory(
                absoluteString.c_str(),
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

        if (!m_ioTaskThread)
        {
            return false;
        }

        if (!m_squiggleBoy)
        {
            return false;
        }

        if (m_sampleDirectoryRootAbsolute.empty())
        {
            return false;
        }

        size_t baseIdx = static_cast<size_t>(ActiveTrio()) * TheNonagonInternal::x_voicesPerTrio;
        size_t voiceIdx = baseIdx + voiceIndex;
        AudioBufferBank* bank = m_squiggleBoy->m_state[voiceIdx].m_voiceConfig.m_audioBufferBank;

        const char* relativeForInit = (bank && !bank->m_directoryName.empty()) ? bank->m_directoryName.c_str() : "";

        std::string absoluteString = m_sampleDirectoryRootAbsolute.string();

        const bool pushed = m_ioTaskThread->PushInitializeDirectoryExplorer(
            absoluteString.c_str(),
            relativeForInit,
            &m_directoryExplorer);

        if (pushed)
        {
            m_activeSampleDirectorySlot = slot;
            m_activeSampleDirectoryVoice = voiceIndex;
        }

        return pushed;
    }

    void SetSampleDirectoryRootAbsolute(const std::filesystem::path& absolutePath)
    {
        m_sampleDirectoryRootAbsolute = absolutePath;
    }

    SquiggleBoyWithEncoderBank* m_squiggleBoy;
    TheNonagonSmartGrid::Trio* m_activeTrio;
    IoTaskThread* m_ioTaskThread;
    std::filesystem::path m_sampleDirectoryRootAbsolute;

    DirectoryExplorer m_directoryExplorer;

    static constexpr size_t x_noActiveSampleDirectoryVoice = TheNonagonInternal::x_voicesPerTrio;

    SampleDirectorySlot m_activeSampleDirectorySlot{SampleDirectorySlot::None};
    size_t m_activeSampleDirectoryVoice{x_noActiveSampleDirectoryVoice};

    bool m_sourceSelected[TheNonagonInternal::x_numTrios][SourceMixer::x_numSources];
};