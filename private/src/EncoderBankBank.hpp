#pragma once

#include "EncoderBank.hpp"
#include "SampleTimer.hpp"

template <size_t NumBanks>
struct EncoderBankBankInternal
{
    SmartGrid::EncoderBankInternal m_banks[NumBanks];
    SmartGrid::SceneManager* m_sceneManager;
    int m_selectedBank;
    SmartGrid::BankedEncoderCell::ModulatorValues m_modulatorValues;
    size_t m_numTracks;
    size_t m_numVoices;
    SmartGrid::Color m_color[NumBanks];

    EncoderBankBankInternal()
        : m_sceneManager(nullptr)
        , m_selectedBank(-1)
        , m_modulatorValues()
        , m_numTracks(1)
        , m_numVoices(1)
    {
    }

    void Init(
        SmartGrid::SceneManager* sceneManager,
        size_t numTracks,
        size_t numVoices)
    {
        m_sceneManager = sceneManager;
        m_numTracks = numTracks;
        m_numVoices = numVoices;

        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].Init(sceneManager, &m_modulatorValues, numTracks, numVoices);
        }

        SelectGrid(0);
    }

    void SelectGesture(const BitSet16& gesture)
    {
        m_modulatorValues.m_selectedGestures = gesture;
    }

    void SetTrack(size_t track)
    {
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].SetTrack(track);
        }
    }

    void Process()
    {
        if (SampleTimer::IsControlFrame())
        {
            m_modulatorValues.ComputeChanged();
        }

        if (m_sceneManager->m_changed)
        {
            HandleChangedSceneManager();
        }

        if (m_sceneManager->m_changedScene)
        {
            HandleChangedSceneManagerScene();
        }

        for (size_t i = 0; i < NumBanks; ++i)
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
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].HandleChangedSceneManager();
        }
    }

    void HandleChangedSceneManagerScene()
    {
        for (size_t i = 0; i < NumBanks; ++i)
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

    BitSet16 GetGesturesAffecting()
    {
        BitSet16 result;
        for (size_t i = 0; i < NumBanks; ++i)
        {
            result = result.Union(m_banks[i].GetGesturesAffecting());
        }

        return result;
    }

    SmartGrid::Color GetGestureColor(int gesture)
    {
        bool found = false;
        SmartGrid::Color result = SmartGrid::Color::Off;
        for (size_t i = 0; i < NumBanks; ++i)
        {
            if (m_banks[i].GetGesturesAffecting().Get(gesture))
            {
                if (found)
                {
                    return SmartGrid::Color::White;
                }

                result = m_color[i];
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
        for (size_t i = 0; i < NumBanks; ++i)
        {
            m_banks[i].ClearGesture(gesture);
        }
    }

    void SetColor(size_t ix, SmartGrid::Color color)
    {
        m_color[ix] = color;
    }

    void Config(size_t bank, size_t i, size_t j, float defaultValue, std::string name, SmartGrid::Color color)
    {
        std::ignore = name;
        m_banks[bank].GetBase(i, j)->m_defaultValue = defaultValue;
        m_banks[bank].GetBase(i, j)->SetValueAllScenesAllTracks(defaultValue);
        m_banks[bank].GetBase(i, j)->m_connected = true;
        m_banks[bank].GetBase(i, j)->m_color = color;
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

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        for (size_t i = 0; i < NumBanks; ++i)
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

    int GetCurrentTrack()
    {
        return m_banks[0].GetCurrentTrack();
    }
};
