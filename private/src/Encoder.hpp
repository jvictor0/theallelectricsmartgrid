#pragma once

#include "SmartGrid.hpp"
#include "SmartGridOneContext.hpp"

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
    bool m_bipolar = false;

    float ToValue(float normalized) const
    {
        return m_bipolar ? 2.0f * normalized - 1.0f : normalized;
    }

    float ToNormalized(float value) const
    {
        return m_bipolar ? (value + 1.0f) * 0.5f : value;
    }

    float GetNeutralNormalizedValue() const
    {
        return m_bipolar ? 0.5f : 0.0f;
    }

    void CopyToScene(size_t scene)
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            SetAndRecordValue(GetNormalizedValueForTrack(i), scene, i);
        }

        SetState();
    }

    void NeutralizeCurrentScene()
    {
        size_t track = m_sharedEncoderState->m_currentTrack;
        if (m_context->m_sceneManager.m_blendFactor < 1)
        {
            SetAndRecordValue(GetNeutralNormalizedValue(), m_context->m_sceneManager.m_scene1, track);
        }

        if (m_context->m_sceneManager.m_blendFactor > 0)
        {
            SetAndRecordValue(GetNeutralNormalizedValue(), m_context->m_sceneManager.m_scene2, track);
        }

        SetStateForTrack(track);
    }

    bool IsNeutralCurrentScene()
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            if (m_values[i][m_context->m_sceneManager.m_scene1] != GetNeutralNormalizedValue() && m_context->m_sceneManager.m_blendFactor < 1)
            {
                return false;
            }

            if (m_values[i][m_context->m_sceneManager.m_scene2] != GetNeutralNormalizedValue() && m_context->m_sceneManager.m_blendFactor > 0)
            {
                return false;
            }
        }

        return true;
    }

    bool IsNeutralCurrentSceneForTrack(size_t track)
    {
        if (m_values[track][m_context->m_sceneManager.m_scene1] != GetNeutralNormalizedValue() && m_context->m_sceneManager.m_blendFactor < 1)
        {
            return false;
        }
        
        if (m_values[track][m_context->m_sceneManager.m_scene2] != GetNeutralNormalizedValue() && m_context->m_sceneManager.m_blendFactor > 0)
        {
            return false;
        }

        return true;
    }

    float m_values[x_maxPoly][SceneManager::x_numScenes];
    float* m_state[x_maxPoly];
    size_t m_numTracks;
    SmartGridOneContext* m_context;
    SharedEncoderStateBase* m_sharedEncoderState;

    JSON ToJSON(JsonArena& a)
    {
        JSON root = a.Object();
        JSON values = a.Array();
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            JSON sceneValues = a.Array();
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                sceneValues.AppendNew(a.Real(ToValue(m_values[j][i])));
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
                float value = ToNormalized(static_cast<float>(sceneValues.GetAt(j).NumberValue()));
                SetAndRecordValue(value, i, j);
            }
        }

        SetState();
    }

    void SetAndRecordValue(float value, int scene, int track)
    {
        m_values[track][scene] = value;
        m_context->m_paramEventLogger.RecordEncoderSet(this, scene, track);
    }
    
    StateEncoderCell()
        : m_values{}
        , m_state{}
        , m_numTracks(0)
        , m_context(nullptr)
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

    StateEncoderCell(SmartGridOneContext* context, SharedEncoderStateBase* sharedEncoderState)
        : m_values{}
        , m_state{}
        , m_numTracks(0)
        , m_context(context)
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
        return m_context->m_sceneManager.GetSceneValue(m_values[track]);
    }

    bool AllNeutral()
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            for (size_t j = 0; j < SceneManager::x_numScenes; ++j)
            {
                if (m_values[i][j] != GetNeutralNormalizedValue())
                {
                    return false;
                }
            }
        }

        return true;
    }

    float GetValue(size_t track)
    {
        return ToValue(GetNormalizedValueForTrack(track));
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
        *m_state[track] = GetNormalizedValueForTrack(track);
    }

    void IncrementInternal(float delta)
    {
        if (delta == 0)
        {
            return;
        }

        int s1 = m_context->m_sceneManager.m_scene1;
        int s2 = m_context->m_sceneManager.m_scene2;
        float t = m_context->m_sceneManager.m_blendFactor;
        size_t track = m_sharedEncoderState->m_currentTrack;
        if (t <= 0)
        {
            SetAndRecordValue(std::max(0.0f, std::min(1.0f, m_values[track][s1] + delta)), s1, track);
        }
        else if (t >= 1)
        {
            SetAndRecordValue(std::max(0.0f, std::min(1.0f, m_values[track][s2] + delta)), s2, track);
        }
        else
        {
            float value = std::max(0.0f, std::min(1.0f, GetNormalizedValueForTrack(track) + delta));
            float newValue1 = m_values[track][s1] + delta * (1.0f - t);
            float newValue2 = m_values[track][s2] + delta * t;
            if (newValue1 < 0 || newValue1 > 1)
            {
                SetAndRecordValue(std::max(0.0f, std::min(1.0f, newValue1)), s1, track);
                SetAndRecordValue((value - m_values[track][s1] * (1 - t)) / t, s2, track);
            }
            else if (newValue2 < 0 || newValue2 > 1)
            {
                SetAndRecordValue(std::max(0.0f, std::min(1.0f, newValue2)), s2, track);
                SetAndRecordValue((value - m_values[track][s2] * t) / (1 - t), s1, track);
            }
            else
            {
                SetAndRecordValue(newValue1, s1, track);
                SetAndRecordValue(newValue2, s2, track);
            }
        }

        SetStateForTrack(track);
    }

    void SetToValue(float value)
    {
        float delta = value - GetNormalizedValueForTrack(m_sharedEncoderState->m_currentTrack);
        IncrementInternal(delta);
    }

    void SetValue(float value, bool allScenes, bool allTracks)
    {
        size_t startTrack = allTracks ? 0 : m_sharedEncoderState->m_currentTrack;
        size_t endTrack = allTracks ? m_numTracks : m_sharedEncoderState->m_currentTrack + 1;

        for (size_t t = startTrack; t < endTrack; ++t)
        {
            for (size_t s = 0; s < SceneManager::x_numScenes; ++s)
            {
                if (!allScenes && !m_context->m_sceneManager.IsSceneActive(s))
                {
                    continue;
                }

                SetAndRecordValue(value, s, t);
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
        : m_visibleCell{}
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
        if (x < 0 || x >= x_width || y < 0 || y >= x_height)
        {
            return nullptr;
        }

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

    
