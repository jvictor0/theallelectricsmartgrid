#pragma once

#include "SquiggleBoy.hpp"
#include "TheNonagon.hpp"
#include "VoiceMachineEnums.hpp"

struct SquiggleBoyConfigGrid : public SmartGrid::Grid
{
    static constexpr int x_numSourceMachines = static_cast<int>(VoiceMachine::SourceMachine::NumSourceMachines);
    static constexpr int x_numFilterMachines = static_cast<int>(VoiceMachine::FilterMachine::NumFilterMachines);

    template<typename EnumT, bool Dec, int NumValues>
    struct EnumIncDecCell : SmartGrid::Cell
    {
        using VoiceConfig = SquiggleBoyVoice::VoiceConfig;

        EnumT (VoiceConfig::* m_memberPtr);
        TheNonagonSmartGrid::Trio m_trio;
        SquiggleBoyConfigGrid* m_owner;

        EnumIncDecCell(SquiggleBoyConfigGrid* owner, TheNonagonSmartGrid::Trio trio, EnumT (VoiceConfig::* memberPtr))
            : m_memberPtr(memberPtr)
            , m_trio(trio)
            , m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return IsPressed() ? TheNonagonSmartGrid::TrioColor(m_trio) : TheNonagonSmartGrid::TrioColor(m_trio).Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            size_t baseIdx = static_cast<size_t>(m_trio) * TheNonagonInternal::x_voicesPerTrio;
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
        TheNonagonSmartGrid::Trio m_trio;
        SquiggleBoyConfigGrid* m_owner;

        SourceSelectCell(SquiggleBoyConfigGrid* owner, TheNonagonSmartGrid::Trio trio, int sourceIndex)
            : m_sourceIndex(sourceIndex)
            , m_trio(trio)
            , m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->m_sourceSelected[static_cast<size_t>(m_trio)][m_sourceIndex] ? SmartGrid::Color::White : SmartGrid::Color::Grey.Dim();
        }

        virtual void OnPress(uint8_t) override
        {
            m_owner->m_sourceSelected[static_cast<size_t>(m_trio)][m_sourceIndex] = !m_owner->m_sourceSelected[static_cast<size_t>(m_trio)][m_sourceIndex];

            int numSelected = 0;
            for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
            {
                if (m_owner->m_sourceSelected[static_cast<size_t>(m_trio)][i])
                {
                    ++numSelected;
                }
            }

            if (TheNonagonInternal::x_voicesPerTrio < numSelected)
            {
                m_owner->m_sourceSelected[static_cast<size_t>(m_trio)][m_sourceIndex == 0 ? 1 : 0] = false;
                --numSelected;
            }
            else if (numSelected == 0)
            {
                m_owner->m_sourceSelected[static_cast<size_t>(m_trio)][m_sourceIndex] = true;
                ++numSelected;
            }

            m_owner->PropagateSourceSelection();
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
    {
    }

    void Init(SquiggleBoyWithEncoderBank* squiggleBoyWithEncoders)
    {
        m_squiggleBoy = squiggleBoyWithEncoders;

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

        for (size_t i = 0; i < TheNonagonInternal::x_numTrios; ++i)
        {
            auto trio = static_cast<TheNonagonSmartGrid::Trio>(i);

            // Source machine: upper = decrement, lower = increment
            //
            Put(i, 0, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::SourceMachine, true, x_numSourceMachines>(
                this, trio, &SquiggleBoyVoice::VoiceConfig::m_sourceMachine));
            Put(i, 1, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::SourceMachine, false, x_numSourceMachines>(
                this, trio, &SquiggleBoyVoice::VoiceConfig::m_sourceMachine));

            // Source selection for Thru mode
            //
            for (size_t j = 0; j < SourceMixer::x_numSources; ++j)
            {
                Put(i, 2 + j, new SourceSelectCell(this, trio, j));
            }

            // Filter machine: upper = decrement, lower = increment
            //
            Put(i, 6, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::FilterMachine, true, x_numFilterMachines>(
                this, trio, &SquiggleBoyVoice::VoiceConfig::m_filterMachine));
            Put(i, 7, new EnumIncDecCell<SquiggleBoyVoice::VoiceConfig::FilterMachine, false, x_numFilterMachines>(
                this, trio, &SquiggleBoyVoice::VoiceConfig::m_filterMachine));
        }

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

        PropagateSourceSelection();
    }

    SquiggleBoyWithEncoderBank* m_squiggleBoy;
    bool m_sourceSelected[TheNonagonInternal::x_numTrios][SourceMixer::x_numSources];
};