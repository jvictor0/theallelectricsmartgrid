#pragma once
#include "plugin.hpp"
#include <string>
#include <random>
#include <vector>
#include <cassert>
#include <cstring>

extern "C"
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-volatile"
    #include <jansson.h>
#pragma clang diagnostic pop
}  

template<size_t NumScenes>
struct StateSaverTemp
{
    struct SceneInfo
    {
        int m_left;
        int m_right;
        float m_blend;

        SceneInfo()
            : m_left(0)
            , m_right(1)
            , m_blend(0.0f)
        {
        }
    };
    
    struct State
    {
        size_t m_len;
        char* m_ptr;
        char m_buf[NumScenes * 8];
        int m_curScene;
        float m_boundary;

        State(size_t len, char* ptr)
            : m_len(len)
            , m_ptr(ptr)
            , m_curScene(0)
        {
            memset(m_buf, 0, sizeof(m_buf));
            assert(len <= 8);
        }

        json_t* ToJSON()
        {
            SaveValToScene();
            json_t* ret = json_array();
            for (size_t s = 0; s < NumScenes; ++s)
            {
                for (size_t i = 0; i < m_len; ++i)
                {
                    json_array_append(ret, json_integer(m_buf[s * m_len + i]));
                }
            }

            return ret;
        }

        void SetFromJSON(json_t* jin)
        {
            size_t len = json_array_size(jin);
            for (size_t s = 0; s < NumScenes; ++s)
            {
                if (len <= s * m_len)
                {
                    break;
                }
                
                for (size_t i = 0; i < m_len; ++i)
                {
                    m_buf[s * m_len + i] = json_integer_value(json_array_get(jin, s * m_len + i));
                }
            }

            memcpy(m_ptr, m_buf + m_curScene * m_len, m_len);
        }

        void SaveValToScene()
        {
            CopyToScene(m_curScene);
        }

        void LoadValFromScene(int scene)
        {
            if (scene == m_curScene)
            {
                return;
            }

            SaveValToScene();
            memcpy(m_ptr, m_buf + scene * m_len, m_len);
            m_curScene = scene;
        }

        void CopyToScene(int scene)
        {
            memcpy(m_buf + scene * m_len, m_ptr, m_len);
        }

        void HandleSceneInfoChange(SceneInfo& info)
        {
            if (info.m_blend < m_boundary)
            {
                LoadValFromScene(info.m_left);
            }
            else
            {
                LoadValFromScene(info.m_right);
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
    
    json_t* ToJSON()
    {
        json_t* rootJ = json_object();
        for (auto& s : m_state)
        {
            json_object_set_new(rootJ, s.first.c_str(), s.second.ToJSON());
        }

        return rootJ;
    }

    void SetFromJSON(json_t* rootJ)
    {
        for (auto& s : m_state)
        {
            json_t* val = json_object_get(rootJ, s.first.c_str());
            if (val)
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
    }

    std::vector<std::pair<std::string, State>> m_state;
    SceneInfo m_sceneInfo;

    struct Input
    {
        int m_left;
        int m_right;
        float m_blend;
        
        Input()
            : m_left(0)
            , m_right(1)
            , m_blend(0.0f)
        {
        }

        void ProcessTrigger(int scene)
        {
            if (m_blend < 0.5f)
            {
                m_right = scene;
            }
            else
            {
                m_left = scene;
            }
        }
    };

    void Process(Input& input)
    {
        if (m_sceneInfo.m_left != input.m_left || m_sceneInfo.m_right != input.m_right || m_sceneInfo.m_blend != input.m_blend)
        {
            m_sceneInfo.m_left = input.m_left;
            m_sceneInfo.m_right = input.m_right;
            m_sceneInfo.m_blend = input.m_blend;
            for (auto& s : m_state)
            {
                s.second.HandleSceneInfoChange(m_sceneInfo);
            }        
        }
    }

    void CopyToScene(int scene)
    {
        for (auto& s : m_state)
        {
            s.second.CopyToScene(scene);
        }
    }
};

typedef StateSaverTemp<1> StateSaver;
typedef StateSaverTemp<8> ScenedStateSaver;
