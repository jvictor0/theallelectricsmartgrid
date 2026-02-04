#pragma once
#include "plugin.hpp"
#include <string>
#include <random>
#include <vector>
#include <cassert>
#include <cstring>
#include <cmath>

#include "JuceSon.hpp"
#include "SceneManager.hpp"

template<size_t NumScenes>
struct StateSaverTemp
{
    struct State
    {
        size_t m_len;
        char* m_ptr;
        char m_buf[NumScenes * 8];
        char m_default[8];
        int m_curScene;
        float m_boundary;

        State(size_t len, char* ptr)
            : m_len(len)
            , m_ptr(ptr)
            , m_curScene(0)
        {
            memset(m_buf, 0, sizeof(m_buf));
            memset(m_default, 0, sizeof(m_default));
            assert(len <= 8);
            SetVal(m_default, m_ptr, m_len);
        }

        JSON ToJSON()
        {
            SaveValToScene();
            JSON ret = JSON::Array();
            for (size_t s = 0; s < NumScenes; ++s)
            {
                for (size_t i = 0; i < m_len; ++i)
                {
                    ret.Append(JSON::Integer(m_buf[s * m_len + i]));
                }
            }

            return ret;
        }

        void SetFromJSON(JSON jin)
        {
            size_t len = jin.Size();
            for (size_t s = 0; s < NumScenes; ++s)
            {
                if (len <= s * m_len)
                {
                    break;
                }
                
                for (size_t i = 0; i < m_len; ++i)
                {
                    m_buf[s * m_len + i] = jin.GetAt(s * m_len + i).IntegerValue();
                }
            }

            SetVal(m_ptr, m_buf + m_curScene * m_len, m_len);
        }

        void SaveValToScene()
        {
            CopyToScene(m_curScene);
        }

        void SetVal(void* dst, void* src, size_t len)
        {
            switch (len)
            {
                case 1:
                {
                    *reinterpret_cast<uint8_t*>(dst) = *reinterpret_cast<uint8_t*>(src);
                    break;
                }
                case 2:
                {
                    *reinterpret_cast<uint16_t*>(dst) = *reinterpret_cast<uint16_t*>(src);
                    break;
                }
                case 4:
                {
                    *reinterpret_cast<uint32_t*>(dst) = *reinterpret_cast<uint32_t*>(src);
                    break;
                }
                case 8:
                {
                    *reinterpret_cast<uint64_t*>(dst) = *reinterpret_cast<uint64_t*>(src);
                    break;
                }
                default:
                {
                    assert(false);
                    break;
                }
            }
        }

        void LoadValFromScene(int scene)
        {
            if (scene == m_curScene)
            {
                return;
            }

            SaveValToScene();
            SetVal(m_ptr, m_buf + scene * m_len, m_len);
            m_curScene = scene;
        }

        void CopyToScene(int scene)
        {
            SetVal(m_buf + scene * m_len, m_ptr, m_len);
        }

        void RevertToDefaultForScene(int scene)
        {
            SetVal(m_buf + scene * m_len, m_default, m_len);
            if (scene == m_curScene)
            {
                SetVal(m_ptr, m_default, m_len);
            }
        }

        void HandleSceneInfoChange(SmartGrid::SceneManager* sceneManager)
        {
            if (sceneManager->m_blendFactor < m_boundary)
            {
                LoadValFromScene(sceneManager->m_scene1);
            }
            else
            {
                LoadValFromScene(sceneManager->m_scene2);
            }
        }
    };

    template<class T>
    static State Mk(T* t)
    {
        return State(sizeof(T), reinterpret_cast<char*>(t));
    }

    template<class T>
    void Insert(std::string name, T* t)
    {
        m_state.push_back(std::make_pair(name, Mk(t)));
    }

    template<class T>
    void Insert(std::string name, size_t i, T* t)
    {
        m_state.push_back(std::make_pair(name + "_" + std::to_string(i), Mk(t)));
    }

    template<class T>
    void Insert(std::string name, size_t i, size_t j, T* t)
    {
        m_state.push_back(std::make_pair(name + "_" + std::to_string(i) + "_" + std::to_string(j), Mk(t)));
    }
    
    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        for (auto& s : m_state)
        {
            rootJ.SetNew(s.first.c_str(), s.second.ToJSON());
        }

        return rootJ;
    }

    void SetFromJSON(JSON rootJ)
    {
        for (auto& s : m_state)
        {
            JSON val = rootJ.Get(s.first.c_str());
            if (!val.IsNull())
            {
                s.second.SetFromJSON(val);
            }
        }
    }

    void SetBoundaries()
    {
        auto rng = std::default_random_engine {};
        std::shuffle(std::begin(m_state), std::end(m_state), rng);

        for (size_t i = 0; i < m_state.size(); ++i)
        {
            m_state[i].second.m_boundary = static_cast<float>(i + 1) / (m_state.size() + 1);
        }
    }

    void Finalize()
    {
        SetBoundaries();
        for (size_t i = 1; i < NumScenes; ++i)
        {
            CopyToScene(i);
        }
    }

    std::vector<std::pair<std::string, State>> m_state;
    SmartGrid::SceneManager* m_sceneManager;

    // Cached previous values to detect changes
    //
    size_t m_prevScene1;
    size_t m_prevScene2;
    float m_prevBlendFactor;

    StateSaverTemp()
        : m_sceneManager(nullptr)
        , m_prevScene1(0)
        , m_prevScene2(1)
        , m_prevBlendFactor(0.0f)
    {
    }

    void SetSceneManager(SmartGrid::SceneManager* sceneManager)
    {
        m_sceneManager = sceneManager;
    }

    void Process()
    {
        if (!m_sceneManager)
        {
            return;
        }

        if (m_prevScene1 != m_sceneManager->m_scene1)
        {
            m_prevScene1 = m_sceneManager->m_scene1;
            HandleBlendChanges(0, m_sceneManager->m_blendFactor);
        }

        if (m_prevScene2 != m_sceneManager->m_scene2)
        {
            m_prevScene2 = m_sceneManager->m_scene2;
            HandleBlendChanges(1, m_sceneManager->m_blendFactor);
        }

        if (m_prevBlendFactor != m_sceneManager->m_blendFactor)
        {
            float oldBlend = m_prevBlendFactor;
            m_prevBlendFactor = m_sceneManager->m_blendFactor;
            HandleBlendChanges(oldBlend, m_sceneManager->m_blendFactor);
        }
    }

    void HandleBlendChanges(float oldBlend, float newBlend)
    {
        if (newBlend < oldBlend)
        {
            std::swap(oldBlend, newBlend);
        }

        size_t minIx = std::max(0, static_cast<int>(oldBlend * (m_state.size() + 1)) - 1);
        size_t maxIx = std::min(m_state.size() - 1, static_cast<size_t>(std::ceil(newBlend * (m_state.size() + 1))));

        assert(minIx == 0 || m_state[minIx - 1].second.m_boundary < oldBlend);
        assert(maxIx == m_state.size() - 1 || newBlend < m_state[maxIx].second.m_boundary);

        for (size_t i = minIx; i < maxIx; ++i)
        {
            m_state[i].second.HandleSceneInfoChange(m_sceneManager);
        }
    }

    void CopyToScene(int scene)
    {
        for (auto& s : m_state)
        {
            s.second.CopyToScene(scene);
        }
    }

    void RevertToDefaultForScene(int scene)
    {
        for (auto& s : m_state)
        {
            s.second.RevertToDefaultForScene(scene);
        }
    }

    void RevertToDefaultAllScenes()
    {
        for (size_t s = 0; s < NumScenes; ++s)
        {
            RevertToDefaultForScene(s);
        }
    }
};

typedef StateSaverTemp<1> StateSaver;
typedef StateSaverTemp<8> ScenedStateSaver;
