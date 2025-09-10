#pragma once

#include "Encoder.hpp"
#include "ModuleUtils.hpp"
#include "BitSet.hpp"
#include "Filter.hpp"
#include "MessageIn.hpp"
#include "EncoderUIState.hpp"

namespace SmartGrid
{

struct BankedEncoderCell : public StateEncoderCell
{
    enum class EncoderType
    {
        BaseParam,
        ModulatorAmount,
        GestureParam
    };

    static constexpr size_t x_numModulators = 15;
    struct ModulatorValues
    {
        ModulatorValues()
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    m_value[i][j] = 0;
                    m_valuePrev[i][j] = 0;
                }
            }
        }

        void ComputeChanged()
        {
            m_changed.Clear();
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (memcmp(m_value[i], m_valuePrev[i], 16 * sizeof(float)) != 0)
                {
                    m_changed.Set(i, true);
                    memcpy(m_valuePrev[i], m_value[i], 16 * sizeof(float));
                }
            }
        }
        
        float m_value[x_numModulators][16];
        float m_valuePrev[x_numModulators][16];
        BitSet16 m_changed;
    };

    virtual ~BankedEncoderCell()
    {
    }
    
    struct Modulators
    {
        std::shared_ptr<BankedEncoderCell> m_modulators[x_numModulators];
        int m_activeModulators[x_numModulators];
        size_t m_numActiveModulators;
        EncoderType* m_modulatorTypes;
        BankedEncoderCell* m_owner;

        Modulators(EncoderType* modulatorTypes, BankedEncoderCell* owner)
            : m_modulatorTypes(modulatorTypes)
            , m_owner(owner)
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                m_modulators[i] = nullptr;
                m_activeModulators[i] = -1;
            }

            m_numActiveModulators = 0;
        }

        void Fill(BankedEncoderCell* parent, StateEncoderCell::SceneManager* sceneManager)
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (!m_modulators[i].get())
                {
                    m_modulators[i] = std::make_shared<BankedEncoderCell>(sceneManager, parent, i, m_modulatorTypes);
                    m_modulators[i]->m_numTracks = parent->m_numTracks;
                    m_activeModulators[m_numActiveModulators] = i;
                    ++m_numActiveModulators;
                }
            }
        }

        void ClearAll()
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                m_modulators[i] = nullptr;
                m_activeModulators[i] = -1;
            }

            m_numActiveModulators = 0;
        }
            
        void GarbageCollect()
        {
            for (int i = m_numActiveModulators - 1; i >= 0; --i)
            {
                m_modulators[m_activeModulators[i]]->GarbageCollectModulators();
                if (m_modulators[m_activeModulators[i]]->CanBeGarbageCollected())
                {
                    m_modulators[m_activeModulators[i]] = nullptr;
                    m_activeModulators[i] = m_activeModulators[m_numActiveModulators - 1];
                    m_activeModulators[m_numActiveModulators - 1] = -1;
                    --m_numActiveModulators;
                }
            }
        }

        void Compute(
            float* bankedValue,
            float* outputs,
            size_t numTracks,
            size_t numVoices,
            ModulatorValues& modulatorValues,
            float* brightness,
            float* ringValue,
            size_t track,
            BitSet16 changed)
        {            
            float modValue[16];
            float modWeight[16];
            double gestureWeightSum[16];
            double gestureValue[16];
            for (size_t i = 0; i < numTracks * numVoices; ++i)
            {
                modValue[i] = 0;
                modWeight[i] = 0;
                gestureWeightSum[i] = 0;
                gestureValue[i] = 0;
            }
            
            for (size_t i = 0; i < m_numActiveModulators; ++i)
            {
                if (m_modulators[m_activeModulators[i]]->m_modulatorsAffecting.IsZero())
                {
                    break;
                }

                BankedEncoderCell* cell = m_modulators[m_activeModulators[i]].get();
                cell->Compute(numTracks, numVoices, modulatorValues, changed);
                for (size_t j = 0; j < numTracks; ++j)
                {
                    if (cell->m_type == EncoderType::GestureParam)
                    {
                        size_t ix = j * numVoices;

                        double w = cell->EffectiveModulatorWeight(modulatorValues.m_value[m_activeModulators[i]][0], j);
                        gestureWeightSum[j] += w;
                        gestureValue[j] += w * static_cast<double>(bankedValue[j] * (1 - w) + static_cast<double>(cell->m_output[ix]) * w);
                    }
                    else
                    {
                        for (size_t k = 0; k < numVoices; ++k)
                        {
                            size_t ix = j * numVoices + k;
                            modValue[ix] += cell->m_output[ix] * modulatorValues.m_value[m_activeModulators[i]][ix];
                            modWeight[ix] += cell->m_output[ix];
                        }
                    }
                }                
            }

            for (size_t i = 0; i < numTracks; ++i)
            {
                float value = gestureWeightSum[i] > 0 ? static_cast<float>(gestureValue[i] / gestureWeightSum[i]) : bankedValue[i];

                for (size_t j = 0; j < numVoices; ++j)
                {
                    size_t ix = i * numVoices + j;
                    if (modWeight[ix] > 1)
                    {
                        outputs[ix] = modValue[ix] / modWeight[ix];
                    }
                    else
                    {
                        outputs[ix] = value * (1 - modWeight[ix]) + modValue[ix];
                    }
                }
            }

            float brightnessVal = 1 - modWeight[numVoices * track];
            brightnessVal *= 1 - gestureWeightSum[track];

            *brightness = std::max(0.0f, std::min(1.0f, brightnessVal));
            *ringValue = outputs[numVoices * track];
        }

        void SetAllStates()
        {
            for (size_t i = 0; i < m_numActiveModulators; ++i)
            {
                m_modulators[m_activeModulators[i]]->SetStateRecursive();
            }
        }
    };
    
    BankedEncoderCell(
        StateEncoderCell::SceneManager* sceneManager, BankedEncoderCell* parent, int index, EncoderType* modulatorTypes)
        : StateEncoderCell(0, 1, false, sceneManager)
        , m_modulators(modulatorTypes, this)
        , m_defaultValue(0)
    {
        int depth = parent ? parent->m_depth + 1 : 0;
        m_parent = parent;
        m_type = depth == 0 ? EncoderType::BaseParam : modulatorTypes[index];

        for (size_t i = 0; i < StateEncoderCell::SceneManager::x_numScenes; ++i)
        {
            for (size_t j = 0; j < 16; ++j)
            {
                m_isActive[i][j] = false;
            }
        }

        if (depth > 0)
        {
            if (m_type == EncoderType::ModulatorAmount)
            {
                m_twisterColor = (124 + 67 * (depth - 1)) / 2;
                m_color = Color::FromTwister(m_twisterColor);
            }
            else
            {
                m_twisterColor = (124 + 67 * depth) / 2;
                m_color = Color::FromTwister(m_twisterColor);
            }
        }
        else
        {
            m_twisterColor = 0;
        }
        
        m_brightness = m_type == EncoderType::GestureParam ? 0 : 1;
        m_connected = true;
        m_depth = depth;
        m_index = index;
        m_isVisible = false;
        m_modulatorsAffecting.Clear();
        m_ringValue = 0;
        m_forceUpdate = true;
        for (size_t i = 0; i < 16; ++i)
        {
            m_bankedValue[i] = 0;
            SetStatePtr(&m_bankedValue[i], i);
            m_output[i] = 0;
        }
    }

    void SetForceUpdateRecursive()
    {
        m_forceUpdate = true;
        if (m_parent)
        {
            m_parent->SetForceUpdateRecursive();
        }
    }

    virtual void PostSetState() override
    {
        SetForceUpdateRecursive();
    }

    float EffectiveModulatorWeight(float weight, int track)
    {
        float w1 = m_isActive[m_sceneManager->m_scene1][track] ? weight : 0;
        float w2 = m_isActive[m_sceneManager->m_scene2][track] ? weight : 0;
        return w1 * (1 - m_sceneManager->m_blendFactor) + w2 * m_sceneManager->m_blendFactor;
    }

    void SetDefaultValue()
    {
        m_defaultValue = m_bankedValue[m_sceneManager->m_track];
    }

    void RevertToDefault()
    {
        ZeroModulatorsCurrentScene();
        SetToValue(m_defaultValue);
    }

    virtual uint8_t GetTwisterColor() override
    {
        return m_twisterColor;
    }

    virtual uint8_t GetAnimationValue() override
    {
        return BrightnessToAnimationValue(m_brightness);
    }

    virtual float GetNormalizedValue() override
    {
        return m_ringValue;
    }

    void CopyToScene(size_t scene)
    {
        StateEncoderCell::CopyToScene(scene);
        for (size_t i = 0; i < 16; ++i)
        {
            m_isActive[scene][i] = (m_isActive[m_sceneManager->m_scene1][i] && m_sceneManager->m_blendFactor < 1)
                                || (m_isActive[m_sceneManager->m_scene2][i] && m_sceneManager->m_blendFactor > 0);
        }

        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->CopyToScene(scene);
        }
    }

    void ZeroModulatorsCurrentScene()
    {
        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->ZeroCurrentScene();
            m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->DeactivateGestureCurrentScene();
            m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->ZeroModulatorsCurrentScene();
        }

        GarbageCollectModulators();
    }

    bool CanBeGarbageCollected()
    {
        if (m_type == EncoderType::GestureParam)
        {
            for (size_t i = 0; i < StateEncoderCell::SceneManager::x_numScenes; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    if (m_isActive[i][j])
                    {
                        return false;
                    }
                }
            }

            return true;
        }
        else
        {
            return m_modulators.m_numActiveModulators == 0 && AllZero();
        }
    }

    struct Input
    {
        uint8_t m_twisterColor;
        Color m_color;
        bool m_connected;

        Input()
            : m_twisterColor(1)
            , m_color(Color(0, 0, 0))
            , m_connected(false)
        {
        }
    };

    void ProcessInput(Input& input)
    {
        m_twisterColor = input.m_twisterColor;
        m_color = input.m_color;
        m_connected = input.m_connected;
    }

    void Compute(size_t numTracks, size_t numVoices, ModulatorValues& modulatorValues, BitSet16 changed)
    {
        if (!m_connected)
        {
            m_brightness = 0;
            m_ringValue = 0;
            return;
        }
        else if (m_forceUpdate || !m_modulatorsAffecting.Intersect(changed).IsZero())
        {
            m_modulators.Compute(m_bankedValue, m_output, numTracks, numVoices, modulatorValues, &m_brightness, &m_ringValue, m_sceneManager->m_track, changed);
        }
        
        if (m_depth > 0)
        {
            if (m_type == EncoderType::ModulatorAmount)
            {
                m_brightness = std::max(0.0f, std::min(1.0f, modulatorValues.m_value[m_index][m_sceneManager->m_track * numVoices]));
            }
            else
            {
                m_brightness = IsActive() ? 1 : 0;
            }
        }

        m_forceUpdate = false;
    }

    JSON ToJSON() 
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("values", StateEncoderCell::ToJSON());
        if (m_modulators.m_numActiveModulators > 0)
        {
            JSON modulatorsJ = JSON::Array();
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (m_modulators.m_modulators[i].get())
                {
                    modulatorsJ.AppendNew(m_modulators.m_modulators[i]->ToJSON());
                }
                else
                {
                    modulatorsJ.AppendNew(JSON::Null());
                }
            }

            rootJ.SetNew("modulators", modulatorsJ);
        }

        if (m_type == EncoderType::GestureParam)
        {
            JSON activeJ = JSON::Array();
            for (size_t i = 0; i < StateEncoderCell::SceneManager::x_numScenes; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    activeJ.AppendNew(JSON::Boolean(m_isActive[i][j]));
                }
            }

            rootJ.SetNew("active", activeJ);
        }

        rootJ.SetNew("defaultValue", JSON::Real(m_defaultValue));

        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON valuesJ = rootJ.Get("values");
        if (!valuesJ.IsNull())
        {
            StateEncoderCell::FromJSON(valuesJ);
        }        

        JSON defaultValueJ = rootJ.Get("defaultValue");
        if (!defaultValueJ.IsNull())
        {
            m_defaultValue = defaultValueJ.RealValue();
        }
        
        JSON modulatorsJ = rootJ.Get("modulators");
        m_modulators.ClearAll();
        if (!modulatorsJ.IsNull())
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                JSON modulatorJ = modulatorsJ.GetAt(i);
                if (!modulatorJ.IsNull())
                {
                    if (modulatorJ.IsNull())
                    {
                        m_modulators.m_modulators[i] = nullptr;
                    }
                    else
                    {
                        m_modulators.m_modulators[i] = std::make_shared<BankedEncoderCell>(m_sceneManager, this, i, m_modulators.m_modulatorTypes);
                        m_modulators.m_modulators[i]->FromJSON(modulatorJ);
                        m_modulators.m_activeModulators[m_modulators.m_numActiveModulators] = i;
                        ++m_modulators.m_numActiveModulators;
                    }
                }
            }

            GarbageCollectModulators();
            if (m_depth == 0)
            {
                SetModulatorsAffecting();
            }
        }

        JSON activeJ = rootJ.Get("active");
        if (!activeJ.IsNull())
        {
            for (size_t i = 0; i < StateEncoderCell::SceneManager::x_numScenes; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    m_isActive[i][j] = activeJ.GetAt(i * 16 + j).BooleanValue();
                }
            }
        }
    }

    void FillModulators(StateEncoderCell::SceneManager* sceneManager)
    {
        m_modulators.Fill(this, sceneManager);
        SetForceUpdateRecursive();
    }

    void GarbageCollectModulators()
    {
        m_modulators.GarbageCollect();
    }

    void SetStateRecursive()
    {
        SetState();
        m_modulators.SetAllStates();
    }

    bool IsActive()
    {
        if (m_type != EncoderType::GestureParam)
        {
            return true;
        }
        else
        {
            return (m_sceneManager->m_blendFactor > 0 && m_isActive[m_sceneManager->m_scene2][m_sceneManager->m_track])
                || (m_sceneManager->m_blendFactor < 1 && m_isActive[m_sceneManager->m_scene1][m_sceneManager->m_track]);
        }
    }

    void ToggleActive()
    {
        if (m_sceneManager->m_blendFactor > 0)
        {
            m_isActive[m_sceneManager->m_scene2][m_sceneManager->m_track] = !m_isActive[m_sceneManager->m_scene2][m_sceneManager->m_track];
        }
        
        if (m_sceneManager->m_blendFactor < 1)
        {
            m_isActive[m_sceneManager->m_scene1][m_sceneManager->m_track] = !m_isActive[m_sceneManager->m_scene1][m_sceneManager->m_track];
        }

        m_brightness = IsActive() ? 1 : 0;
    }

    void DeactivateGestureCurrentScene()
    {
        if (m_sceneManager->m_blendFactor > 0)
        {
            m_isActive[m_sceneManager->m_scene2][m_sceneManager->m_track] = false;
        }
        
        if (m_sceneManager->m_blendFactor < 1)
        {
            m_isActive[m_sceneManager->m_scene1][m_sceneManager->m_track] = false;
        }
    }

    void SetModulatorsAffecting()
    {
        m_modulatorsAffecting.Clear();
        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->SetModulatorsAffecting();
            m_modulatorsAffecting = m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->m_modulatorsAffecting.Union(m_modulatorsAffecting);
        }

        if (m_depth > 0)
        {
            if (!m_modulatorsAffecting.IsZero() || !IsZeroCurrentScene() || m_isVisible)
            {
                m_modulatorsAffecting.Set(m_index, true);
            }
        }

        int lastAffectedModulator = m_modulators.m_numActiveModulators - 1;
        for (int i = m_modulators.m_numActiveModulators - 1; i >= 0; --i)
        {
            if (m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->m_modulatorsAffecting.IsZero())
            {
                std::swap(m_modulators.m_activeModulators[i], m_modulators.m_activeModulators[lastAffectedModulator]);
                --lastAffectedModulator;
            }
        }
    }
                    
    BankedEncoderCell* m_parent;
    uint8_t m_twisterColor;
    Color m_color;
    float m_brightness;
    float m_ringValue;
    bool m_connected;
    Modulators m_modulators;
    int m_depth;
    int m_index;
    float m_bankedValue[16];
    float m_output[16];
    float m_defaultValue;
    BitSet16 m_modulatorsAffecting;
    bool m_isVisible;
    BankedEncoderCell::EncoderType m_type;
    bool m_isActive[StateEncoderCell::SceneManager::x_numScenes][16];
    bool m_forceUpdate;
};

struct EncoderBankInternal : public EncoderGrid
{
    struct Output
    {
        bool m_sceneLight[StateEncoderCell::SceneManager::x_numScenes];
    };

    StateEncoderCell::SceneManager m_sceneManager;
    std::shared_ptr<BankedEncoderCell> m_baseCell[4][4];
    Output m_output;
    std::shared_ptr<BankedEncoderCell> m_selected;
    bool m_shift;
    BankedEncoderCell::EncoderType m_modulatorTypes[BankedEncoderCell::x_numModulators];
    BulkFilter<16 * 16> m_bulkFilter;
    size_t m_totalVoices;
    size_t m_activeEncoderPrefix;
    size_t m_numTracks;
    size_t m_numVoices;
    size_t m_currentTrack;

    void PutAndSetVisible(int x, int y, std::shared_ptr<BankedEncoderCell> cell)
    {
        if (Get(x, y))
        {
            static_cast<BankedEncoderCell*>(Get(x, y))->m_isVisible = false;
        }
        
        Put(x, y, cell);
        cell->m_isVisible = true;
    }
    
    EncoderBankInternal()
    {
        for (size_t i = 0; i < BankedEncoderCell::x_numModulators; ++i)
        {
            m_modulatorTypes[i] = BankedEncoderCell::EncoderType::ModulatorAmount;
        }

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j] = std::make_shared<BankedEncoderCell>(&m_sceneManager, nullptr, i + 4 * j, m_modulatorTypes);
                PutAndSetVisible(i, j, m_baseCell[i][j]);
            }
        }

        m_selected = nullptr;
    }

    void SetAsDefault()
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j]->SetDefaultValue();
            }
        }
    }

    void RevertToDefault()
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j]->RevertToDefault();
            }
        }
    }

    BankedEncoderCell* GetBase(int i, int j)
    {
        return m_baseCell[i][j].get();
    }

    struct Input
    {
        StateEncoderCell::SceneManager::Input m_sceneManagerInput;
        size_t m_numTracks;
        size_t m_numVoices;
        size_t m_sceneChange;
        bool m_shift;
        BankedEncoderCell::Input m_cellInput[4][4];
        BankedEncoderCell::ModulatorValues m_modulatorValues;
        bool m_revertToDefault;

        Input()
            : m_numTracks(0)
            , m_numVoices(0)
            , m_sceneChange(BankedEncoderCell::SceneManager::x_numScenes)
            , m_shift(false)
            , m_revertToDefault(false)
        {
        }
    };

    void CopyToScene(int scene)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j]->CopyToScene(scene);
            }
        }
    }

    void SetAllModulatorsAffecting()
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                GetBase(i, j)->SetModulatorsAffecting();
            }
        }
    }

    void SetNumTracks(size_t numTracks)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                GetBase(i, j)->SetNumTracks(numTracks);
            }
        }
    }

    void ProcessInput(Input& input)
    {
        m_numTracks = input.m_numTracks;
        m_numVoices = input.m_numVoices;
        m_currentTrack = input.m_sceneManagerInput.m_track;

        SetNumTracks(input.m_numTracks);

        if (input.m_sceneChange < BankedEncoderCell::SceneManager::x_numScenes)
        {
            if (input.m_shift)
            {
                CopyToScene(input.m_sceneChange);
            }
            else
            {
                if (input.m_sceneManagerInput.m_blendFactor <= 0.5)
                {
                    input.m_sceneManagerInput.m_scene2 = input.m_sceneChange;
                }
                else
                {
                    input.m_sceneManagerInput.m_scene1 = input.m_sceneChange;
                }
            }
                            
            input.m_sceneChange = BankedEncoderCell::SceneManager::x_numScenes;                
        }

        m_shift = input.m_shift;

        {
            bool changed = false;
            bool changedScene = false;
            m_sceneManager.Process(input.m_sceneManagerInput, &changed, &changedScene);
            if (changed)
            {
                for (int i = 0; i < 4; ++i)
                {
                    for (int j = 0; j < 4; ++j)
                    {
                        GetBase(i, j)->SetStateRecursive();
                    }
                }
            }

            if (changedScene)
            {
                SetAllModulatorsAffecting();
            }
        }
        
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {                    
                GetBase(i, j)->ProcessInput(input.m_cellInput[i][j]);
            }
        }

        if (input.m_revertToDefault)
        {
            RevertToDefault();
        }

        BitSet16 changed = input.m_modulatorValues.m_changed;
        m_totalVoices = input.m_numVoices * input.m_numTracks;
        m_activeEncoderPrefix = 0;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (GetBase(i, j)->m_connected)
                {
                    m_activeEncoderPrefix = i * 4 + j + 1;
                }

                GetBase(i, j)->Compute(input.m_numTracks, input.m_numVoices, input.m_modulatorValues, changed);
                m_bulkFilter.LoadTarget(m_totalVoices, (i * 4 + j) * m_totalVoices, GetBase(i, j)->m_output);
            }
        }
    }

    void ProcessBulkFilter()
    {
        m_bulkFilter.Process(m_totalVoices * m_activeEncoderPrefix);
    }

    std::pair<int, int> ModulatorIndexToXY(int index)
    {
        return std::make_pair(index % 4, index / 4);
    }

    void MakeSelection(int x, int y, std::shared_ptr<BankedEncoderCell> cell)
    {
        cell->FillModulators(&m_sceneManager);
        m_selected = cell;
        PutAndSetVisible(3, 3, m_selected);
        for (size_t i = 0; i < BankedEncoderCell::x_numModulators; ++i)
        {
            auto xy = ModulatorIndexToXY(i);
            PutAndSetVisible(xy.first, xy.second, m_selected->m_modulators.m_modulators[i]);
        }

        m_selected->SetModulatorsAffecting();
    }

    void Deselect()
    {
        if (m_selected.get())
        {
            m_selected->GarbageCollectModulators();
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    PutAndSetVisible(i, j, m_baseCell[i][j]);
                    m_baseCell[i][j]->SetModulatorsAffecting();
                }
            }

            m_selected.reset();
        }
    }

    virtual void HandlePress(int x, int y) override
    {
        std::shared_ptr<BankedEncoderCell> cell = std::static_pointer_cast<BankedEncoderCell>(GetShared(x, y));

        if (cell && cell->m_type == BankedEncoderCell::EncoderType::GestureParam)
        {
            cell->ToggleActive();
        }
        else if (m_shift)
        {
            cell->ZeroModulatorsCurrentScene();
        }
        else if (m_selected.get() && x == 3 && y == 3)
        {
            Deselect();
        }
        else
        {
            MakeSelection(x, y, cell);
        }
    }

    void SetModulatorType(size_t index, BankedEncoderCell::EncoderType type)
    {
        m_modulatorTypes[index] = type;
    }

    JSON ToJSON()
    {
        JSON rootJ = JSON::Array();
        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                rootJ.AppendNew(GetBase(i, j)->ToJSON());
            }
        }

        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                GetBase(i, j)->FromJSON(rootJ.GetAt(i * 4 + j));
            }
        }
    }    

    float GetValue(size_t i, size_t j, size_t channel)
    {
        return m_bulkFilter.m_output[(i * 4 + j) * m_totalVoices + channel];
    }

    float GetValueNoSlew(size_t i, size_t j, size_t channel)
    {
        return m_bulkFilter.m_target[(i * 4 + j) * m_totalVoices + channel];
    }

    virtual void Apply(MessageIn msg) override
    {
        if (msg.m_mode == MessageIn::Mode::EncoderIncDec)
        {
            std::shared_ptr<BankedEncoderCell> cell = std::static_pointer_cast<BankedEncoderCell>(GetShared(msg.m_x, msg.m_y));
            cell->HandleIncDec(msg.m_amount);
        }
        else if (msg.m_mode == MessageIn::Mode::EncoderPush)
        {
            HandlePress(msg.m_x, msg.m_y);
        }
    }

    void PopulateUIState(EncoderBankUIState* uiState)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                std::shared_ptr<BankedEncoderCell> cell = std::static_pointer_cast<BankedEncoderCell>(GetShared(i, j));

                if (cell->m_connected)
                {
                    uiState->SetConnected(i, j, true);
                    uiState->SetColor(i, j, cell->m_color);
                    uiState->SetBrightness(i, j, cell->m_brightness);
                    for (size_t k = 0; k < m_numTracks * m_numVoices; ++k)
                    {
                        uiState->SetValue(i, j, k, cell->m_output[k]);
                    }
                }
                else
                {
                    uiState->SetConnected(i, j, false);
                    uiState->SetBrightness(i, j, 0);
                    for (size_t k = 0; k < m_numTracks * m_numVoices; ++k)
                    {
                        uiState->SetValue(i, j, k, 0);
                    }
                }
            }
        }

        uiState->SetNumTracks(m_numTracks);
        uiState->SetNumVoices(m_numVoices);
        uiState->SetCurrentTrack(m_currentTrack);
    }
};

#ifndef IOS_BUILD
struct EncoderBankIOManager
{
    IOMgr* m_ioMgr;
    EncoderBankInternal::Input* m_state;
    bool m_saveJSON;
    bool m_loadJSON;

    // Inputs
    //
    IOMgr::Input* m_blendFactor;
    IOMgr::Input* m_modulationInputs[BankedEncoderCell::x_numModulators];
    IOMgr::Input* m_shift;

    // Triggers
    //
    IOMgr::Trigger* m_sceneChangeSelector;
    IOMgr::Trigger* m_trackSelector;
    IOMgr::Trigger* m_revertToDefault;
    IOMgr::Trigger* m_saveJSONTrigger;
    IOMgr::Trigger* m_loadJSONTrigger;

    // Random Access Switches
    //
    IOMgr::RandomAccessSwitch* m_sceneChange;
    IOMgr::RandomAccessSwitch* m_track;

    EncoderBankIOManager(
        IOMgr* ioMgr,
        EncoderBankInternal::Input* state)
        : m_ioMgr(ioMgr)
        , m_state(state)
        , m_saveJSON(false)
        , m_loadJSON(false)
    {
        CreateInputs();
    }

    void CreateInputs()
    {
        m_blendFactor = m_ioMgr->AddInput("blendFactor", false);
        m_blendFactor->SetTarget(0, &m_state->m_sceneManagerInput.m_blendFactor);
        m_blendFactor->m_scale = 0.1;

        for (size_t i = 0; i < BankedEncoderCell::x_numModulators; ++i)
        {
            m_modulationInputs[i] = m_ioMgr->AddInput("modulation " + std::to_string(i), true);
            m_modulationInputs[i]->m_scale = 0.1;
            for (size_t j = 0; j < 16; ++j)
            {
                m_modulationInputs[i]->SetTarget(j, &m_state->m_modulatorValues.m_value[i][j]);
            }
        }

        m_sceneChangeSelector = m_ioMgr->AddTrigger("Scene Change Selector", false);
        m_sceneChange = m_ioMgr->AddRandomAccessSwitch(&m_state->m_sceneChange, false);

        for (size_t i = 0; i < BankedEncoderCell::SceneManager::x_numScenes; ++i)
        {
            m_sceneChangeSelector->SetTrigger(i, m_sceneChange->AddTrigger());
        }

        m_trackSelector = m_ioMgr->AddTrigger("Track Selector", false);
        m_track = m_ioMgr->AddRandomAccessSwitch(&m_state->m_sceneManagerInput.m_track, false);
                
        for (size_t i = 0; i < 3; ++i)
        {
            m_trackSelector->SetTrigger(i, m_track->AddTrigger());
        }

        m_shift = m_ioMgr->AddInput("Shift", false);
        m_shift->SetTarget(0, &m_state->m_shift);

        m_revertToDefault = m_ioMgr->AddTrigger("Revert to Default Trigger", false);
        m_revertToDefault->SetTrigger(0, &m_state->m_revertToDefault);

        m_saveJSONTrigger = m_ioMgr->AddTrigger("Save JSON", false);
        m_saveJSONTrigger->SetTrigger(0, &m_saveJSON);

        m_loadJSONTrigger = m_ioMgr->AddTrigger("Load JSON", false);
        m_loadJSONTrigger->SetTrigger(0, &m_loadJSON);
    }
};

struct EncoderBank : Module
{
    EncoderBankInternal m_bank;
    EncoderBankInternal::Input m_state;

    JSON m_savedJSON;
    
    IOMgr m_ioMgr;
    IOMgr::Output* m_output[4][4];
    IOMgr::Output* m_gridId;
    IOMgr::Output* m_sceneLight;
    IOMgr::Param* m_color;
    IOMgr::Param* m_numTracks;
    IOMgr::Param* m_numVoices;

    EncoderBankIOManager m_bankIOMgr;

    EncoderBank()
        : m_savedJSON(JSON::Null())
        , m_ioMgr(this)
        , m_bankIOMgr(&m_ioMgr, &m_state)
    {
        m_color = m_ioMgr.AddParam("Color", 0, 254, 64, false);        
        
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_output[i][j] = m_ioMgr.AddOutput("output " + std::to_string(i) + " " + std::to_string(j), true);
                m_output[i][j]->m_scale = 10;
                m_color->SetTarget(i * 4 + j, &m_state.m_cellInput[i][j].m_twisterColor);
                for (size_t k = 0; k < 16; ++k)
                {
                    m_output[i][j]->SetSource(k, &m_bank.GetBase(i,j)->m_output[k]);
                }
            }
        }

        m_gridId = m_ioMgr.AddOutput("gridId", false);
        m_gridId->SetSource(0, &m_bank.m_gridId);

        m_sceneLight = m_ioMgr.AddOutput("Scene Light", false);
        for (size_t i = 0; i < BankedEncoderCell::SceneManager::x_numScenes; ++i)
        {
            m_sceneLight->SetSource(i, &m_bank.m_output.m_sceneLight[i]);
        }

        m_sceneLight->SetChannels(BankedEncoderCell::SceneManager::x_numScenes);

        m_numTracks = m_ioMgr.AddParam("Num Tracks", 1, 16, 1, false);
        m_numTracks->SetTarget(0, &m_state.m_numTracks);
        m_numTracks->m_switch = true;

        m_numVoices = m_ioMgr.AddParam("Num Voices", 1, 16, 1, false);
        m_numVoices->SetTarget(0, &m_state.m_numVoices);
        m_numVoices->m_switch = true;

        m_ioMgr.Config();
    }

    void SetSources()
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                IOMgr::Output* output = m_output[i][j];
                output->SetChannels(m_state.m_numTracks * m_state.m_numVoices);
            }
        }
    }

    void SetSceneLight()
    {
        size_t scene1 = (m_bank.m_sceneManager.m_blendFactor <= 0.5) ? m_state.m_sceneManagerInput.m_scene1 : m_state.m_sceneManagerInput.m_scene2;
        size_t scene2 = (m_bank.m_sceneManager.m_blendFactor <= 0.5) ? m_state.m_sceneManagerInput.m_scene2 : m_state.m_sceneManagerInput.m_scene1;
        
        for (size_t i = 0; i < BankedEncoderCell::SceneManager::x_numScenes; ++i)
        {
            m_bank.m_output.m_sceneLight[i] = scene1 == i ||
                (scene2 == i && m_ioMgr.Flash());
        }
    }
    
    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        if (m_ioMgr.IsControlFrame() || m_ioMgr.m_firstFrame)
        {
            SetSources();
            
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    m_state.m_cellInput[i][j].m_connected = m_output[i][j]->m_connected;
                }
            }

            SetSceneLight();

            if (m_bankIOMgr.m_saveJSON)
            {
                SaveJSON();
                m_bankIOMgr.m_saveJSON = false;
            }

            if (m_bankIOMgr.m_loadJSON)
            {
                LoadSavedJSON();
                m_bankIOMgr.m_loadJSON = false;
            }
        }

        m_state.m_modulatorValues.ComputeChanged();

        m_bank.ProcessStatic(args.sampleTime);
        if (m_ioMgr.IsControlFrame())
        {
            m_bank.ProcessInput(m_state);
        }

        m_bank.ProcessBulkFilter();
        m_ioMgr.SetOutputs();
    }

    json_t* dataToJson() override
    {
        JSON rootJ = JSON::Object();
        JSON encoders;

        if (!m_savedJSON.IsNull())
        {
            encoders = m_savedJSON.Incref();
        }
        else
        {
            encoders = JSON::Array();
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    encoders.AppendNew(m_bank.GetBase(i, j)->ToJSON());
                }
            }
        }

        rootJ.SetNew("encoders", encoders);
        return static_cast<json_t*>(rootJ.ToJsonT());
    }

    void SaveJSON()
    {
        m_savedJSON = m_bank.ToJSON();
    }

    void LoadSavedJSON()
    {
        if (!m_savedJSON.IsNull())
        {
            m_bank.FromJSON(m_savedJSON);
        }
    }

    void dataFromJson(json_t* rootJ) override
    {
        JSON root = JSON::FromJsonT(rootJ);
        JSON encoders = root.Get("encoders");

        if (!encoders.IsNull())
        {
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    m_bank.GetBase(i, j)->FromJSON(encoders.GetAt(i * 4 + j));
                }
            }
        }

        m_savedJSON = encoders.Incref();
    }

    void SetAsDefault()
    {
        m_bank.SetAsDefault();
    }
};

struct EncoderBankWidget : public ModuleWidget
{
    struct SetDefaultItem : MenuItem
    {
        EncoderBank* m_module;
        SetDefaultItem(EncoderBank* module)
        {
            m_module = module;
            text = "Set as Default";
        }
        
        void onAction(const event::Action &e) override
        {
            m_module->SetAsDefault();
        }
    };
    
    EncoderBankWidget(EncoderBank* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/EncoderBank.svg")));

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        if (module)
        {
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    module->m_output[i][j]->Widget(this, 6 + i, 4 + j);
                    if (i != 3 || j != 3)
                    {
                        module->m_bankIOMgr.m_modulationInputs[i + 4 * j]->Widget(this, 1 + i, 4 + j);
                    }
                }
            }

            module->m_gridId->Widget(this, 1, 10);
            module->m_bankIOMgr.m_blendFactor->Widget(this, 2, 10);
            module->m_bankIOMgr.m_sceneChangeSelector->Widget(this, 3, 10);
            module->m_bankIOMgr.m_trackSelector->Widget(this, 4, 10);
            module->m_bankIOMgr.m_shift->Widget(this, 5, 10);
            module->m_sceneLight->Widget(this, 6, 10);
            module->m_bankIOMgr.m_revertToDefault->Widget(this, 7, 10);
            module->m_bankIOMgr.m_saveJSONTrigger->Widget(this, 8, 10);
            module->m_bankIOMgr.m_loadJSONTrigger->Widget(this, 9, 10);

            module->m_numTracks->Widget(this, 3, 11);
            module->m_numVoices->Widget(this, 4, 11);

            module->m_color->Widget(this, 1, 11);
        }        
    }

    void appendContextMenu(Menu *menu) override
    {
        menu->addChild(new SetDefaultItem(dynamic_cast<EncoderBank*>(module)));
    }
};
#endif

}
