#pragma once

#include "JuceSon.hpp"
#include <atomic>

// StateInterchange — lock-free handshake for patch save/load between the audio
// thread (which owns the live state) and the message thread (which owns disk IO).
//
// Save serialization runs on the AUDIO thread and must not call the system
// allocator. It therefore builds into m_saveArena, a bump allocator whose buffer
// is owned here and (re)allocated only on the message thread. If a build
// exhausts the arena, the audio thread acks failure; the message thread grows
// the arena (doubling) and re-requests — the audio thread only ever bumps.
//
// Lifetime: the serialized tree is pointers into m_saveArena, so the arena must
// outlive the message-thread Dumps()/AckSaveCompleted(). It is reset only at the
// start of the next build, so m_toSave/m_lastSave stay valid until then.
//
struct StateInterchange
{
    std::atomic<bool> m_saveRequested;
    std::atomic<bool> m_saveCompleted;
    std::atomic<bool> m_saveFailed;
    std::atomic<bool> m_loadRequested;
    std::atomic<bool> m_restoreFaders;
    std::atomic<bool> m_newRequested;

    JSON m_toSave;
    JSON m_lastSave;
    JSON m_toLoad;

    // Backing arenas. m_saveArena: audio-thread build target. m_loadArena:
    // message-thread parse target (load tree handed to the audio thread).
    //
    JsonArena m_saveArena;
    JsonArena m_loadArena;

    StateInterchange()
        : m_saveRequested(false)
        , m_saveCompleted(true)
        , m_saveFailed(false)
        , m_loadRequested(false)
        , m_restoreFaders(true)
        , m_newRequested(false)
        , m_toSave(JSON::Null())
        , m_lastSave(JSON::Null())
        , m_toLoad(JSON::Null())
        , m_saveArena(JsonArena::kDefaultCapacity)
        , m_loadArena(JsonArena::kDefaultCapacity)
    {
    }

    // ---- Save ----
    //
    // Audio thread: the arena to build into. Reset it before building.
    //
    JsonArena& SaveArena()
    {
        return m_saveArena;
    }

    bool RequestSave()
    {
        if (m_saveRequested.load() || !m_saveCompleted.load())
        {
            return false;
        }

        m_saveFailed.store(false);
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

    // Audio thread: a build succeeded.
    //
    void AckSaveRequested(JSON toSave)
    {
        m_toSave = toSave;
        m_lastSave = m_toSave;
        m_saveCompleted.store(false);
        m_saveRequested.store(false);
        m_saveFailed.store(false);
    }

    // Audio thread: a build exhausted the arena. The message thread must grow
    // and re-request (see RetrySaveIfFailed).
    //
    void AckSaveFailed()
    {
        m_saveRequested.store(false);
        m_saveFailed.store(true);
    }

    void AckSaveCompleted()
    {
        m_lastSave = m_toSave;
        m_toSave = JSON::Null();
        m_saveCompleted.store(true);
    }

    // Message thread: if the last build failed, double the arena and re-arm the
    // request. Returns true when a retry was armed (so the caller gives the
    // audio thread another frame before reading the result).
    //
    bool RetrySaveIfFailed()
    {
        if (!m_saveFailed.load())
        {
            return false;
        }

        m_saveArena.GrowAndReset();
        m_saveFailed.store(false);
        m_saveRequested.store(true);
        return true;
    }

    // ---- Load ----
    //
    // Message thread: parse JSON text into the load arena (growing on
    // exhaustion). Returns JSON::Null() on parse error. The returned tree is
    // valid until the next ParseForLoad.
    //
    JSON ParseForLoad(const char* text)
    {
        m_loadArena.Reset();
        JSON parsed = m_loadArena.Loads(text);
        while (parsed.IsNull() && m_loadArena.Failed())
        {
            m_loadArena.GrowAndReset();
            parsed = m_loadArena.Loads(text);
        }

        return parsed;
    }

    bool RequestLoad(JSON toLoad, bool restoreFaders)
    {
        if (m_loadRequested.load())
        {
            return false;
        }

        m_toLoad = toLoad;
        m_restoreFaders.store(restoreFaders);
        m_loadRequested.store(true);
        return true;
    }

    bool GetRestoreFaders()
    {
        return m_restoreFaders.load();
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
        m_restoreFaders.store(true);
        m_loadRequested.store(false);
    }

    // ---- New ----
    //
    bool RequestNew()
    {
        if (m_newRequested.load())
        {
            return false;
        }

        m_newRequested.store(true);
        return true;
    }

    bool IsNewRequested()
    {
        return m_newRequested.load();
    }

    void AckNewCompleted()
    {
        m_newRequested.store(false);
    }
};
