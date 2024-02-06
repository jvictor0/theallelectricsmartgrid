#pragma once
#include "plugin.hpp"
#include <string>
#include <vector>

struct StateSaver
{
    struct State
    {
        size_t m_len;
        char* m_ptr;

        State(size_t len, char* ptr)
            : m_len(len)
            , m_ptr(ptr)
        {
        }

        json_t* ToJSON()
        {
            json_t* ret = json_array();
            for(size_t i = 0; i < m_len; ++i)
            {
                json_array_append(ret, json_integer(m_ptr[i]));
            }
            
            return ret;
        }

        void SetFromJSON(json_t* jin)
        {
            for (size_t i = 0; i < m_len; ++i)
            {
                json_t* j = json_array_get(jin, i);
                m_ptr[i] = json_integer_value(j);
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
        for (auto s : m_state)
        {
            json_object_set_new(rootJ, s.first.c_str(), s.second.ToJSON());
        }

        return rootJ;
    }

    void SetFromJSON(json_t* rootJ)
    {
        for (auto s : m_state)
        {
            json_t* val = json_object_get(rootJ, s.first.c_str());
            if (val)
            {
                s.second.SetFromJSON(val);
            }
        }
    }

    std::vector<std::pair<std::string, State>> m_state;
};
