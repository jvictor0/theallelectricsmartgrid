#pragma once

#include <string>
#include <cstring>
#include <cassert>
#include "Json.hpp"
#include "SceneManager.hpp"

struct ParamEventLogger;
struct State;

void RecordStateChange(ParamEventLogger* paramEventLogger, State* state, int scene);

struct State
{
    static const size_t x_maxScenes = 8;
    static const size_t x_maxValueLen = 8;
    std::string m_name;
    size_t m_len;
    char* m_ptr;
    char m_buf[x_maxScenes * x_maxValueLen];
    char m_default[x_maxValueLen];
    int m_curScene;
    float m_boundary;
    int m_numScenes;
    ParamEventLogger* m_paramEventLogger;

    State(std::string name, size_t len, char* ptr, int numScenes, ParamEventLogger* paramEventLogger)
        : m_name(name)
        , m_len(len)
        , m_ptr(ptr)
        , m_curScene(0)
        , m_numScenes(numScenes)
        , m_paramEventLogger(paramEventLogger)
    {
        memset(m_buf, 0, sizeof(m_buf));
        memset(m_default, 0, sizeof(m_default));
        assert(len <= x_maxValueLen);
        SetVal(m_default, m_ptr, m_len);
        SetVal(m_buf, m_ptr, m_len);
    }

    template<class T>
    T Get()
    {
        return *reinterpret_cast<T*>(m_ptr);
    }

    template<class T>
    void Set(T t)
    {
        SetVal(m_ptr, reinterpret_cast<char*>(&t), m_len);
        SetBytes(reinterpret_cast<char*>(&t), m_curScene);
    }

    template<class T>
    void SetRaw(T t)
    {
        SetVal(m_ptr, &t, m_len);
        SetVal(m_buf + m_curScene * m_len, &t, m_len);
    }

    void SetBytes(char* bytes, int scene)
    {
        SetVal(m_buf + scene * m_len, bytes, m_len);
        RecordStateChange(m_paramEventLogger, this, scene);
    }

    JSON ToJSON(JsonArena& a)
    {
        SaveValToScene();
        JSON ret = a.Array();
        for (int s = 0; s < m_numScenes; ++s)
        {
            for (size_t i = 0; i < m_len; ++i)
            {
                ret.Append(a.Integer(m_buf[s * m_len + i]));
            }
        }

        return ret;
    }

    void SetFromJSON(JSON jin)
    {
        size_t len = jin.Size();
        for (int s = 0; s < m_numScenes; ++s)
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
        SetVal(m_buf + m_curScene * m_len, m_ptr, m_len);
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

        SetVal(m_ptr, m_buf + scene * m_len, m_len);
        m_curScene = scene;
    }

    void CopyToScene(int scene)
    {
        SetBytes(m_ptr, scene);
    }

    void RevertToDefaultForScene(int scene)
    {
        SetBytes(m_default, scene);
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
