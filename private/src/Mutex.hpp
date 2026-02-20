#pragma once

#include <atomic>

struct ReaderWriterLock
{
    #pragma pack(push, 1)
    struct State
    {
        uint8_t m_readers;
        uint8_t m_writer;
        bool m_writerWaiting;

        State()
            : m_readers(0)
            , m_writer(0)
            , m_writerWaiting(false)
        {
        }
    };
    #pragma pack(pop)
    
    std::atomic<State> m_state;

    ReaderWriterLock()
        : m_state(State())
    {
    }

    bool TryLockReader()
    {
        State state = m_state.load();
        if (state.m_writer == 0 && !state.m_writerWaiting)
        {
            State newState = state;
            newState.m_readers++;
            return m_state.compare_exchange_strong(state, newState);
        }

        return false;
    }

    void UnlockReader()
    {
        while (true)
        {
            State state = m_state.load();
            State newState = state;
            newState.m_readers--;
            if (m_state.compare_exchange_strong(state, newState))
            {
                return;
            }
        }
    }

    bool TryLockWriter()
    {
        State state = m_state.load();
        if (state.m_writer == 0 && state.m_readers == 0)
        {
            State newState = state;
            newState.m_writer = 1;
            newState.m_writerWaiting = false;
            return m_state.compare_exchange_strong(state, newState);
        }
        else if (state.m_writer == 0 && state.m_readers > 0)
        {
            State newState = state;
            newState.m_writerWaiting = true;
            m_state.compare_exchange_strong(state, newState);
            return false;
        }

        return false;
    }

    void LockWriter()
    {
        while (!TryLockWriter())
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    void UnlockWriter()
    {
        State state = m_state.load();
        state.m_writer = 0;
        m_state.store(state);
    }
};

struct SpinLock
{
    std::atomic<bool> m_locked;

    SpinLock()
     : m_locked(false)
    {
    }

    void Lock()
    {
        while (m_locked.exchange(true))
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    void Unlock()
    {
        m_locked.store(false);
    }
};

struct AutoLockSpin
{
    SpinLock& m_lock;

    AutoLockSpin(SpinLock& lock)
     : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~AutoLockSpin()
    {
        m_lock.Unlock();
    }
};

struct AutoLockReader
{
    ReaderWriterLock& m_lock;
    bool m_locked;

    AutoLockReader(ReaderWriterLock& lock)
     : m_lock(lock)
     , m_locked(false)
    {
        m_locked = m_lock.TryLockReader();
    }

    ~AutoLockReader()
    {
        if (m_locked)
        {
            m_lock.UnlockReader();
        }
    }
};

struct AutoLockWriter
{
    ReaderWriterLock& m_lock;

    AutoLockWriter(ReaderWriterLock& lock)
     : m_lock(lock)
    {
        m_lock.LockWriter();
    }

    ~AutoLockWriter()
    {
        m_lock.UnlockWriter();
    }
};