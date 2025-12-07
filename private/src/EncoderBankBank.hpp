#pragma once

#include "EncoderBank.hpp"

template <size_t NumBanks>
struct EncoderBankBankInternal
{
    SmartGrid::EncoderBankInternal m_banks[NumBanks];
    int m_selectedBank;
    size_t m_selectedGridId;
    SmartGrid::Color m_color[NumBanks];

    static constexpr size_t x_controlFrameRate = 8;
    size_t m_frame;

    struct Input
    {
        SmartGrid::BankedEncoderCell::Input m_bankedEncoderCellInput[NumBanks][4][4];
        SmartGrid::EncoderBankInternal::Input m_bankedEncoderInternalInput;

        void SetInput(size_t ix)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 4; ++j)
                {
                    m_bankedEncoderInternalInput.m_cellInput[i][j] = m_bankedEncoderCellInput[ix][i][j];
                }
            }
        }

        void SetColor(size_t ix, SmartGrid::Color color)
        {
            uint8_t hue = color.ToTwister();
            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 4; ++j)
                {
                    m_bankedEncoderCellInput[ix][i][j].m_twisterColor = hue;
                    if (j < 2)
                    {
                        m_bankedEncoderCellInput[ix][i][j].m_color = color;
                    }
                    else
                    {
                        m_bankedEncoderCellInput[ix][i][j].m_color = color.Similar();
                    }
                }
            }
        }

        void SetConnected(size_t bank, size_t i, size_t j)
        {
            m_bankedEncoderCellInput[bank][i][j].m_connected = true;
        }

        void SelectGesture(int gesture)
        {
            m_bankedEncoderInternalInput.m_selectedGesture = gesture;
        }
    };

    EncoderBankBankInternal()
    {
        SelectGrid(0);
    }

    void Process(Input& input, float dt)
    {
        m_frame++;
        if (m_frame % x_controlFrameRate == 0)
        {
            input.m_bankedEncoderInternalInput.m_modulatorValues.ComputeChanged();
        }
        
        m_selectedGridId = m_selectedBank >= 0 ? m_banks[m_selectedBank].m_gridId : SmartGrid::x_numGridIds;

        for (size_t i = 0; i < NumBanks; ++i)
        {
            input.SetInput(i);
            m_banks[i].ProcessStatic(dt);
            if (m_frame % x_controlFrameRate == 0)
            {                
                m_banks[i].ProcessInput(input.m_bankedEncoderInternalInput);
            }

            m_banks[i].ProcessBulkFilter();
        }

    }

    float GetValue(size_t ix, size_t i, size_t j, size_t channel)
    {
        return m_banks[ix].GetValue(i, j, channel);
    }

    float GetValueNoSlew(size_t ix, size_t i, size_t j, size_t channel)
    {
        return m_banks[ix].GetValueNoSlew(i, j, channel);
    }

    uint64_t GetGridId(size_t ix)
    {
        return m_banks[ix].m_gridId;
    }

    uint64_t GetCurrentGridId()
    {
        return m_selectedGridId;
    }

    void SelectGrid(uint64_t ix)
    {
        if (m_selectedBank >= 0)
        {
            m_banks[m_selectedBank].Deselect();
        }

        m_selectedBank = ix;
        m_selectedGridId = m_selectedBank >= 0 ? m_banks[m_selectedBank].m_gridId : SmartGrid::x_numGridIds;
    }

    void ResetGrid(uint64_t ix)
    {
        m_banks[ix].RevertToDefault();
    }

    SmartGrid::Color GetSelectorColor(int ix)
    {
        return m_selectedBank == ix ? m_color[ix] : m_color[ix].Dim();
    }

    // Returns the union of gestures affecting all banks for the specified track
    //
    BitSet16 GetGesturesAffectingForTrack(size_t track)
    {
        BitSet16 result;
        for (size_t i = 0; i < NumBanks; ++i)
        {
            result = result.Union(m_banks[i].GetGesturesAffectingForTrack(track));
        }

        return result;
    }

    // Returns true if the specified gesture affects the specified bank for the specified track
    //
    bool IsGestureAffectingBank(int gesture, size_t bank, size_t track)
    {
        return m_banks[bank].GetGesturesAffectingForTrack(track).Get(gesture);
    }

    void ClearGesture(int gesture)
    {
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].ClearGesture(gesture);
        }
    }

    void SetColor(size_t ix, SmartGrid::Color color, Input& input)
    {
        m_color[ix] = color;
        input.SetColor(ix, color);
    }

    void Config(size_t bank, size_t i, size_t j, float defaultValue, std::string name, Input& input)
    {
        std::ignore = name;
        m_banks[bank].GetBase(i, j)->m_defaultValue = defaultValue;
        m_banks[bank].GetBase(i, j)->SetValueAllScenesAllTracks(defaultValue);
        input.SetConnected(bank, i, j);
    }

    void SetModulatorType(size_t index, SmartGrid::BankedEncoderCell::EncoderType type)
    {
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].SetModulatorType(index, type);
        }
    }

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        for (size_t i = 0; i < NumBanks; ++i)
        {
            std::string key = "Bank" + std::to_string(i);
            rootJ.SetNew(key.c_str(), m_banks[i].ToJSON());
        }
    
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        for (size_t i = 0; i < NumBanks; ++i)
        {
            std::string key = "Bank" + std::to_string(i);
            JSON bankJ = rootJ.Get(key.c_str());
            if (!bankJ.IsNull())
            {
                m_banks[i].FromJSON(bankJ);
            }
        }

        JSON increfRoot = rootJ.Incref();
    }

    void CopyToScene(int scene)
    {
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].CopyToScene(scene);
        }
    }

    void RevertToDefault()
    {
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].RevertToDefault();
        }
    }

    void Apply(SmartGrid::MessageIn msg)
    {
        if (m_selectedBank >= 0)
        {
            m_banks[m_selectedBank].Apply(msg);
        }
    }

    void PopulateUIState(EncoderBankUIState* uiState)
    {
        if (m_selectedBank >= 0)
        {
            m_banks[m_selectedBank].PopulateUIState(uiState);
        }
    }

    int GetCurrentTrack()
    {
        return m_banks[0].GetCurrentTrack();
    }
};
