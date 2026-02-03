#pragma once

#include "SquiggleBoy.hpp"
#include "TheNonagon.hpp"

struct SquiggleBoyConfigGrid : public SmartGrid::Grid
{
    struct MachineSelectCell : SmartGrid::Cell
    {
        SquiggleBoyVoice::VoiceConfig::Machine m_machine;
        TheNonagonSmartGrid::Trio m_trio;
        SquiggleBoyConfigGrid* m_owner;

        MachineSelectCell(SquiggleBoyConfigGrid* owner, TheNonagonSmartGrid::Trio trio, SquiggleBoyVoice::VoiceConfig::Machine machine)
            : m_machine(machine)
            , m_trio(trio)
            , m_owner(owner)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            bool isSelected = m_owner->m_squiggleBoy->m_state[static_cast<size_t>(m_trio) * TheNonagonInternal::x_voicesPerTrio].m_voiceConfig.m_machine == m_machine;
            return isSelected ? TheNonagonSmartGrid::TrioColor(m_trio) : TheNonagonSmartGrid::TrioColor(m_trio).Dim();
        }
        
        virtual void OnPress(uint8_t ) override
        {
            for (size_t i = 0; i < TheNonagonInternal::x_voicesPerTrio; ++i)
            {
                m_owner->m_squiggleBoy->m_state[static_cast<size_t>(m_trio) * TheNonagonInternal::x_voicesPerTrio + i].m_voiceConfig.m_machine = m_machine;
            }
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

    void Init(SquiggleBoy* squiggleBoy)
    {
        m_squiggleBoy = squiggleBoy;

        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {   
            squiggleBoy->m_stateSaver->Insert("voiceMachine", i, &squiggleBoy->m_state[i].m_voiceConfig.m_machine);
        }

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            squiggleBoy->m_stateSaver->Insert("monitor", i, &squiggleBoy->m_mixerState.m_monitor[TheNonagonInternal::x_numVoices + i]);
            squiggleBoy->m_stateSaver->Insert("deepVocoderSend", i, &squiggleBoy->m_sourceMixerState.m_deepVocoderSend[i]);

            for (size_t j = 0; j < TheNonagonInternal::x_numTrios; ++j)
            {
                squiggleBoy->m_stateSaver->Insert("sourceSelected", j, i, &m_sourceSelected[j][i]);
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
            Put(i, 0, new MachineSelectCell(this, static_cast<TheNonagonSmartGrid::Trio>(i), SquiggleBoyVoice::VoiceConfig::Machine::VCO));
            Put(i, 1, new MachineSelectCell(this, static_cast<TheNonagonSmartGrid::Trio>(i), SquiggleBoyVoice::VoiceConfig::Machine::Thru));
            for (size_t j = 0; j < SourceMixer::x_numSources; ++j)
            {
                Put(i, 2 + j, new SourceSelectCell(this, static_cast<TheNonagonSmartGrid::Trio>(i), j));
            }
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

    SquiggleBoy* m_squiggleBoy;
    bool m_sourceSelected[TheNonagonInternal::x_numTrios][SourceMixer::x_numSources];
};