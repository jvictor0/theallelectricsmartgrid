#pragma once

#include "EncoderBank.hpp"
#include "SampleTimer.hpp"

struct EncoderBankBank
{
    size_t m_numBanks;
    size_t m_numModes;
    size_t m_numEncoders;

    struct BankMode
    {
        size_t m_numTracks;
        size_t m_numVoices;
        SmartGrid::BankedEncoderCell::ModulatorValues m_modulatorValues;
        SmartGrid::BankedEncoderCell::SharedEncoderState m_sharedEncoderState;
        
        BankMode()
            : m_numTracks(0)
            , m_numVoices(0)
            , m_modulatorValues()
            , m_sharedEncoderState()
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
    SmartGrid::EncoderPtr* m_encoders;

    EncoderBankBank(size_t numBanks, size_t numModes, size_t numEncoders)
        : m_numBanks(numBanks)
        , m_numModes(numModes)
        , m_numEncoders(numEncoders)
        , m_banks(new SmartGrid::EncoderBankInternal[numBanks])
        , m_sceneManager(nullptr)
        , m_selectedBank(-1)
        , m_bankConfigs(new BankConfig[numBanks])
        , m_bankModes(new BankMode[numModes])
        , m_encoders(new SmartGrid::EncoderPtr[numEncoders])
    {
    }

    ~EncoderBankBank()
    {
        delete[] m_banks;
        delete[] m_bankConfigs;
        delete[] m_bankModes;
        delete[] m_encoders;
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
        m_bankModes[modeIx].m_sharedEncoderState.m_numTracks = numTracks;
        m_bankModes[modeIx].m_sharedEncoderState.m_numVoices = numVoices;
        m_bankModes[modeIx].m_sharedEncoderState.m_modulatorValues = &m_bankModes[modeIx].m_modulatorValues;
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

    size_t CreateEncoder(
        SmartGrid::SceneManager* sceneManager,
        size_t index,
        size_t modeIx,
        float defaultValue,
        const char* name,
        const char* shortName,
        SmartGrid::Color color,
        int switchValues)
    {
        if (index >= m_numEncoders)
        {
            return index;
        }

        m_encoders[index] = SmartGrid::MakeEncoder(
            sceneManager,
            nullptr,
            static_cast<int>(index),
            SmartGrid::BankedEncoderCell::EncoderType::BaseParam);
        SmartGrid::BankedEncoderCell* cell = m_encoders[index].get();
        cell->m_sharedEncoderState = &m_bankModes[modeIx].m_sharedEncoderState;
        cell->m_numTracks = m_bankModes[modeIx].m_numTracks;
        cell->m_defaultValue = defaultValue;
        cell->SetValueAllScenesAllTracks(defaultValue);
        cell->InitSlewState(defaultValue);
        cell->m_connected = true;
        cell->m_color = color;
        cell->m_name = name;
        cell->m_shortName = shortName;
        cell->m_switchValues = switchValues;
        return index;
    }

    SmartGrid::BankedEncoderCell* GetEncoder(size_t index)
    {
        if (index >= m_numEncoders)
        {
            return nullptr;
        }

        return m_encoders[index].get();
    }

    float GetValueByEncoderIndex(size_t encoderIndex, size_t channel)
    {
        SmartGrid::BankedEncoderCell* cell = GetEncoder(encoderIndex);
        if (cell)
        {
            return cell->GetSlewedValue(channel);
        }

        return 0.0f;
    }

    float GetValueNoSlewByEncoderIndex(size_t encoderIndex, size_t channel)
    {
        SmartGrid::BankedEncoderCell* cell = GetEncoder(encoderIndex);
        if (cell)
        {
            return cell->m_output[channel];
        }

        return 0.0f;
    }

    void PlaceEncoder(size_t encoderIndex, size_t bank, int x, int y)
    {
        SmartGrid::BankedEncoderCell* encoder = GetEncoder(encoderIndex);
        m_banks[bank].PlaceEncoder(x, y, encoder);
    }

    void NullBank(size_t bankIx)
    {
        for (int x = 0; x < 4; ++x)
        {
            for (int y = 0; y < 4; ++y)
            {
                m_banks[bankIx].PlaceEncoder(x, y, nullptr);
            }
        }
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
        m_bankModes[modeIx].m_sharedEncoderState.m_currentTrack = track;

        for (size_t i = 0; i < m_numBanks; ++i)
        {
            if (m_bankConfigs[i].m_modeIx == modeIx)
            {
                m_banks[i].SetTrack(track);
            }
        }
    }

    bool EncoderBelongsToMode(size_t encoderIx, size_t modeIx)
    {
        SmartGrid::BankedEncoderCell* cell = GetEncoder(encoderIx);
        return cell && cell->m_sharedEncoderState == &m_bankModes[modeIx].m_sharedEncoderState;
    }

    template <typename Func>
    void ForEachNamedEncoder(Func func)
    {
        for (size_t i = 0; i < m_numEncoders; ++i)
        {
            SmartGrid::BankedEncoderCell* cell = m_encoders[i].get();
            if (cell && cell->m_name)
            {
                func(i, cell);
            }
        }
    }

    template <typename Func>
    void ForEachNamedEncoderInMode(size_t modeIx, Func func)
    {
        ForEachNamedEncoder(
            [this, modeIx, func](size_t encoderIx, SmartGrid::BankedEncoderCell* cell)
            {
                if (EncoderBelongsToMode(encoderIx, modeIx))
                {
                    func(encoderIx, cell);
                }
            });
    }

    void SetAllModulatorsAffectingForMode(size_t modeIx)
    {
        ForEachNamedEncoderInMode(
            modeIx,
            [](size_t, SmartGrid::BankedEncoderCell* cell)
            {
                cell->SetModulatorsAffecting();
            });

        for (size_t i = 0; i < m_numBanks; ++i)
        {
            if (m_bankConfigs[i].m_modeIx == modeIx)
            {
                m_banks[i].ComputeGesturesAffectingPerTrack();
            }
        }
    }

    void SetAllModulatorsAffectingForAllModes()
    {
        for (size_t modeIx = 0; modeIx < m_numModes; ++modeIx)
        {
            SetAllModulatorsAffectingForMode(modeIx);
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

        if (SampleTimer::IsControlFrame())
        {
            ForEachNamedEncoder(
                [](size_t, SmartGrid::BankedEncoderCell* cell)
                {
                    cell->Compute();
                });

            for (size_t i = 0; i < m_numBanks; ++i)
            {
                m_banks[i].ProcessTopology();
            }
        }
    }

    void HandleChangedSceneManager()
    {
        ForEachNamedEncoder(
            [](size_t, SmartGrid::BankedEncoderCell* cell)
            {
                cell->SetStateRecursive();
            });
    }

    void HandleChangedSceneManagerScene()
    {
        SetAllModulatorsAffectingForAllModes();
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

    void ResetGrid(uint64_t ix)
    {
        m_banks[ix].RevertToDefault(false, false);
        m_banks[ix].SetAllModulatorsAffecting();
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
        ForEachNamedEncoder(
            [gesture](size_t, SmartGrid::BankedEncoderCell* cell)
            {
                cell->ClearGesture(gesture);
                cell->SetForceUpdateRecursive();
            });

        SetAllModulatorsAffectingForAllModes();
    }

    JSON ToJSON(JsonArena& a)
    {
        JSON rootJ = a.Object();
        for (size_t i = 0; i < m_numEncoders; ++i)
        {
            SmartGrid::BankedEncoderCell* cell = m_encoders[i].get();
            if (cell && cell->m_name)
            {
                rootJ.SetNew(cell->m_name, cell->ToJSON(a));
            }
        }

        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        for (size_t i = 0; i < m_numEncoders; ++i)
        {
            SmartGrid::BankedEncoderCell* cell = m_encoders[i].get();
            if (cell && cell->m_name)
            {
                JSON paramJ = rootJ.Get(cell->m_name);
                if (!paramJ.IsNull())
                {
                    cell->FromJSON(paramJ);
                }
            }
        }

    }

    void CopyToScene(int scene)
    {
        ForEachNamedEncoder(
            [scene](size_t, SmartGrid::BankedEncoderCell* cell)
            {
                cell->CopyToScene(scene);
            });
    }

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        ForEachNamedEncoder(
            [allScenes, allTracks](size_t, SmartGrid::BankedEncoderCell* cell)
            {
                cell->RevertToDefault(allScenes, allTracks);
            });

        SetAllModulatorsAffectingForAllModes();
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
