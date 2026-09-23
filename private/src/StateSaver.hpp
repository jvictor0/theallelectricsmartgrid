#pragma once
#include "plugin.hpp"
#include <string>
#include <map>
#include <random>
#include <vector>
#include <cassert>
#include <cstring>
#include <cmath>
#include "State.hpp"
#include "ThreadId.hpp"

#include "JuceSon.hpp"
#include "SmartGridOneContext.hpp"

template<size_t NumScenes>
struct StateSaverTemp
{
    template<class T>
    static State* Mk(std::string name, T* t, SmartGridOneContext* context)
    {
        return new State(name, sizeof(T), reinterpret_cast<char*>(t), NumScenes, &context->m_paramEventLogger);
    }

    State* Insert(State* s)
    {
        m_state.push_back(s);
        auto inserted = m_stateMap.insert(std::make_pair(s->m_name, s));
        if (!inserted.second)
        {
            char* oldPtr = inserted.first->second->m_ptr;
            char* newPtr = s->m_ptr;
            if (oldPtr != newPtr)
            {
                throw std::runtime_error("State with name " + s->m_name + " already exists");
            }
        }

        return Get(s->m_name);
    }

    State* Get(std::string name)
    {
        if (thread_threadId == ThreadId::Audio)
        {
            throw std::runtime_error("Don't call metadata stuff from audio thread you dingus");
        }

        auto it = m_stateMap.find(name);
        if (it == m_stateMap.end())
        {
            return nullptr;
        }

        return it->second;
    }

    State* Get(std::string name, size_t i)
    {
        std::string name2 = name + "_" + std::to_string(i);
        return Get(name2);
    }

    State* Get(std::string name, size_t i, size_t j)
    {
        std::string name2 = name + "_" + std::to_string(i) + "_" + std::to_string(j);
        return Get(name2);
    }

    template<class T>
    State* Insert(std::string name, T* t)
    {
        return Insert(Mk(name, t, m_context));
    }

    template<class T>
    State* Insert(std::string name, size_t i, T* t)
    {
        std::string name2 = name + "_" + std::to_string(i);
        return Insert(Mk(name2, t, m_context));
    }

    template<class T>
    State* Insert(std::string name, size_t i, size_t j, T* t)
    {
        std::string name2 = name + "_" + std::to_string(i) + "_" + std::to_string(j);
        return Insert(Mk(name2, t, m_context));
    }

    JSON ToJSON(JsonArena& a)
    {
        JSON rootJ = a.Object();
        for (auto& s : m_state)
        {
            rootJ.SetNew(s->m_name.c_str(), s->ToJSON(a));
        }

        return rootJ;
    }

    void SetFromJSON(JSON rootJ)
    {
        for (auto& s : m_state)
        {
            JSON val = rootJ.Get(s->m_name.c_str());
            if (!val.IsNull())
            {
                s->SetFromJSON(val);
            }
        }
    }

    void SetBoundaries()
    {
        auto rng = std::default_random_engine {};
        std::shuffle(std::begin(m_state), std::end(m_state), rng);

        for (size_t i = 0; i < m_state.size(); ++i)
        {
            m_state[i]->m_boundary = static_cast<float>(i + 1) / (m_state.size() + 1);
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

    std::vector<State*> m_state;
    std::map<std::string, State*> m_stateMap;
    SmartGridOneContext* m_context;

    // Cached previous values to detect changes
    //
    size_t m_prevScene1;
    size_t m_prevScene2;
    float m_prevBlendFactor;

    StateSaverTemp(SmartGridOneContext* context)
        : m_context(context)
        , m_prevScene1(0)
        , m_prevScene2(1)
        , m_prevBlendFactor(0.0f)
    {
    }

    ~StateSaverTemp()
    {
        for (State* state : m_state)
        {
            delete state;
        }
    }

    void Process()
    {
        if constexpr (NumScenes == 1)
        {
            return;
        }

        SmartGrid::SceneManager& sceneManager = m_context->m_sceneManager;
        if (m_prevScene1 != sceneManager.m_scene1)
        {
            m_prevScene1 = sceneManager.m_scene1;
            HandleBlendChanges(0, sceneManager.m_blendFactor);
        }

        if (m_prevScene2 != sceneManager.m_scene2)
        {
            m_prevScene2 = sceneManager.m_scene2;
            HandleBlendChanges(1, sceneManager.m_blendFactor);
        }

        if (m_prevBlendFactor != sceneManager.m_blendFactor)
        {
            float oldBlend = m_prevBlendFactor;
            m_prevBlendFactor = sceneManager.m_blendFactor;
            HandleBlendChanges(oldBlend, sceneManager.m_blendFactor);
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

        assert(minIx == 0 || m_state[minIx - 1]->m_boundary < oldBlend);
        assert(maxIx == m_state.size() - 1 || newBlend < m_state[maxIx]->m_boundary);

        for (size_t i = minIx; i < maxIx; ++i)
        {
            m_state[i]->HandleSceneInfoChange(&m_context->m_sceneManager);
        }
    }

    void CopyToScene(int scene)
    {
        for (auto& s : m_state)
        {
            s->CopyToScene(scene);
        }
    }

    void RevertToDefaultForScene(int scene)
    {
        for (auto& s : m_state)
        {
            s->RevertToDefaultForScene(scene);
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
