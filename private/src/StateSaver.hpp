#pragma once
#include "plugin.hpp"
#include <string>
#include <random>
#include <vector>
#include <cassert>
#include <cstring>
#include <cmath>

#ifndef IOS_BUILD
#include "JanssonAdapter.hpp"
#else
#include "JuceSon.hpp"
#endif

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

        void SetLeftScene(int scene)
        {
            m_left = scene;
        }

        void SetRightScene(int scene)
        {
            m_right = scene;
        }
    };

    void Process(Input& input)
    {
        if (m_sceneInfo.m_left != input.m_left)
        {
            m_sceneInfo.m_left = input.m_left;
            HandleBlendChanges(0, m_sceneInfo.m_blend);
        }

        if (m_sceneInfo.m_right != input.m_right)
        {
            m_sceneInfo.m_right = input.m_right;
            HandleBlendChanges(1, m_sceneInfo.m_blend);
        }

        if (m_sceneInfo.m_blend != input.m_blend)
        {
            float oldBlend = m_sceneInfo.m_blend;
            m_sceneInfo.m_blend = input.m_blend;
            HandleBlendChanges(oldBlend, input.m_blend);
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
            m_state[i].second.HandleSceneInfoChange(m_sceneInfo);
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
