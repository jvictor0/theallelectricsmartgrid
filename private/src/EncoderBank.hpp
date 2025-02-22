#include "Encoder.hpp"
#include "ModuleUtils.hpp"
#include "BitSet.hpp"

namespace SmartGrid
{

struct BankedEncoderCell : public StateEncoderCell
{
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
                }
            }
        }

        BitSet16 GetChanged()
        {
            BitSet16 changed;
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (memcmp(m_value[i], m_valuePrev[i], 16 * sizeof(float)) != 0)
                {
                    changed.Set(i, true);
                    memcpy(m_valuePrev[i], m_value[i], 16 * sizeof(float));
                }
            }

            return changed;
        }
        
        float m_value[x_numModulators][16];
        float m_valuePrev[x_numModulators][16];
    };

    virtual ~BankedEncoderCell()
    {
    }
    
    struct Modulators
    {
        std::shared_ptr<BankedEncoderCell> m_modulators[x_numModulators];
        int m_activeModulators[x_numModulators];
        size_t m_numActiveModulators;

        Modulators()
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
                    m_modulators[i] = std::make_shared<BankedEncoderCell>(sceneManager, parent->m_depth + 1, i);
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
            BitSet16 changed,
            bool control)
        {            
            float modValue[16];
            float modWeight[16];
            for (size_t i = 0; i < numTracks * numVoices; ++i)
            {
                modValue[i] = 0;
                modWeight[i] = 0;
            }
            
            for (size_t i = 0; i < m_numActiveModulators; ++i)
            {
                if (m_modulators[m_activeModulators[i]]->m_modulatorsAffecting.IsZero())
                {
                    break;
                }

                BankedEncoderCell* cell = m_modulators[m_activeModulators[i]].get();
                cell->Compute(numTracks, numVoices, modulatorValues, changed, control);
                for (size_t j = 0; j < numTracks; ++j)
                {
                    for (size_t k = 0; k < numVoices; ++k)
                    {
                        size_t ix = j * numVoices + k;
                        modValue[ix] += cell->m_output[ix] * modulatorValues.m_value[m_activeModulators[i]][ix];
                        modWeight[ix] += cell->m_output[ix];
                    }
                }                
            }

            for (size_t i = 0; i < numTracks; ++i)
            {
                for (size_t j = 0; j < numVoices; ++j)
                {
                    size_t ix = i * numVoices + j;
                    if (modWeight[ix] > 1)
                    {
                        outputs[ix] = modValue[ix] / modWeight[ix];
                    }
                    else
                    {
                        float value = bankedValue[i];
                        outputs[ix] = value * (1 - modWeight[ix]) + modValue[ix];
                    }
                }
            }

            *brightness = std::max(0.0f, std::min(1.0f, 1 - modWeight[numVoices * track]));
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
        StateEncoderCell::SceneManager* sceneManager, int depth, int index)
        : StateEncoderCell(0, 1, false, sceneManager)
        , m_defaultValue(0)
    {
        if (depth > 0)
        {
            m_twisterColor = 124 + 67 * (depth - 1);
        }
        else
        {
            m_twisterColor = 0;
        }
        
        m_brightness = 1.0;
        m_connected = true;
        m_depth = depth;
        m_index = index;
        m_isVisible = false;
        for (size_t i = 0; i < 16; ++i)
        {
            m_bankedValue[i] = 0;
            SetStatePtr(&m_bankedValue[i], i);
        }
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
            m_modulators.m_modulators[m_modulators.m_activeModulators[i]]->ZeroModulatorsCurrentScene();
        }

        GarbageCollectModulators();
    }

    bool CanBeGarbageCollected()
    {
        return m_modulators.m_numActiveModulators == 0 && AllZero();
    }

    struct Input
    {
        uint8_t m_twisterColor;
        bool m_connected;
    };

    void ProcessInput(Input& input)
    {
        m_twisterColor = input.m_twisterColor;
        m_connected = input.m_connected;
    }

    void Compute(size_t numTracks, size_t numVoices, ModulatorValues& modulatorValues, BitSet16 changed, bool control)
    {
        if (!m_connected)
        {
            m_brightness = 0;
            m_ringValue = 0;
            return;
        }
        else if (m_modulatorsAffecting.Intersect(changed).IsZero())
        {
            if (control && m_modulatorsAffecting.IsZero())
            {
                for (size_t i = 0; i < numTracks; ++i)
                {
                    for (size_t j = 0; j < numVoices; ++j)
                    {
                        m_output[i * numVoices + j] = m_bankedValue[i];
                    }
                }
                
                m_brightness = 1;
                m_ringValue = m_output[numVoices * m_sceneManager->m_track];
            }
        }
        else
        {
            m_modulators.Compute(m_bankedValue, m_output, numTracks, numVoices, modulatorValues, &m_brightness, &m_ringValue, m_sceneManager->m_track, changed, control);
        }
        
        if (m_depth > 0)
        {
            m_brightness = std::max(0.0f, std::min(1.0f, modulatorValues.m_value[m_index][m_sceneManager->m_track * numVoices]));
        }
    }

    json_t* ToJSON() 
    {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "values", StateEncoderCell::ToJSON());
        if (m_modulators.m_numActiveModulators > 0)
        {
            json_t* modulatorsJ = json_array();
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (m_modulators.m_modulators[i].get())
                {
                    json_array_append_new(modulatorsJ, m_modulators.m_modulators[i]->ToJSON());
                }
                else
                {
                    json_array_append_new(modulatorsJ, json_null());
                }
            }

            json_object_set_new(rootJ, "modulators", modulatorsJ);
        }

        json_object_set_new(rootJ, "defaultValue", json_real(m_defaultValue));

        return rootJ;
    }

    void FromJSON(json_t* rootJ)
    {
        json_t* valuesJ = json_object_get(rootJ, "values");
        if (valuesJ)
        {
            StateEncoderCell::FromJSON(valuesJ);
        }        

        json_t* defaultValueJ = json_object_get(rootJ, "defaultValue");
        if (defaultValueJ)
        {
            m_defaultValue = json_real_value(defaultValueJ);
        }
        
        json_t* modulatorsJ = json_object_get(rootJ, "modulators");
        m_modulators.ClearAll();
        if (modulatorsJ)
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                json_t* modulatorJ = json_array_get(modulatorsJ, i);
                if (modulatorJ)
                {
                    if (json_is_null(modulatorJ))
                    {
                        m_modulators.m_modulators[i] = nullptr;
                    }
                    else
                    {
                        m_modulators.m_modulators[i] = std::make_shared<BankedEncoderCell>(m_sceneManager, m_depth + 1, i);
                        m_modulators.m_modulators[i]->FromJSON(modulatorJ);
                        m_modulators.m_activeModulators[m_modulators.m_numActiveModulators] = i;
                        ++m_modulators.m_numActiveModulators;
                    }
                }
            }

            GarbageCollectModulators();
        }
    }

    void FillModulators(StateEncoderCell::SceneManager* sceneManager)
    {
        m_modulators.Fill(this, sceneManager);
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
                    
    uint8_t m_twisterColor;
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
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j] = std::make_shared<BankedEncoderCell>(&m_sceneManager, 0, i + 4 * j);
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

    void ProcessInput(Input& input, bool control)
    {
        if (input.m_sceneChange < BankedEncoderCell::SceneManager::x_numScenes)
        {
            if (input.m_shift)
            {
                for (int i = 0; i < 4; ++i)
                {
                    for (int j = 0; j < 4; ++j)
                    {
                        m_baseCell[i][j]->CopyToScene(input.m_sceneChange);
                    }
                }                
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
            m_sceneManager.Process(input.m_sceneManagerInput, &changed);
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
        }
        
        if (control)
        {
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {                    
                    input.m_cellInput[i][j].m_twisterColor = input.m_cellInput[0][0].m_twisterColor;
                    GetBase(i, j)->ProcessInput(input.m_cellInput[i][j]);
                    GetBase(i, j)->SetModulatorsAffecting();
                }
            }

            if (input.m_revertToDefault)
            {
                RevertToDefault();
            }
        }

        BitSet16 changed = control ? BitSet16(0xFFFF) : input.m_modulatorValues.GetChanged();
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                GetBase(i, j)->Compute(input.m_numTracks, input.m_numVoices, input.m_modulatorValues, changed, control);
            }
        }
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
                }
            }

            m_selected.reset();
        }
    }

    virtual void HandlePress(int x, int y) override
    {
        if (m_shift)
        {
            std::shared_ptr<BankedEncoderCell> cell = std::static_pointer_cast<BankedEncoderCell>(GetShared(x, y));
            cell->ZeroModulatorsCurrentScene();
        }
        else if (m_selected.get() && x == 3 && y == 3)
        {
            Deselect();
        }
        else
        {
            std::shared_ptr<BankedEncoderCell> cell = std::static_pointer_cast<BankedEncoderCell>(GetShared(x, y));
            MakeSelection(x, y, cell);
        }
    }
};

struct EncoderBank : Module
{
    EncoderBankInternal m_bank;
    EncoderBankInternal::Input m_state;
    bool m_saveJSON;

    json_t* m_savedJSON;
    
    IOMgr m_ioMgr;
    IOMgr::Output* m_output[4][4];
    IOMgr::Output* m_gridId;

    IOMgr::Input* m_blendFactor;
    IOMgr::Trigger* m_sceneChangeSelector;
    IOMgr::Trigger* m_trackSelector;
    
    IOMgr::RandomAccessSwitch* m_sceneChange;
    IOMgr::RandomAccessSwitch* m_track;

    IOMgr::Param* m_color;

    IOMgr::Input* m_modulationInputs[BankedEncoderCell::x_numModulators];
    IOMgr::Input* m_shift;

    IOMgr::Output* m_sceneLight;

    IOMgr::Trigger* m_revertToDefault;
    IOMgr::Trigger* m_saveJSONTrigger;

    IOMgr::Param* m_numTracks;
    IOMgr::Param* m_numVoices;

    EncoderBank()
        : m_savedJSON(nullptr)
        , m_ioMgr(this)
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

        for (size_t i = 0; i < BankedEncoderCell::x_numModulators; ++i)
        {
            m_modulationInputs[i] = m_ioMgr.AddInput("modulation " + std::to_string(i), true);
            m_modulationInputs[i]->m_scale = 0.1;
            for (size_t j = 0; j < 16; ++j)
            {
                m_modulationInputs[i]->SetTarget(j, &m_state.m_modulatorValues.m_value[i][j]);
            }
        }   

        m_gridId = m_ioMgr.AddOutput("gridId", false);
        m_gridId->SetSource(0, &m_bank.m_gridId);

        m_blendFactor = m_ioMgr.AddInput("blendFactor", false);
        m_blendFactor->SetTarget(0, &m_state.m_sceneManagerInput.m_blendFactor);
        m_blendFactor->m_scale = 0.1;

        m_sceneChangeSelector = m_ioMgr.AddTrigger("Scene Change Selector", false);
        m_sceneChange = m_ioMgr.AddRandomAccessSwitch(&m_state.m_sceneChange, false);

        for (size_t i = 0; i < BankedEncoderCell::SceneManager::x_numScenes; ++i)
        {
            m_sceneChangeSelector->SetTrigger(i, m_sceneChange->AddTrigger());
        }

        m_trackSelector = m_ioMgr.AddTrigger("Track Selector", false);
        m_track = m_ioMgr.AddRandomAccessSwitch(&m_state.m_sceneManagerInput.m_track, false);
                
        for (size_t i = 0; i < 3; ++i)
        {
            m_trackSelector->SetTrigger(i, m_track->AddTrigger());
        }

        m_shift = m_ioMgr.AddInput("Shift", false);
        m_shift->SetTarget(0, &m_state.m_shift);

        m_sceneLight = m_ioMgr.AddOutput("Scene Light", false);
        for (size_t i = 0; i < BankedEncoderCell::SceneManager::x_numScenes; ++i)
        {
            m_sceneLight->SetSource(i, &m_bank.m_output.m_sceneLight[i]);
        }

        m_sceneLight->SetChannels(BankedEncoderCell::SceneManager::x_numScenes);

        m_revertToDefault = m_ioMgr.AddTrigger("Revert to Default Trigger", false);
        m_revertToDefault->SetTrigger(0, &m_state.m_revertToDefault);

        m_numTracks = m_ioMgr.AddParam("Num Tracks", 1, 16, 1, false);
        m_numTracks->SetTarget(0, &m_state.m_numTracks);
        m_numTracks->m_switch = true;

        m_numVoices = m_ioMgr.AddParam("Num Voices", 1, 16, 1, false);
        m_numVoices->SetTarget(0, &m_state.m_numVoices);
        m_numVoices->m_switch = true;

        m_saveJSONTrigger = m_ioMgr.AddTrigger("Save JSON", false);
        m_saveJSONTrigger->SetTrigger(0, &m_saveJSON);
        m_saveJSON = false;

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
                m_bank.GetBase(i, j)->SetNumTracks(m_state.m_numTracks);
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

            if (m_saveJSON)
            {
                SaveJSON();
                m_saveJSON = false;
            }
        }

        m_bank.ProcessStatic(args.sampleTime);
        m_bank.ProcessInput(m_state, m_ioMgr.IsControlFrame());
        m_ioMgr.SetOutputs();
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();
        json_t* encoders = json_array();

        if (m_savedJSON)
        {
            encoders = json_incref(m_savedJSON);
        }
        else
        {
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    json_array_append_new(encoders, m_bank.GetBase(i, j)->ToJSON());
                }
            }
        }

        json_object_set_new(rootJ, "encoders", encoders);
        return rootJ;
    }

    void SaveJSON()
    {
        json_decref(m_savedJSON);
        m_savedJSON = nullptr;
        std::ignore = dataToJson();
    }

    void dataFromJson(json_t* rootJ) override
    {
        json_t* encoders = json_object_get(rootJ, "encoders");

        if (encoders)
        {
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    m_bank.GetBase(i, j)->FromJSON(json_array_get(encoders, i * 4 + j));
                }
            }
        }

        m_savedJSON = json_incref(encoders);
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
                        module->m_modulationInputs[i + 4 * j]->Widget(this, 1 + i, 4 + j);
                    }
                }
            }

            module->m_gridId->Widget(this, 1, 10);
            module->m_blendFactor->Widget(this, 2, 10);
            module->m_sceneChangeSelector->Widget(this, 3, 10);
            module->m_trackSelector->Widget(this, 4, 10);
            module->m_shift->Widget(this, 5, 10);
            module->m_sceneLight->Widget(this, 6, 10);
            module->m_revertToDefault->Widget(this, 7, 10);
            module->m_saveJSONTrigger->Widget(this, 8, 10);

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

}
