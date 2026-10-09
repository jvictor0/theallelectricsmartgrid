#pragma once

#include "SmartGrid.hpp"
#include "SmartGridOneContext.hpp"

namespace SmartGrid
{

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
        SetAndRecordValue(GetSceneNormalizedValue(), scene);

        SetState();
    }

    void NeutralizeCurrentScene()
    {
        if (m_context->m_sceneManager.m_blendFactor < 1)
        {
            SetAndRecordValue(GetNeutralNormalizedValue(), m_context->m_sceneManager.m_scene1);
        }

        if (m_context->m_sceneManager.m_blendFactor > 0)
        {
            SetAndRecordValue(GetNeutralNormalizedValue(), m_context->m_sceneManager.m_scene2);
        }

        SetState();
    }

    bool IsNeutralCurrentScene()
    {
        if (m_values[m_context->m_sceneManager.m_scene1] != GetNeutralNormalizedValue() && m_context->m_sceneManager.m_blendFactor < 1)
        {
            return false;
        }

        if (m_values[m_context->m_sceneManager.m_scene2] != GetNeutralNormalizedValue() && m_context->m_sceneManager.m_blendFactor > 0)
        {
            return false;
        }

        return true;
    }

    float m_values[SceneManager::x_numScenes];
    float* m_state;
    SmartGridOneContext* m_context;

    JSON ToJSON(JsonArena& a)
    {
        JSON root = a.Object();
        JSON values = a.Array();
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            values.AppendNew(a.Real(ToValue(m_values[i])));
        }

        root.SetNew("values", values);
        return root;
    }

    void FromJSON(JSON root)
    {
        JSON values = root.Get("values");
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            float value = ToNormalized(static_cast<float>(values.GetAt(i).NumberValue()));
            m_values[i] = value;
        }

        SetState();
    }

    void SetAndRecordValue(float value, int scene)
    {
        m_values[scene] = value;
        m_context->m_paramEventLogger.RecordEncoderSet(this, scene);
    }
    
    StateEncoderCell()
        : m_values{}
        , m_state(nullptr)
        , m_context(nullptr)
    {
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            m_values[i] = 0;
        }
    }

    StateEncoderCell(SmartGridOneContext* context)
        : m_values{}
        , m_state(nullptr)
        , m_context(context)
    {
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            m_values[i] = 0;
        }
    }

    void SetStatePtr(float* state)
    {
        m_state = state;
    }

    virtual ~StateEncoderCell()
    {
    }

    virtual float GetNormalizedValue() override
    {
        return GetSceneNormalizedValue();
    }

    float GetSceneNormalizedValue()
    {
        return m_context->m_sceneManager.GetSceneValue(m_values);
    }

    bool AllNeutral()
    {
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            if (m_values[i] != GetNeutralNormalizedValue())
            {
                return false;
            }
        }

        return true;
    }

    float GetValue()
    {
        return ToValue(GetSceneNormalizedValue());
    }

    void SetState()
    {
        *m_state = GetSceneNormalizedValue();
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
        if (t <= 0)
        {
            SetAndRecordValue(std::max(0.0f, std::min(1.0f, m_values[s1] + delta)), s1);
        }
        else if (t >= 1)
        {
            SetAndRecordValue(std::max(0.0f, std::min(1.0f, m_values[s2] + delta)), s2);
        }
        else
        {
            float value = std::max(0.0f, std::min(1.0f, GetSceneNormalizedValue() + delta));
            float newValue1 = m_values[s1] + delta * (1.0f - t);
            float newValue2 = m_values[s2] + delta * t;
            if (newValue1 < 0 || newValue1 > 1)
            {
                SetAndRecordValue(std::max(0.0f, std::min(1.0f, newValue1)), s1);
                SetAndRecordValue((value - m_values[s1] * (1 - t)) / t, s2);
            }
            else if (newValue2 < 0 || newValue2 > 1)
            {
                SetAndRecordValue(std::max(0.0f, std::min(1.0f, newValue2)), s2);
                SetAndRecordValue((value - m_values[s2] * t) / (1 - t), s1);
            }
            else
            {
                SetAndRecordValue(newValue1, s1);
                SetAndRecordValue(newValue2, s2);
            }
        }

        SetState();
    }

    void SetToValue(float value)
    {
        float delta = value - GetSceneNormalizedValue();
        IncrementInternal(delta);
    }

    void SetValue(float value, bool allScenes)
    {
        for (size_t s = 0; s < SceneManager::x_numScenes; ++s)
        {
            if (!allScenes && !m_context->m_sceneManager.IsSceneActive(s))
            {
                continue;
            }

            SetAndRecordValue(value, s);
        }

        SetState();
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

    
