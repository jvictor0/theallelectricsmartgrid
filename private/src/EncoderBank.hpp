#pragma once

#include "Encoder.hpp"
#include "BitSet.hpp"
#include "Filter.hpp"
#include "MessageIn.hpp"
#include "EncoderUIState.hpp"
#include "FixedAllocator.hpp"

namespace SmartGrid
{

struct EncoderBankInternal;

struct BankedEncoderCell : public StateEncoderCell
{
    static constexpr size_t x_encoderPoolSize = 512;
    static SegmentedAllocator<BankedEncoderCell, x_encoderPoolSize> s_allocator;

    using Ptr = PoolPtr<BankedEncoderCell, x_encoderPoolSize>;

    template <typename... Args>
    static Ptr Make(Args&&... args)
    {
        return MakePooled(s_allocator, std::forward<Args>(args)...);
    }

    struct ModulatorValues;
    
    struct SharedEncoderState : public SharedEncoderStateBase
    {
        size_t m_numVoices;
        ModulatorValues* m_modulatorValues;

        SharedEncoderState()
            : SharedEncoderStateBase()
            , m_numVoices(0)
            , m_modulatorValues(nullptr)
        {
        }
    };

    enum class EncoderType
    {
        BaseParam,
        ModulatorAmount,
        GestureParam
    };

    static constexpr size_t x_numModulators = 15;
    static constexpr size_t x_numGestureParams = 16;

    struct ModulatorValues
    {
        ModulatorValues()
            : m_value{}
            , m_valuePrev{}
            , m_modulatorConnected{}
            , m_modulatorColor{}
            , m_gestureWeights{}
            , m_gestureWeightsPrev{}
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    m_value[i][j] = 0;
                    m_valuePrev[i][j] = 0;
                }

                m_modulatorColor[i] = Color::Off;
                m_modulatorConnected[i] = false;
            }

            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                m_gestureWeights[i] = 0;
                m_gestureWeightsPrev[i] = 0;
            }
        }

        void ComputeChanged()
        {
            m_changedModulators.Clear();
            m_changedGestures.Clear();
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (memcmp(m_value[i], m_valuePrev[i], 16 * sizeof(float)) != 0)
                {
                    m_changedModulators.Set(i, true);
                    memcpy(m_valuePrev[i], m_value[i], 16 * sizeof(float));
                }
            }

            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (m_gestureWeights[i] != m_gestureWeightsPrev[i])
                {
                    m_changedGestures.Set(i, true);
                    m_gestureWeightsPrev[i] = m_gestureWeights[i];
                }
            }

            if (m_selectedGestures != m_selectedGesturesPrev)
            {
                for (size_t i = 0; i < x_numGestureParams; ++i)
                {
                    if (m_selectedGestures.Get(i) != m_selectedGesturesPrev.Get(i))
                    {
                        m_changedGestures.Set(i, true);
                    }
                }

                m_selectedGesturesPrev = m_selectedGestures;
            }                    
        }

        void SetModulatorColor(size_t index, Color color)
        {
            m_modulatorColor[index] = color;
            m_modulatorConnected[index] = true;
        }
        
        float m_value[x_numModulators][16];
        float m_valuePrev[x_numModulators][16];
        bool m_modulatorConnected[x_numModulators];
        Color m_modulatorColor[x_numModulators];

        float m_gestureWeights[x_numGestureParams];
        float m_gestureWeightsPrev[x_numGestureParams];

        BitSet16 m_selectedGestures;
        BitSet16 m_selectedGesturesPrev;

        BitSet16 m_changedModulators;
        BitSet16 m_changedGestures;
    };

    virtual ~BankedEncoderCell()
    {
    }
    
    struct Modulators
    {
        Ptr m_modulators[x_numModulators];
        Ptr m_gestures[x_numGestureParams];
        int m_activeModulators[x_numModulators];
        size_t m_numActiveModulators;
        BankedEncoderCell* m_owner;

        Modulators(BankedEncoderCell* owner)
            : m_modulators{}
            , m_gestures{}
            , m_activeModulators{}
            , m_numActiveModulators(0)
            , m_owner(owner)
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                m_modulators[i] = nullptr;
                m_activeModulators[i] = -1;
            }

            m_numActiveModulators = 0;
        }

        void FillModulators(BankedEncoderCell* parent, SceneManager* sceneManager)
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                if (!m_modulators[i].get())
                {
                    m_modulators[i] = Make(sceneManager, parent, i, BankedEncoderCell::EncoderType::ModulatorAmount);
                    m_modulators[i]->m_numTracks = parent->m_numTracks;
                    m_activeModulators[m_numActiveModulators] = i;
                    ++m_numActiveModulators;
                }
            }
        }

        void AddGesture(BankedEncoderCell* parent, size_t gestureIx)
        {
            if (!m_gestures[gestureIx].get())
            {
                m_gestures[gestureIx] = Make(parent->m_sceneManager, parent, gestureIx, BankedEncoderCell::EncoderType::GestureParam);
                m_gestures[gestureIx]->m_numTracks = parent->m_numTracks;
            }
        }

        void ClearAll()
        {
            for (size_t i = 0; i < x_numModulators; ++i)
            {
                m_modulators[i] = nullptr;
                m_activeModulators[i] = -1;
            }

            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                m_gestures[i] = nullptr;
            }

            m_numActiveModulators = 0;
        }

        BankedEncoderCell* GetModulator(size_t i)
        {
            return m_modulators[m_activeModulators[i]].get();
        }
            
        void GarbageCollect()
        {
            for (int i = m_numActiveModulators - 1; i >= 0; --i)
            {
                GetModulator(i)->GarbageCollectModulators();
                if (GetModulator(i)->CanBeGarbageCollected())
                {
                    m_modulators[m_activeModulators[i]] = nullptr;
                    m_activeModulators[i] = m_activeModulators[m_numActiveModulators - 1];
                    m_activeModulators[m_numActiveModulators - 1] = -1;
                    --m_numActiveModulators;
                }
            }

            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (m_gestures[i] && m_gestures[i]->CanBeGarbageCollected())
                {
                    m_gestures[i] = nullptr;
                }
            }
        }

        void ComputePostGestureValues(
            ModulatorValues* modulatorValues,
            size_t currentTrack)
        {            
            size_t numTracks = m_owner->m_sharedEncoderState->m_numTracks;

            double gestureWeightSum[16];
            double gestureValue[16];
            for (size_t i = 0; i < numTracks; ++i)
            {
                gestureWeightSum[i] = 0;
                gestureValue[i] = 0;
            }

            float currentGestureWeight = 0;
            
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (m_gestures[i])
                {
                    BankedEncoderCell* cell = m_gestures[i].get();
                    cell->Compute();
                    for (size_t j = 0; j < numTracks; ++j)
                    {
                        cell->SetEffectiveModulatorWeight(modulatorValues->m_gestureWeights[i], j);
                        double w = cell->m_effectiveModulatorWeights[j];
                        if (m_owner->GetSelectedGestures().Get(i) && j == currentTrack)
                        {
                            currentGestureWeight = w;
                        }

                        gestureWeightSum[j] += w;
                        float bankedValue = m_owner->m_bankedValue[j];
                        gestureValue[j] += w * static_cast<double>(bankedValue * (1 - w) + static_cast<double>(cell->m_bankedValue[j]) * w);
                    }
                }                
            }

            for (size_t i = 0; i < numTracks; ++i)
            {
                m_owner->m_postGestureValue[i] = gestureWeightSum[i] > 0 ? static_cast<float>(gestureValue[i] / gestureWeightSum[i]) : m_owner->m_bankedValue[i];
                m_owner->m_gestureWeightSum[i] = static_cast<float>(gestureWeightSum[i]);
            }
        }

        void Compute(
            ModulatorValues* modulatorValues,
            size_t track)
        { 
            size_t numTracks = m_owner->m_sharedEncoderState->m_numTracks;
            size_t numVoices = m_owner->GetSharedEncoderState()->m_numVoices;

            float modValue[16];
            float modWeight[16];
            for (size_t i = 0; i < numTracks * numVoices; ++i)
            {
                modValue[i] = 0;
                modWeight[i] = 0;
            }
            
            for (size_t i = 0; i < m_numActiveModulators; ++i)
            {
                if (GetModulator(i)->m_modulatorsAffecting.IsZero())
                {
                    break;
                }

                BankedEncoderCell* cell = GetModulator(i);
                cell->Compute();
                for (size_t j = 0; j < numTracks; ++j)
                {
                    for (size_t k = 0; k < numVoices; ++k)
                    {
                        size_t ix = j * numVoices + k;
                        modValue[ix] += cell->m_output[ix] * modulatorValues->m_value[m_activeModulators[i]][ix];
                        modWeight[ix] += cell->m_output[ix];
                    }
                }                
            }

            for (size_t i = 0; i < numTracks; ++i)
            {
                float value = m_owner->m_postGestureValue[i];
                for (size_t j = 0; j < numVoices; ++j)
                {
                    size_t ix = i * numVoices + j;
                    if (modWeight[ix] > 1)
                    {
                        m_owner->m_output[ix] = modValue[ix] / modWeight[ix];
                        m_owner->m_maxValue[ix] = 1;
                        m_owner->m_minValue[ix] = 0;
                    }
                    else
                    {
                        m_owner->m_output[ix] = value * (1 - modWeight[ix]) + modValue[ix];
                        m_owner->m_maxValue[ix] = value * (1 - modWeight[ix]) + modWeight[ix];
                        m_owner->m_minValue[ix] = value * (1 - modWeight[ix]);
                    }
                }
            }

            float brightnessVal = 1 - modWeight[numVoices * track];

            m_owner->m_brightness = std::max(0.0f, std::min(1.0f, brightnessVal));
        }

        void SetAllStates()
        {
            for (size_t i = 0; i < m_numActiveModulators; ++i)
            {
                GetModulator(i)->SetStateRecursive();
            }

            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (m_gestures[i])
                {
                    m_gestures[i]->SetStateRecursive();
                }
            }
        }
    };
    
    BankedEncoderCell()
        : StateEncoderCell()
        , m_parent(nullptr)
        , m_ownerBank(nullptr)
        , m_brightness(1)
        , m_connected(false)
        , m_modulators(this)
        , m_depth(0)
        , m_index(0)
        , m_defaultValue(0)
        , m_isVisible(false)
        , m_type(EncoderType::BaseParam)
        , m_forceUpdate(false)
        , m_gestureWeightSum{}
        , m_bankedValue{}
        , m_postGestureValue{}
        , m_output{}
        , m_maxValue{}
        , m_minValue{}
        , m_effectiveModulatorWeights{}
        , m_isActive{}
    {
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            for (size_t j = 0; j < 16; ++j)
            {
                m_isActive[i][j] = false;
            }
        }

        for (size_t i = 0; i < 16; ++i)
        {
            m_gestureWeightSum[i] = 1;
            m_bankedValue[i] = 0;
            m_output[i] = 0;
            m_effectiveModulatorWeights[i] = 0;
        }

        m_modulatorsAffecting.Clear();
        m_gesturesAffecting.Clear();
        for (size_t i = 0; i < 16; ++i)
        {
            m_modulatorsAffectingPerTrack[i].Clear();
            m_gesturesAffectingPerTrack[i].Clear();
        }
    }

    BankedEncoderCell(
        SceneManager* sceneManager, BankedEncoderCell* parent, int index, EncoderType modulatorType)
        : StateEncoderCell(sceneManager, parent ? parent->m_sharedEncoderState : nullptr)
        , m_modulators(this)
        , m_defaultValue(0)
        , m_gestureWeightSum{}
        , m_bankedValue{}
        , m_postGestureValue{}
        , m_output{}
        , m_maxValue{}
        , m_minValue{}
        , m_effectiveModulatorWeights{}
        , m_isActive{}
    {
        int depth = parent ? parent->m_depth + 1 : 0;
        m_parent = parent;
        m_ownerBank = parent ? parent->m_ownerBank : nullptr;
        m_type = modulatorType;

        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
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
                m_color = GetSharedEncoderState()->m_modulatorValues->m_modulatorColor[index];
                m_connected = GetSharedEncoderState()->m_modulatorValues->m_modulatorConnected[index];
            }
            else
            {
                m_color = GetGestureColor(depth, index);
                m_connected = true;
            }
        }
        else
        {
            m_connected = false;
        }
        
        m_brightness = 1;
        for (size_t i = 0; i < 16; ++i)
        {
            m_gestureWeightSum[i] = 1;
        }

        m_depth = depth;
        m_index = index;
        m_isVisible = false;
        m_modulatorsAffecting.Clear();
        m_gesturesAffecting.Clear();
        m_forceUpdate = true;
        for (size_t i = 0; i < 16; ++i)
        {
            m_bankedValue[i] = 0;
            SetStatePtr(&m_bankedValue[i], i);
            m_output[i] = 0;
            m_effectiveModulatorWeights[i] = 0;
        }
    }

    static Color GetGestureColor(int depth, int index)
    {
        std::ignore = index;
        return Color::FromTwister((124 + 67 * depth) / 2);
    }

    void SetForceUpdateRecursive()
    {
        m_forceUpdate = true;
        if (m_parent)
        {
            m_parent->SetForceUpdateRecursive();
        }
    }

    virtual void Increment(float delta) override
    {
        if (m_type != EncoderType::GestureParam && !GetSelectedGestures().IsZero())
        {
            // Add all selected gestures
            //
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (GetSelectedGestures().Get(i))
                {
                    m_modulators.AddGesture(this, i);
                    BankedEncoderCell* gestureCell = m_modulators.m_gestures[i].get();
                    if (!gestureCell->IsActiveBothScenes())
                    {
                        gestureCell->SetActive(true);
                        SetModulatorsAffectingRecursive();
                    }
                }
            }
        }

        float mainWeight = 0;
        float gestureWeightSum = m_gestureWeightSum[m_sharedEncoderState->m_currentTrack];
        if (gestureWeightSum > 0)
        {
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                BankedEncoderCell* gestureCell = m_modulators.m_gestures[i].get();
                if (gestureCell)
                {
                    float weight = gestureCell->m_effectiveModulatorWeights[m_sharedEncoderState->m_currentTrack];
                    gestureCell->IncrementInternal(delta * weight * weight / gestureWeightSum);
                    gestureCell->m_forceUpdate = true;
                    mainWeight += weight * (1 - weight);
                }
            }

            IncrementInternal(delta * mainWeight / gestureWeightSum);
            m_forceUpdate = true;
        }
        else
        {
            IncrementInternal(delta);
        }

        SetForceUpdateRecursive();        
    }

    void SetEffectiveModulatorWeight(float weight, int track)
    {
        float w1 = m_isActive[m_sceneManager->m_scene1][track] ? weight : 0;
        float w2 = m_isActive[m_sceneManager->m_scene2][track] ? weight : 0;
        m_effectiveModulatorWeights[track] = w1 * (1 - m_sceneManager->m_blendFactor) + w2 * m_sceneManager->m_blendFactor;
    }

    void SetDefaultValue()
    {
        m_defaultValue = m_bankedValue[m_sharedEncoderState->m_currentTrack];
    }

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        ZeroModulators(allScenes, allTracks);
        SetValue(m_defaultValue, allScenes, allTracks);
        SetForceUpdateRecursive();
        SetModulatorsAffecting();
    }

    Color GetSquareColor()
    {
        if (!GetSelectedGestures().IsZero() && m_type != EncoderType::GestureParam)
        {
            // Use the first selected gesture's color
            //
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (GetSelectedGestures().Get(i))
                {
                    if (m_modulators.m_gestures[i] && m_modulators.m_gestures[i]->IsActiveForCurrentTrack())
                    {
                        return m_modulators.m_gestures[i]->GetSquareColor();
                    }
                    break;
                }
            }

            return m_color;
        }
        else
        {
            return m_color;
        }
    }

    float GetBrightness()
    {
        if (m_sceneManager->m_blinker.m_blink && 
            m_type == EncoderType::BaseParam &&
            GetSelectedGestures().IsZero())
        {
            return (1 - m_gestureWeightSum[m_sharedEncoderState->m_currentTrack]) * m_brightness;
        }
        else
        {
            return m_brightness;
        }
    }

    virtual uint8_t GetAnimationValue() override
    {
        return BrightnessToAnimationValue(m_brightness);
    }

    virtual float GetNormalizedValue() override
    {
        return m_output[GetSharedEncoderState()->m_numVoices * m_sharedEncoderState->m_currentTrack];
    }

    BankedEncoderCell* GetModulator(size_t i)
    {
        return m_modulators.GetModulator(i);
    }

    void CopyToScene(size_t scene)
    {
        StateEncoderCell::CopyToScene(scene);
        for (size_t i = 0; i < 16; ++i)
        {
            m_isActive[scene][i] = IsActiveForTrack(i);
        }

        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            GetModulator(i)->CopyToScene(scene);
        }

        for (size_t i = 0; i < x_numGestureParams; ++i)
        {
            if (m_modulators.m_gestures[i])
            {
                m_modulators.m_gestures[i]->CopyToScene(scene);
            }
        }
    }

    void HandleShiftPress()
    {
        if (GetSelectedGestures().IsZero())
        {
            ZeroModulators(false, false);
        }
        else
        {
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (GetSelectedGestures().Get(i))
                {
                    BankedEncoderCell* gestureCell = m_modulators.m_gestures[i].get();
                    if (gestureCell)
                    {
                        gestureCell->DeactivateGestureCurrentScene();
                    }
                }
            }
        }

        SetModulatorsAffectingRecursive();
    }

    void ZeroModulators(bool allScenes, bool allTracks)
    {
        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            GetModulator(i)->SetValue(0, allScenes, allTracks);
            GetModulator(i)->ZeroModulators(allScenes, allTracks);
        }

        for (size_t i = 0; i < x_numGestureParams; ++i)
        {
            if (m_modulators.m_gestures[i])
            {
                m_modulators.m_gestures[i]->SetValue(0, allScenes, allTracks);
                DeactivateGesture(i, allScenes, allTracks);
            }
        }

        GarbageCollectModulators();
    }

    void DeactivateGesture(size_t gestureIx, bool allScenes, bool allTracks)
    {
        if (!m_modulators.m_gestures[gestureIx])
        {
            return;
        }

        size_t startTrack = allTracks ? 0 : m_sharedEncoderState->m_currentTrack;
        size_t endTrack = allTracks ? m_numTracks : m_sharedEncoderState->m_currentTrack + 1;

        for (size_t t = startTrack; t < endTrack; ++t)
        {
            for (size_t s = 0; s < SceneManager::x_numScenes; ++s)
            {
                if (!allScenes && !m_sceneManager->IsSceneActive(s))
                {
                    continue;
                }

                m_modulators.m_gestures[gestureIx]->m_isActive[s][t] = false;
            }
        }
    }

    bool CanBeGarbageCollected()
    {
        if (m_type == EncoderType::GestureParam)
        {
            for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
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
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                if (m_modulators.m_gestures[i])
                {
                    return false;
                }
            }

            return m_modulators.m_numActiveModulators == 0 && AllZero();
        }
    }

    void Compute()
    {
        ModulatorValues* modulatorValues = GetSharedEncoderState()->m_modulatorValues;
        if (!m_connected)
        {
            m_brightness = 0;
            return;
        }
        else if (m_forceUpdate || 
                !m_modulatorsAffecting.Intersect(modulatorValues->m_changedModulators).IsZero() ||
                !m_gesturesAffecting.Intersect(modulatorValues->m_changedGestures).IsZero())
        {
            if (m_forceUpdate || !m_gesturesAffecting.Intersect(modulatorValues->m_changedGestures).IsZero())
            {
                m_modulators.ComputePostGestureValues(modulatorValues, m_sharedEncoderState->m_currentTrack);
            }

            m_modulators.Compute(modulatorValues, m_sharedEncoderState->m_currentTrack);
        }
        
        if (m_depth > 0)
        {
            if (m_type == EncoderType::ModulatorAmount)
            {
                m_brightness = std::max(0.0f, std::min(1.0f, modulatorValues->m_value[m_index][m_sharedEncoderState->m_currentTrack * GetSharedEncoderState()->m_numVoices]));
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

        JSON gesturesJ = JSON::Array();
        bool hasGestures = false;
        for (size_t i = 0; i < x_numGestureParams; ++i)
        {
            if (m_modulators.m_gestures[i])
            {
                hasGestures = true;
                gesturesJ.AppendNew(m_modulators.m_gestures[i]->ToJSON());
            }
            else
            {
                gesturesJ.AppendNew(JSON::Null());
            }
        }

        if (hasGestures)
        {
            rootJ.SetNew("gestures", gesturesJ);
        }

        if (m_type == EncoderType::GestureParam)
        {
            JSON activeJ = JSON::Array();
            for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    activeJ.AppendNew(JSON::Boolean(m_isActive[i][j]));
                }
            }

            rootJ.SetNew("active", activeJ);
        }

        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON valuesJ = rootJ.Get("values");
        if (!valuesJ.IsNull())
        {
            StateEncoderCell::FromJSON(valuesJ);
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
                        m_modulators.m_modulators[i] = Make(m_sceneManager, this, i, BankedEncoderCell::EncoderType::ModulatorAmount);
                        m_modulators.m_modulators[i]->FromJSON(modulatorJ);
                        m_modulators.m_activeModulators[m_modulators.m_numActiveModulators] = i;
                        ++m_modulators.m_numActiveModulators;
                    }
                }
            }
        }

        JSON gesturesJ = rootJ.Get("gestures");
        if (!gesturesJ.IsNull())
        {
            for (size_t i = 0; i < x_numGestureParams; ++i)
            {
                m_modulators.m_gestures[i] = Make(m_sceneManager, this, i, BankedEncoderCell::EncoderType::GestureParam);
                m_modulators.m_gestures[i]->FromJSON(gesturesJ.GetAt(i));
            }
        }

        JSON activeJ = rootJ.Get("active");
        if (!activeJ.IsNull())
        {
            for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
            {
                for (size_t j = 0; j < 16; ++j)
                {
                    m_isActive[i][j] = activeJ.GetAt(i * 16 + j).BooleanValue();
                }
            }
        }

        GarbageCollectModulators();
        if (m_depth == 0)
        {
            SetModulatorsAffecting();
        }

        m_forceUpdate = true;
    }

    void FillModulators(SceneManager* sceneManager)
    {
        m_modulators.FillModulators(this, sceneManager);
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
        m_forceUpdate = true;
    }

    bool IsActiveForTrack(size_t track)
    {
        if (m_type != EncoderType::GestureParam)
        {
            return true;
        }
        else
        {
            return (m_sceneManager->Scene2Active() && m_isActive[m_sceneManager->m_scene2][track])
                || (m_sceneManager->Scene1Active() && m_isActive[m_sceneManager->m_scene1][track]);
        }
    }

    bool IsActiveForCurrentTrack()
    {
        return IsActiveForTrack(m_sharedEncoderState->m_currentTrack);
    }

    bool IsActiveBothScenes()
    {
        return (!m_sceneManager->Scene2Active() || m_isActive[m_sceneManager->m_scene2][m_sharedEncoderState->m_currentTrack])
            && (!m_sceneManager->Scene1Active() || m_isActive[m_sceneManager->m_scene1][m_sharedEncoderState->m_currentTrack]);
    }

    void ToggleActive()
    {
        if (m_sceneManager->Scene2Active())
        {
            m_isActive[m_sceneManager->m_scene2][m_sharedEncoderState->m_currentTrack] = !m_isActive[m_sceneManager->m_scene2][m_sharedEncoderState->m_currentTrack];
        }
        
        if (m_sceneManager->Scene1Active())
        {
            m_isActive[m_sceneManager->m_scene1][m_sharedEncoderState->m_currentTrack] = !m_isActive[m_sceneManager->m_scene1][m_sharedEncoderState->m_currentTrack];
        }
    }

    void SetActive(bool active)
    {
        if (m_sceneManager->Scene2Active())
        {
            if (active && !m_isActive[m_sceneManager->m_scene2][m_sharedEncoderState->m_currentTrack])
            {
                m_values[m_sharedEncoderState->m_currentTrack][m_sceneManager->m_scene2] = m_parent->m_values[m_sharedEncoderState->m_currentTrack][m_sceneManager->m_scene2];
                SetStateForTrack(m_sharedEncoderState->m_currentTrack);
            }

            m_isActive[m_sceneManager->m_scene2][m_sharedEncoderState->m_currentTrack] = active;
        }
        
        if (m_sceneManager->Scene1Active())
        {
            if (active && !m_isActive[m_sceneManager->m_scene1][m_sharedEncoderState->m_currentTrack])
            {
                m_values[m_sharedEncoderState->m_currentTrack][m_sceneManager->m_scene1] = m_parent->m_values[m_sharedEncoderState->m_currentTrack][m_sceneManager->m_scene1];
                SetStateForTrack(m_sharedEncoderState->m_currentTrack);
            }

            m_isActive[m_sceneManager->m_scene1][m_sharedEncoderState->m_currentTrack] = active;
        }
    }

    void DeactivateGestureCurrentScene()
    {
        if (m_sceneManager->Scene2Active())
        {
            m_isActive[m_sceneManager->m_scene2][m_sharedEncoderState->m_currentTrack] = false;
        }
        
        if (m_sceneManager->Scene1Active())
        {
            m_isActive[m_sceneManager->m_scene1][m_sharedEncoderState->m_currentTrack] = false;
        }
    }

    // Recursively deactivates the specified gesture in all tracks for the current scene(s)
    //
    void ClearGesture(int gesture)
    {
        if (m_modulators.m_gestures[gesture])
        {
            BankedEncoderCell* gestureCell = m_modulators.m_gestures[gesture].get();
            for (size_t track = 0; track < m_numTracks; ++track)
            {
                if (m_sceneManager->Scene2Active())
                {
                    gestureCell->m_isActive[m_sceneManager->m_scene2][track] = false;
                }

                if (m_sceneManager->Scene1Active())
                {
                    gestureCell->m_isActive[m_sceneManager->m_scene1][track] = false;
                }
            }
        }

        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            GetModulator(i)->ClearGesture(gesture);
        }
    }

    void SetModulatorsAffecting()
    {
        m_modulatorsAffecting.Clear();
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            m_modulatorsAffectingPerTrack[i].Clear();
        }

        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            GetModulator(i)->SetModulatorsAffecting();
            m_modulatorsAffecting = GetModulator(i)->m_modulatorsAffecting.Union(m_modulatorsAffecting);
            
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                m_modulatorsAffectingPerTrack[j] = GetModulator(i)->m_modulatorsAffectingPerTrack[j].Union(m_modulatorsAffectingPerTrack[j]);
            }
        }

        if (m_depth > 0)
        {
            if (!m_modulatorsAffecting.IsZero() || !IsZeroCurrentScene() || m_isVisible)
            {
                m_modulatorsAffecting.Set(m_index, true);
            }

            for (size_t i = 0; i < m_numTracks; ++i)
            {
                if (!m_modulatorsAffectingPerTrack[i].IsZero() || !IsZeroCurrentSceneForTrack(i))
                {
                    m_modulatorsAffectingPerTrack[i].Set(m_index, true);
                }
            }
        }

        int lastAffectedModulator = m_modulators.m_numActiveModulators - 1;
        for (int i = m_modulators.m_numActiveModulators - 1; i >= 0; --i)
        {
            if (GetModulator(i)->m_modulatorsAffecting.IsZero())
            {
                std::swap(m_modulators.m_activeModulators[i], m_modulators.m_activeModulators[lastAffectedModulator]);
                --lastAffectedModulator;
            }
        }

        m_gesturesAffecting.Clear();
        for (size_t j = 0; j < 16; ++j)
        {
            m_gesturesAffectingPerTrack[j].Clear();
        }

        for (size_t i = 0; i < x_numGestureParams; ++i)
        {
            if (m_modulators.m_gestures[i])
            {
                for (size_t j = 0; j < m_numTracks; ++j)
                {
                    if (m_modulators.m_gestures[i]->IsActiveForTrack(j))
                    {
                        m_gesturesAffecting.Set(i, true);
                        m_gesturesAffectingPerTrack[j].Set(i, true);
                        if (m_type == EncoderType::ModulatorAmount)
                        {
                            m_modulatorsAffecting.Set(m_index, true);
                            m_modulatorsAffectingPerTrack[j].Set(m_index, true);
                        }
                    }
                }
            }
        }

        for (size_t i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            m_gesturesAffecting = m_gesturesAffecting.Union(GetModulator(i)->m_gesturesAffecting);
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                m_gesturesAffectingPerTrack[j] = m_gesturesAffectingPerTrack[j].Union(GetModulator(i)->m_gesturesAffectingPerTrack[j]);
            }
        }
    }

    void PrintStateRecursive(int indent)
    {
        std::string indentString(indent, ' ');
        if (m_depth == 0)
        {
            INFO("--------------------------------");
        }

        INFO("%sEncoder %d banked value: %f modulators affecting 0x%04x (0x%04x), gestures affecting 0x%04x (0x%04x)", 
            indentString.c_str(), m_index, m_bankedValue[m_sharedEncoderState->m_currentTrack], 
            m_modulatorsAffecting.m_bits, m_modulatorsAffectingPerTrack[m_sharedEncoderState->m_currentTrack].m_bits, 
            m_gesturesAffecting.m_bits, m_gesturesAffectingPerTrack[m_sharedEncoderState->m_currentTrack].m_bits);

        for (int i = 0; i < m_modulators.m_numActiveModulators; ++i)
        {
            GetModulator(i)->PrintStateRecursive(indent + 2);
        }

        if (m_depth == 0)
        {
            INFO("--------------------------------");
        }
    }

    void SetModulatorsAffectingRecursive();

    SharedEncoderState* GetSharedEncoderState()
    {
        return static_cast<SharedEncoderState*>(m_sharedEncoderState);
    }

    BitSet16 GetSelectedGestures()
    {
        return GetSharedEncoderState()->m_modulatorValues->m_selectedGestures;
    }
        
    BankedEncoderCell* m_parent;
    EncoderBankInternal* m_ownerBank;
    Color m_color;
    float m_gestureWeightSum[16];
    float m_brightness;
    bool m_connected;
    Modulators m_modulators;
    int m_depth;
    int m_index;
    float m_bankedValue[16];
    float m_postGestureValue[16];
    float m_output[16];
    float m_maxValue[16];
    float m_minValue[16];
    float m_defaultValue;
    float m_effectiveModulatorWeights[16];
    BitSet16 m_modulatorsAffecting;
    BitSet16 m_modulatorsAffectingPerTrack[16];
    BitSet16 m_gesturesAffecting;
    BitSet16 m_gesturesAffectingPerTrack[16];
    bool m_isVisible;
    BankedEncoderCell::EncoderType m_type;
    bool m_isActive[SceneManager::x_numScenes][16];
    bool m_forceUpdate;
};

inline SegmentedAllocator<BankedEncoderCell, BankedEncoderCell::x_encoderPoolSize> BankedEncoderCell::s_allocator;

using EncoderPtr = PoolPtr<BankedEncoderCell, BankedEncoderCell::x_encoderPoolSize>;

template <typename... Args>
EncoderPtr MakeEncoder(Args&&... args)
{
    return MakePooled(BankedEncoderCell::s_allocator, std::forward<Args>(args)...);
}

struct EncoderBankInternal : public EncoderGrid
{
    SceneManager* m_sceneManager;
    EncoderPtr m_baseCell[4][4];
    BankedEncoderCell* m_selected;
    BulkFilter<16 * 16> m_bulkFilter;
    size_t m_totalVoices;
    size_t m_activeEncoderPrefix;
    BankedEncoderCell::SharedEncoderState m_sharedEncoderState;
    BitSet16 m_gesturesAffectingPerTrack[16];
    BitSet16 m_gesturesAffecting;

    void SetVisibleCell(int x, int y, BankedEncoderCell* cell)
    {
        BankedEncoderCell* current = static_cast<BankedEncoderCell*>(GetVisible(x, y));
        if (current)
        {
            current->m_isVisible = false;
        }
        
        SetVisible(x, y, cell);
        cell->m_isVisible = true;
    }
    
    EncoderBankInternal()
        : m_sceneManager(nullptr)
        , m_selected(nullptr)
        , m_totalVoices(0)
        , m_activeEncoderPrefix(0)
    {
    }

    void Init(SceneManager* sceneManager, BankedEncoderCell::ModulatorValues* modulatorValues, size_t numTracks, size_t numVoices)
    {
        m_sceneManager = sceneManager;
        m_sharedEncoderState.m_modulatorValues = modulatorValues;
        m_sharedEncoderState.m_numTracks = numTracks;
        m_sharedEncoderState.m_numVoices = numVoices;
        m_totalVoices = numVoices * numTracks;

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j] = MakeEncoder(m_sceneManager, nullptr, i + 4 * j, BankedEncoderCell::EncoderType::BaseParam);
                m_baseCell[i][j]->m_sharedEncoderState = &m_sharedEncoderState;
                m_baseCell[i][j]->m_ownerBank = this;
                SetVisibleCell(i, j, m_baseCell[i][j].get());
            }
        }

        SetNumTracks(numTracks);
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

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                m_baseCell[i][j]->RevertToDefault(allScenes, allTracks);
            }
        }
    }

    BankedEncoderCell* GetBase(int i, int j)
    {
        return m_baseCell[i][j].get();
    }

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

        ComputeGesturesAffectingPerTrack();
    }

    void ComputeGesturesAffectingPerTrack()
    {
        m_gesturesAffecting.Clear();
        for (size_t t = 0; t < 16; ++t)
        {
            m_gesturesAffectingPerTrack[t].Clear();
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    m_gesturesAffectingPerTrack[t] = m_gesturesAffectingPerTrack[t].Union(GetBase(i, j)->m_gesturesAffectingPerTrack[t]);                    
                    m_gesturesAffecting = m_gesturesAffecting.Union(GetBase(i, j)->m_gesturesAffectingPerTrack[t]);
                }
            }
        }
    }

    BitSet16 GetGesturesAffectingForTrack(size_t track)
    {
        return m_gesturesAffectingPerTrack[track];
    }

    BitSet16 GetGesturesAffecting()
    {
        return m_gesturesAffecting;
    }

    void ClearGesture(int gesture)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                GetBase(i, j)->ClearGesture(gesture);
            }
        }

        SetAllModulatorsAffecting();
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

    void SetTrack(size_t track)
    {
        m_sharedEncoderState.m_currentTrack = track;
    }

    void HandleChangedSceneManager()
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                GetBase(i, j)->SetStateRecursive();
            }
        }
    }

    void HandleChangedSceneManagerScene()
    {
        SetAllModulatorsAffecting();
    }

    void ProcessTopology()
    {
        m_activeEncoderPrefix = 0;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (GetBase(i, j)->m_connected)
                {
                    m_activeEncoderPrefix = i * 4 + j + 1;
                }

                GetBase(i, j)->Compute();
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

    void MakeSelection(int x, int y, BankedEncoderCell* cell)
    {
        cell->FillModulators(m_sceneManager);
        m_selected = cell;
        SetVisibleCell(3, 3, m_selected);
        for (size_t i = 0; i < BankedEncoderCell::x_numModulators; ++i)
        {
            auto xy = ModulatorIndexToXY(i);
            SetVisibleCell(xy.first, xy.second, m_selected->m_modulators.m_modulators[i].get());
        }

        m_selected->SetModulatorsAffecting();
    }

    void Deselect()
    {
        if (m_selected)
        {
            m_selected->GarbageCollectModulators();
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    SetVisibleCell(i, j, m_baseCell[i][j].get());
                    m_baseCell[i][j]->SetModulatorsAffecting();
                }
            }

            m_selected = nullptr;
        }
    }

    virtual void HandlePress(int x, int y) override
    {
        BankedEncoderCell* cell = static_cast<BankedEncoderCell*>(GetVisible(x, y));

        if (m_sceneManager->m_shift)
        {
            cell->HandleShiftPress();
        }
        else if (m_selected && x == 3 && y == 3)
        {
            Deselect();
        }
        else
        {
            MakeSelection(x, y, cell);
        }
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

        ComputeGesturesAffectingPerTrack();
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
            BankedEncoderCell* cell = static_cast<BankedEncoderCell*>(GetVisible(msg.m_x, msg.m_y));
            cell->HandleIncDec(msg.m_timestamp, msg.m_amount);
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
                BankedEncoderCell* cell = static_cast<BankedEncoderCell*>(GetVisible(i, j));

                if (cell->m_connected)
                {
                    uiState->SetConnected(i, j, true);
                    uiState->SetColor(i, j, cell->GetSquareColor());
                    uiState->SetBrightness(i, j, cell->GetBrightness());
                    for (size_t k = 0; k < m_sharedEncoderState.m_numTracks * m_sharedEncoderState.m_numVoices; ++k)
                    {
                        uiState->SetValue(i, j, k, cell->m_output[k]);
                        uiState->SetMinValue(i, j, k, cell->m_minValue[k]);
                        uiState->SetMaxValue(i, j, k, cell->m_maxValue[k]);
                    }

                    uiState->SetModulatorsAffecting(i, j, cell->m_modulatorsAffectingPerTrack[m_sharedEncoderState.m_currentTrack]);
                    uiState->SetGesturesAffecting(i, j, cell->m_gesturesAffectingPerTrack[m_sharedEncoderState.m_currentTrack]);
                }
                else
                {
                    uiState->SetConnected(i, j, false);
                    uiState->SetBrightness(i, j, 0);
                    for (size_t k = 0; k < m_sharedEncoderState.m_numTracks * m_sharedEncoderState.m_numVoices; ++k)
                    {
                        uiState->SetValue(i, j, k, 0);
                        uiState->SetMinValue(i, j, k, 0);
                        uiState->SetMaxValue(i, j, k, 0);
                    }

                    uiState->SetModulatorsAffecting(i, j, BitSet16());
                    uiState->SetGesturesAffecting(i, j, BitSet16());
                }
            }
        }

        uiState->SetNumTracks(m_sharedEncoderState.m_numTracks);
        uiState->SetNumVoices(m_sharedEncoderState.m_numVoices);
        uiState->SetCurrentTrack(m_sharedEncoderState.m_currentTrack);
    }

    int GetCurrentTrack()
    {
        return m_sharedEncoderState.m_currentTrack;
    }
};

inline void BankedEncoderCell::SetModulatorsAffectingRecursive()
{
    BankedEncoderCell* cell = this;
    while (cell->m_parent)
    {
        cell = cell->m_parent;
    }

    cell->SetModulatorsAffecting();
    m_ownerBank->ComputeGesturesAffectingPerTrack();
}

}
