#pragma once

#include "SmartGrid.hpp"
#include "SceneManager.hpp"

namespace SmartGrid
{

struct SharedEncoderStateBase
{
    size_t m_numTracks;
    size_t m_currentTrack;

    SharedEncoderStateBase()
        : m_numTracks(0)
        , m_currentTrack(0)
    {
    }
};

struct EncoderCell
{
    uint8_t m_lastVelocity;
    static constexpr float x_minSpeed = 0.001f;
    static constexpr float x_maxSpeed = 1.0f / 128.0f;
    static constexpr float x_pressSpeed = 0.005f;
    size_t m_lastTimestamp;
    int m_lastDeltaSign;
    float m_lastSpeed;

    EncoderCell()
        : m_lastVelocity(0)
        , m_lastTimestamp(0)
        , m_lastDeltaSign(0)
        , m_lastSpeed(x_minSpeed)
    {
    }
    
    virtual ~EncoderCell()
    {
    }

    void HandleIncDec(size_t timestamp, int64_t delta)
    {
        if (delta == 0)
        {
            return;
        }

        int currentSign = (delta > 0) ? 1 : -1;
        
        // Reset acceleration if direction changed or too much time passed
        //
        static constexpr size_t x_resetTimeUs = 200000;
        bool resetAcceleration = false;
        
        if (m_lastTimestamp == 0)
        {
            resetAcceleration = true;
        }
        else if (m_lastDeltaSign != 0 && currentSign != m_lastDeltaSign)
        {
            resetAcceleration = true;
        }
        else if (m_lastTimestamp < timestamp && x_resetTimeUs < (timestamp - m_lastTimestamp))
        {
            resetAcceleration = true;
        }

        float speed = x_minSpeed;
        
        if (resetAcceleration)
        {
            speed = x_minSpeed;
        }
        else if (m_lastTimestamp < timestamp)
        {
            // Calculate scaling factor based on time delta
            // Faster movements (shorter time) = higher scaling (accelerate)
            // Slower movements (longer time) = lower scaling (maintain speed)
            //
            size_t timeDeltaUs = timestamp - m_lastTimestamp;
            
            // Map time delta to scaling factor
            // 5ms (5000us) = 2.0x (double the speed)
            // 50ms (50000us) = 1.0x (keep same speed)
            //
            static constexpr size_t x_fastTimeUs = 5000;
            static constexpr size_t x_slowTimeUs = 50000;
            
            float scaleFactor = 1.0f;
            
            if (timeDeltaUs <= x_fastTimeUs)
            {
                scaleFactor = 2.0f;
            }
            else if (x_slowTimeUs <= timeDeltaUs)
            {
                scaleFactor = 1.0f;
            }
            else
            {
                // Linear interpolation from 2.0 to 1.0
                //
                float t = static_cast<float>(timeDeltaUs - x_fastTimeUs) / static_cast<float>(x_slowTimeUs - x_fastTimeUs);
                scaleFactor = 2.0f * (1.0f - t) + 1.0f * t;
            }
            
            // Apply scaling to last speed and clamp
            //
            speed = m_lastSpeed * scaleFactor;
            speed = std::max(x_minSpeed, std::min(x_maxSpeed, speed));
        }
        else
        {
            speed = m_lastSpeed;
        }
        
        m_lastTimestamp = timestamp;
        m_lastDeltaSign = currentSign;
        m_lastSpeed = speed;
        
        Increment(delta * speed);
    }

    virtual float GetNormalizedValue() = 0;

    virtual uint8_t GetTwisterColor()
    {
        return 64;
    }

    virtual void Increment(float delta) = 0;

    virtual uint8_t GetAnimationValue()
    {
        return 47;
    }

    static uint8_t BrightnessToAnimationValue(float brightness)
    {
        return 17 + brightness * 30;
    }
};

struct StateEncoderCell : public EncoderCell
{
    static constexpr size_t x_maxPoly = 16;

    void CopyToScene(size_t scene)
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            m_values[i][scene] = GetNormalizedValueForTrack(i);
        }

        SetState();
    }

    void ZeroCurrentScene()
    {
        size_t track = m_sharedEncoderState->m_currentTrack;
        if (m_sceneManager->m_blendFactor < 1)
        {
            m_values[track][m_sceneManager->m_scene1] = 0;
        }

        if (m_sceneManager->m_blendFactor > 0)
        {
            m_values[track][m_sceneManager->m_scene2] = 0;
        }

        SetStateForTrack(track);
    }

    bool IsZeroCurrentScene()
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            if (m_values[i][m_sceneManager->m_scene1] != 0 && m_sceneManager->m_blendFactor < 1)
            {
                return false;
            }

            if (m_values[i][m_sceneManager->m_scene2] != 0 && m_sceneManager->m_blendFactor > 0)
            {
                return false;
            }
        }

        return true;
    }

    bool IsZeroCurrentSceneForTrack(size_t track)
    {
        if (m_values[track][m_sceneManager->m_scene1] != 0 && m_sceneManager->m_blendFactor < 1)
        {
            return false;
        }
        
        if (m_values[track][m_sceneManager->m_scene2] != 0 && m_sceneManager->m_blendFactor > 0)
        {
            return false;
        }

        return true;
    }

    float m_values[x_maxPoly][SceneManager::x_numScenes];
    float* m_state[x_maxPoly];
    size_t m_numTracks;
    SceneManager* m_sceneManager;
    SharedEncoderStateBase* m_sharedEncoderState;

    JSON ToJSON()
    {
        JSON root = JSON::Object();
        JSON values = JSON::Array();
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            JSON sceneValues = JSON::Array();
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                sceneValues.AppendNew(JSON::Real(m_values[j][i]));
            }
            
            values.AppendNew(sceneValues);
        }
        
        root.SetNew("values", values);
        return root;
    }

    void FromJSON(JSON root)
    {
        JSON values = root.Get("values");
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            JSON sceneValues = values.GetAt(i);
            m_numTracks = sceneValues.Size();
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                m_values[j][i] = sceneValues.GetAt(j).RealValue();
            }
        }

        SetState();
    }
    
    StateEncoderCell()
        : m_numTracks(0)
        , m_sceneManager(nullptr)
        , m_sharedEncoderState(nullptr)
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            for (size_t j = 0; j < SceneManager::x_numScenes; ++j)
            {
                m_values[i][j] = 0;
            }
        }

        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_state[i] = nullptr;
        }
    }

    StateEncoderCell(SceneManager* sceneManager, SharedEncoderStateBase* sharedEncoderState)
        : m_numTracks(0)
        , m_sceneManager(sceneManager)
        , m_sharedEncoderState(sharedEncoderState)
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            for (size_t j = 0; j < SceneManager::x_numScenes; ++j)
            {
                m_values[i][j] = 0;
            }
        }

        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_state[i] = nullptr;
        }

        m_sceneManager->RegisterCell(this);
    }

    void SetNumTracks(size_t numTracks)
    {
        m_numTracks = numTracks;
    }

    void SetStatePtr(float* state, size_t track)
    {
        m_state[track] = state;
    }

    virtual ~StateEncoderCell()
    {
    }

    virtual float GetNormalizedValue() override
    {
        return GetNormalizedValueForTrack(m_sharedEncoderState->m_currentTrack);
    }

    float GetNormalizedValueForTrack(size_t track)
    {
        return m_sceneManager->GetSceneValue(m_values[track]);
    }

    bool AllZero()
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            for (size_t j = 0; j < SceneManager::x_numScenes; ++j)
            {
                if (m_values[i][j] != 0)
                {
                    return false;
                }
            }
        }

        return true;
    }

    float GetValue(size_t track)
    {
        return GetNormalizedValueForTrack(track);
    }

    void SetState()
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            SetStateForTrack(i);
        }
    }

    void SetStateForTrack(size_t track)
    {
        *m_state[track] = GetValue(track);
    }

    void IncrementInternal(float delta)
    {
        if (delta == 0)
        {
            return;
        }

        int s1 = m_sceneManager->m_scene1;
        int s2 = m_sceneManager->m_scene2;
        float t = m_sceneManager->m_blendFactor;
        size_t track = m_sharedEncoderState->m_currentTrack;
        if (t <= 0)
        {
            m_values[track][s1] = std::max(0.0f, std::min(1.0f, m_values[track][s1] + delta));
        }
        else if (t >= 1)
        {
            m_values[track][s2] = std::max(0.0f, std::min(1.0f, m_values[track][s2] + delta));
        }
        else
        {
            float value = std::max(0.0f, std::min(1.0f, GetNormalizedValueForTrack(track) + delta));
            float newValue1 = m_values[track][s1] + delta * (1.0f - t);
            float newValue2 = m_values[track][s2] + delta * t;
            if (newValue1 < 0 || newValue1 > 1)
            {
                m_values[track][s1] = std::max(0.0f, std::min(1.0f, newValue1));
                m_values[track][s2] = (value - m_values[track][s1] * (1 - t)) / t;
            }
            else if (newValue2 < 0 || newValue2 > 1)
            {
                m_values[track][s2] = std::max(0.0f, std::min(1.0f, newValue2));
                m_values[track][s1] = (value - m_values[track][s2] * t) / (1 - t);
            }
            else
            {
                m_values[track][s1] = newValue1;
                m_values[track][s2] = newValue2;
            }
        }

        SetStateForTrack(track);
    }

    void SetToValue(float value)
    {
        float delta = value - GetNormalizedValueForTrack(m_sharedEncoderState->m_currentTrack);
        IncrementInternal(delta);
    }

    void SetValueAllScenesAllTracks(float value)
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            for (size_t j = 0; j < SceneManager::x_numScenes; ++j)
            {
                m_values[i][j] = value;
            }

            SetStateForTrack(i);
        }
    }

    void SetValue(float value, bool allScenes, bool allTracks)
    {
        size_t startTrack = allTracks ? 0 : m_sharedEncoderState->m_currentTrack;
        size_t endTrack = allTracks ? m_numTracks : m_sharedEncoderState->m_currentTrack + 1;

        for (size_t t = startTrack; t < endTrack; ++t)
        {
            for (size_t s = 0; s < SceneManager::x_numScenes; ++s)
            {
                int scene = s;
                if (allScenes)
                {
                    if (s == 0)
                    {
                        scene = m_sceneManager->m_scene1;
                        if (!m_sceneManager->Scene1Active())
                        {
                            continue;
                        }
                    }
                    else if (s == 1)
                    {
                        scene = m_sceneManager->m_scene2;
                        if (!m_sceneManager->Scene2Active())
                        {
                            continue;
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                m_values[t][scene] = value;
            }

            SetStateForTrack(t);
        }
    }
};

struct EncoderGrid
{
    static constexpr int x_width = 4;
    static constexpr int x_height = 4;

    EncoderCell* m_visibleCell[x_width][x_height];

    EncoderGrid()
    {
        for (int i = 0; i < x_width; ++i)
        {
            for (int j = 0; j < x_height; ++j)
            {
                m_visibleCell[i][j] = nullptr;
            }
        }
    }
    
    virtual ~EncoderGrid()
    {
    }

    EncoderCell* GetVisible(int x, int y)
    {
        return m_visibleCell[x][y];
    }

    void SetVisible(int x, int y, EncoderCell* cell)
    {
        m_visibleCell[x][y] = cell;
    }

    virtual void HandlePress(int x, int y)
    {
    }

    virtual void Apply(MessageIn msg) = 0;
};

}

    
