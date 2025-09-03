#pragma once

#include "SmartGrid.hpp"

namespace SmartGrid
{

struct EncoderCell : public Cell
{
    uint8_t m_lastVelocity;
    static constexpr float x_speed = 0.005;

    EncoderCell()
        : m_lastVelocity(0)
    {
        SetPressureSensitive();
    }
    
    virtual ~EncoderCell()
    {
    }

    virtual Color GetColor() override
    {
        return Color(GetNormalizedValue() * 255, GetTwisterColor(), GetAnimationValue());
    }

    virtual void OnPress(uint8_t velocity) override
    {
        int8_t svelocity = velocity;
        Increment(svelocity * x_speed);
        m_lastVelocity = velocity;
    }

    virtual void OnRelease() override
    {
        m_lastVelocity = 0;
    }

    virtual void OnPressureChange(uint8_t velocity) override
    {
        int8_t svelocity = velocity - m_lastVelocity;
        Increment(svelocity * x_speed);
        m_lastVelocity = velocity;
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
    struct SceneManager
    {
        static constexpr size_t x_numScenes = 8;
        size_t m_scene1;
        size_t m_scene2;
        float m_blendFactor;
        size_t m_track;
        std::vector<StateEncoderCell*> m_cells;
        bool m_externalState;
        
        SceneManager()
            : m_scene1(0)
            , m_scene2(1)
            , m_blendFactor(0.0f)
            , m_track(0)
            , m_externalState(false)
        {
        }
        
        float GetSceneValue(float* values)
        {
            return values[m_scene1] * (1.0f - m_blendFactor) + values[m_scene2] * m_blendFactor;
        }

        void RegisterCell(StateEncoderCell* cell)
        {
            if (m_externalState)
            {                
                m_cells.push_back(cell);
            }
        }

        struct Input
        {
            float m_blendFactor;
            size_t m_scene1;
            size_t m_scene2;
            size_t m_track;

            Input()
                : m_blendFactor(0.0f)
                , m_scene1(0)
                , m_scene2(1)
                , m_track(0)
            {
            }
        };

        void Process(const Input& input, bool* changed, bool* changedScene)
        {
            if (input.m_scene1 != m_scene1 || input.m_scene2 != m_scene2)
            {
                *changedScene = true;
            }

            if (input.m_blendFactor != m_blendFactor || input.m_scene1 != m_scene1 || input.m_scene2 != m_scene2)
            {
                m_blendFactor = input.m_blendFactor;
                m_scene1 = input.m_scene1;
                m_scene2 = input.m_scene2;
                *changed = true;
                SetAllStates();
            }

            m_track = input.m_track;
        }

        void SetAllStates()
        {
            if (m_externalState)
            {
                for (StateEncoderCell* cell : m_cells)
                {
                    cell->SetState();
                }
            }
        }

        void SetBlendFactor(float blendFactor)
        {
            m_blendFactor = blendFactor;
            SetAllStates();
        }

        void SetScene1(size_t scene1)
        {
            m_scene1 = scene1;
            SetAllStates();
        }

        void SetScene2(size_t scene2)
        {
            m_scene2 = scene2;
            SetAllStates();
        }
    };

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
        size_t track = m_sceneManager->m_track;
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
            if (m_values[i][m_sceneManager->m_scene1] != 0 || m_values[i][m_sceneManager->m_scene2] != 0)
            {
                return false;
            }
        }

        return true;
    }

    float m_values[x_maxPoly][SceneManager::x_numScenes];
    float* m_state[x_maxPoly];
    size_t m_numTracks;
    float m_min;
    float m_max;
    float m_logMaxOverMin;
    bool m_exponential;
    SceneManager* m_sceneManager;

    json_t* ToJSON()
    {
        json_t* root = json_object();
        json_t* values = json_array();
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            json_t* sceneValues = json_array();
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                json_array_append_new(sceneValues, json_real(m_values[j][i]));
            }
            
            json_array_append_new(values, sceneValues);
        }
        
        json_object_set_new(root, "values", values);
        return root;
    }

    void FromJSON(json_t* root)
    {
        json_t* values = json_object_get(root, "values");
        for (size_t i = 0; i < SceneManager::x_numScenes; ++i)
        {
            json_t* sceneValues = json_array_get(values, i);
            m_numTracks = json_array_size(sceneValues);
            for (size_t j = 0; j < m_numTracks; ++j)
            {
                m_values[j][i] = json_real_value(json_array_get(sceneValues, j));
            }
        }

        if (m_exponential)
        {
            m_logMaxOverMin = std::log2f(m_max / m_min);
        }

        SetState();
    }
    
    StateEncoderCell(
        float min,
        float max,
        bool exponential,
        SceneManager* sceneManager)
        : m_numTracks(0)
        , m_min(min)
        , m_max(max)
        , m_exponential(exponential)
        , m_sceneManager(sceneManager)
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

        if (m_exponential)
        {
            m_logMaxOverMin = std::log2f(m_max / m_min);
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
        return GetNormalizedValueForTrack(m_sceneManager->m_track);
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
        float normalizedValue = GetNormalizedValueForTrack(track);
        if (m_exponential)
        {
            return m_min * std::pow(2, normalizedValue * m_logMaxOverMin);
        }
        else
        {
            return m_min + normalizedValue * (m_max - m_min);
        }
    }

    void SetState()
    {
        for (size_t i = 0; i < m_numTracks; ++i)
        {
            SetStateForTrack(i);
        }

        PostSetState();
    }

    virtual void PostSetState()
    {
    }

    void SetStateForTrack(size_t track)
    {
        *m_state[track] = GetValue(track);
        PostSetState();
    }

    virtual void Increment(float delta) override
    {
        if (delta == 0)
        {
            return;
        }

        int s1 = m_sceneManager->m_scene1;
        int s2 = m_sceneManager->m_scene2;
        float t = m_sceneManager->m_blendFactor;
        size_t track = m_sceneManager->m_track;
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
        float delta = value - GetNormalizedValueForTrack(m_sceneManager->m_track);
        Increment(delta);
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
};

struct EncoderGrid : public Grid
{
    static constexpr int x_width = 4;
    static constexpr int x_height = 4;

    EncoderGrid()
    {
    }
    
    virtual ~EncoderGrid()
    {
    }

    virtual void HandlePress(int x, int y)
    {
    }
    
    virtual void Apply(Message msg) override
    {
        if (msg.m_y >= 0 && msg.m_y < x_height)
        {
            if (msg.m_x >= 0 && msg.m_x < x_width)
            {
                Grid::Apply(msg);
            }
            else if (msg.m_x >= x_width && msg.m_x < 2 * x_width && msg.m_velocity > 0)
            {
                HandlePress(msg.m_x - x_width, msg.m_y);
            }
        }        
    }
    
    EncoderCell* GetEncoderCell(int x, int y)
    {
        return static_cast<EncoderCell*>(Get(x, y));
    }
};

}

    
