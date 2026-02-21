#pragma once

#include "EncoderBank.hpp"
#include "SampleTimer.hpp"

struct EncoderBankBank
{
    size_t m_numBanks;
    size_t m_numModes;

    struct BankMode
    {
        size_t m_numTracks;
        size_t m_numVoices;
        SmartGrid::BankedEncoderCell::ModulatorValues m_modulatorValues;
        
        BankMode()
            : m_numTracks(0)
            , m_numVoices(0)
            , m_modulatorValues()
        {
        }
    };

    struct BankConfig
    {
        size_t m_modeIx;
        SmartGrid::Color m_color;

        BankConfig()
            : m_modeIx(0)
            , m_color(SmartGrid::Color::Off)
        {
        }
    };

    SmartGrid::EncoderBankInternal* m_banks;
    SmartGrid::SceneManager* m_sceneManager;
    int m_selectedBank;
    BankConfig* m_bankConfigs;
    BankMode* m_bankModes;

    EncoderBankBank(size_t numBanks, size_t numModes)
        : m_numBanks(numBanks)
        , m_numModes(numModes)
        , m_banks(new SmartGrid::EncoderBankInternal[numBanks])
        , m_sceneManager(nullptr)
        , m_selectedBank(-1)
        , m_bankConfigs(new BankConfig[numBanks])
        , m_bankModes(new BankMode[numModes])
    {
    }

    ~EncoderBankBank()
    {
        delete[] m_banks;
        delete[] m_bankConfigs;
        delete[] m_bankModes;
    }

    void InitSceneManager(SmartGrid::SceneManager* sceneManager)
    {
        m_sceneManager = sceneManager;
    }

    void InitMode(
        size_t modeIx,
        size_t numTracks,
        size_t numVoices)
    {
        m_bankModes[modeIx].m_numTracks = numTracks;
        m_bankModes[modeIx].m_numVoices = numVoices;
   }

    void InitBank(size_t bankIx, size_t modeIx, SmartGrid::Color color)
    {
        m_bankConfigs[bankIx].m_modeIx = modeIx;
        m_bankConfigs[bankIx].m_color = color;
        m_banks[bankIx].Init(
            m_sceneManager, 
            &m_bankModes[modeIx].m_modulatorValues, 
            m_bankModes[modeIx].m_numTracks, 
            m_bankModes[modeIx].m_numVoices);
   }

    void SelectGesture(const BitSet16& gesture)
    {
        for (size_t i = 0; i < m_numModes; ++i)
        {
            m_bankModes[i].m_modulatorValues.m_selectedGestures = gesture;
        }
    }

    void SetTrack(size_t modeIx, size_t track)
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            if (m_bankConfigs[i].m_modeIx == modeIx)
            {
                m_banks[i].SetTrack(track);
            }
        }
    }

    void Process()
    {
        if (SampleTimer::IsControlFrame())
        {
            for (size_t i = 0; i < m_numModes; ++i)
            {
                m_bankModes[i].m_modulatorValues.ComputeChanged();
            }
        }

        if (m_sceneManager->m_changed)
        {
            HandleChangedSceneManager();
        }

        if (m_sceneManager->m_changedScene)
        {
            HandleChangedSceneManagerScene();
        }

        for (size_t i = 0; i < m_numBanks; ++i)
        {
            if (SampleTimer::IsControlFrame())
            {
                m_banks[i].ProcessTopology();
            }

            m_banks[i].ProcessBulkFilter();
        }
    }

    void HandleChangedSceneManager()
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].HandleChangedSceneManager();
        }
    }

    void HandleChangedSceneManagerScene()
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].HandleChangedSceneManagerScene();
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

    void SelectGrid(int ix)
    {
        if (m_selectedBank >= 0)
        {
            m_banks[m_selectedBank].Deselect();
        }

        m_selectedBank = ix;
    }

    void ResetGrid(uint64_t ix, bool allScenes, bool allTracks)
    {
        m_banks[ix].RevertToDefault(allScenes, allTracks);
    }

    SmartGrid::Color GetSelectorColor(int ix)
    {
        SmartGrid::Color color = m_bankConfigs[ix].m_color;
        return m_selectedBank == ix ? color : color.Dim();
    }

    // Returns the union of gestures affecting all banks for the specified track
    //
    BitSet16 GetGesturesAffectingForTrack(size_t track)
    {
        BitSet16 result;
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            result = result.Union(m_banks[i].GetGesturesAffectingForTrack(track));
        }

        return result;
    }

    BitSet16 GetGesturesAffecting()
    {
        BitSet16 result;
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            result = result.Union(m_banks[i].GetGesturesAffecting());
        }

        return result;
    }

    SmartGrid::Color GetGestureColor(int gesture)
    {
        bool found = false;
        SmartGrid::Color result = SmartGrid::Color::Off;
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            if (m_banks[i].GetGesturesAffecting().Get(gesture))
            {
                if (found)
                {
                    return SmartGrid::Color::White;
                }

                result = m_bankConfigs[i].m_color;
                found = true;
            }
        }

        return result;
    }

    // Returns the gestures affecting the specified bank for the specified track
    //
    BitSet16 GetGesturesAffectingBankForTrack(size_t bank, size_t track)
    {
        return m_banks[bank].GetGesturesAffectingForTrack(track);
    }

    // Returns true if the specified gesture affects the specified bank for the specified track
    //
    bool IsGestureAffectingBank(int gesture, size_t bank, size_t track)
    {
        return m_banks[bank].GetGesturesAffectingForTrack(track).Get(gesture);
    }

    void ClearGesture(int gesture)
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].ClearGesture(gesture);
        }
    }

    void Config(size_t bank, size_t i, size_t j, float defaultValue, const char* name, const char* shortName, SmartGrid::Color color)
    {
        m_banks[bank].GetBase(i, j)->m_defaultValue = defaultValue;
        m_banks[bank].GetBase(i, j)->SetValueAllScenesAllTracks(defaultValue);
        m_banks[bank].GetBase(i, j)->m_connected = true;
        m_banks[bank].GetBase(i, j)->m_color = color;
        m_banks[bank].GetBase(i, j)->m_name = name;
        m_banks[bank].GetBase(i, j)->m_shortName = shortName;
    }

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].ToJSON(rootJ);
        }

        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].FromJSON(rootJ);
        }

        JSON increfRoot = rootJ.Incref();
    }

    void CopyToScene(int scene)
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].CopyToScene(scene);
        }
    }

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            m_banks[i].RevertToDefault(allScenes, allTracks);
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

    int GetCurrentTrack(size_t modeIx)
    {
        // Find first bank with this mode
        //
        for (size_t i = 0; i < m_numBanks; ++i)
        {
            if (m_bankConfigs[i].m_modeIx == modeIx)
            {
                return m_banks[i].GetCurrentTrack();
            }
        }

        return 0;
    }

    SmartGrid::BankedEncoderCell::ModulatorValues& GetModulatorValues(size_t modeIx)
    {
        return m_bankModes[modeIx].m_modulatorValues;
    }

    size_t GetModeForBank(size_t bankIx)
    {
        return m_bankConfigs[bankIx].m_modeIx;
    }
};
