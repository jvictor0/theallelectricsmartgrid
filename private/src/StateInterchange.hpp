#pragma once

#include "JuceSon.hpp"
#include <atomic>

struct StateInterchange
{
    std::atomic<bool> m_saveRequested;
    std::atomic<bool> m_saveCompleted;
    std::atomic<bool> m_loadRequested;

    JSON m_toSave;
    JSON m_lastSave;
    JSON m_toLoad;

    StateInterchange()
        : m_saveRequested(false)
        , m_saveCompleted(true)
        , m_loadRequested(false)
        , m_toSave(JSON::Null())
        , m_lastSave(JSON::Null())
        , m_toLoad(JSON::Null())
    {
    }

    bool RequestLoad(JSON toLoad)
    {
        if (m_loadRequested.load())
        {
            return false;
        }

        m_toLoad = toLoad;
        m_loadRequested.store(true);
        return true;
    }

    bool IsLoadRequested()
    {
        return m_loadRequested.load();
    }

    JSON GetToLoad()
    {
        if (!m_loadRequested.load())
        {
            return JSON::Null();
        }

        m_lastSave = m_toLoad;
        return m_toLoad;
    }

    void AckLoadCompleted()
    {
        m_toLoad = JSON::Null();
        m_loadRequested.store(false);
    }

    bool RequestSave()
    {
        if (m_saveRequested.load() || !m_saveCompleted.load())
        {
            return false;
        }

        m_saveRequested.store(true);
        return true;
    }

    JSON GetToSave()
    {
        if (m_saveCompleted.load())
        {
            return JSON::Null();
        }

        return m_toSave;
    }

    bool IsSaveRequested()
    {
        return m_saveRequested.load();
    }

    bool IsSavePending()
    {
        return !m_saveCompleted.load();
    }

    void AckSaveRequested(JSON toSave)
    {
        m_toSave = toSave;
        m_lastSave = m_toSave;
        m_saveCompleted.store(false);
        m_saveRequested.store(false);
    }

    void AckSaveCompleted()
    {
        m_lastSave = m_toSave;
        m_toSave = JSON::Null();
        m_saveCompleted.store(true);
    }
};