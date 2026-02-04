#pragma once

#include <vector>
#include "Blink.hpp"
#include "JuceSon.hpp"


namespace SmartGrid
{

struct StateEncoderCell;

struct SceneManager
{
    static constexpr size_t x_numScenes = 8;

    // Scene state (set directly by owner)
    //
    size_t m_scene1;
    size_t m_scene2;
    float m_blendFactor;

    // Shift state (centralized here, set directly by owner)
    //
    bool m_shift;

    // Blink state (for UI feedback)
    //
    Blink m_blinker;

    // Output flags (set by Process, read by consumers)
    //
    bool m_changed;
    bool m_changedScene;

    // Previous state for change detection
    //
    size_t m_prevScene1;
    size_t m_prevScene2;
    float m_prevBlendFactor;

    // Cell registration for external state
    //
    std::vector<StateEncoderCell*> m_cells;
    bool m_externalState;

    SceneManager()
        : m_scene1(0)
        , m_scene2(1)
        , m_blendFactor(0.0f)
        , m_shift(false)
        , m_changed(false)
        , m_changedScene(false)
        , m_prevScene1(0)
        , m_prevScene2(1)
        , m_prevBlendFactor(0.0f)
        , m_externalState(false)
    {
    }

    float GetSceneValue(float* values)
    {
        return values[m_scene1] * (1.0f - m_blendFactor) + values[m_scene2] * m_blendFactor;
    }

    bool Scene1Active()
    {
        return m_blendFactor < 1;
    }

    bool Scene2Active()
    {
        return m_blendFactor > 0;
    }

    bool IsSceneActive(size_t sceneIx)
    {
        return (sceneIx == m_scene1 && Scene1Active()) || (sceneIx == m_scene2 && Scene2Active());
    }

    void RegisterCell(StateEncoderCell* cell)
    {
        if (m_externalState)
        {
            m_cells.push_back(cell);
        }
    }

    // Process method called first each frame
    // Compares current state to previous state and sets m_changed and m_changedScene flags
    //
    void Process()
    {
        m_blinker.Process();

        m_changed = false;
        m_changedScene = false;

        if (m_scene1 != m_prevScene1 || m_scene2 != m_prevScene2)
        {
            m_changedScene = true;
        }

        if (m_blendFactor != m_prevBlendFactor || m_scene1 != m_prevScene1 || m_scene2 != m_prevScene2)
        {
            if (m_blendFactor == 0 || m_blendFactor == 1 ||
                m_prevBlendFactor == 0 || m_prevBlendFactor == 1)
            {
                m_changedScene = true;
            }

            m_changed = true;            
        }

        // Update previous state
        //
        m_prevScene1 = m_scene1;
        m_prevScene2 = m_scene2;
        m_prevBlendFactor = m_blendFactor;
    }

    void SetBlendFactor(float blendFactor)
    {
        m_blendFactor = blendFactor;
    }

    void SetScene1(size_t scene1)
    {
        m_scene1 = scene1;
    }

    void SetScene2(size_t scene2)
    {
        m_scene2 = scene2;
    }
};

}
