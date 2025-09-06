#pragma once
#include "SmartGrid.hpp"
#include <atomic>

template<class BusInput>
struct SmartBusGeneric
{
    std::atomic<uint64_t> m_epoch;
    std::atomic<BusInput> m_messages[x_gridMaxSize][x_gridMaxSize];

    SmartBusGeneric()
        : m_epoch(0)
    {
    }
    
    void Put(int i, int j, BusInput payload, bool* changed)
    {        
        Store(i - x_gridXMin, j - x_gridYMin, payload, changed);
    }

    BusInput Get(int i, int j)
    {
        return Load(i - x_gridXMin, j - x_gridYMin);
    }

    void Store(size_t i, size_t j, BusInput payload, bool* changed)
    {        
        BusInput newPayload = m_messages[i][j].exchange(payload);
        if (payload != newPayload)
        {
            *changed = true;
        }
    }

    BusInput Load(size_t i, size_t j)
    {
        return m_messages[i][j].load();
    }    

    struct Iterator
    {
        int m_x;
        int m_y;
        SmartBusGeneric* m_owner;

        Iterator()
            : m_x(x_gridXMax)
            , m_y(x_gridYMax)
            , m_owner(nullptr)
        {
        }
        
        Iterator(SmartBusGeneric* owner)
            : m_x(x_gridXMin)
            , m_y(x_gridYMin)
            , m_owner(owner)
        {
        }

        bool operator==(const Iterator& other) const
        {
            if (other.Done())
            {
                return Done();
            }
            else if (Done())
            {
                return false;
            }

            return std::tie(m_x, m_y, m_owner) == std::tie(other.m_x, other.m_y, other.m_owner);
        }

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        Message operator*()
        {
            BusInput payload = m_owner->Get(m_x, m_y);
            return Message(m_x, m_y, payload);
        }

        Iterator& operator++()
        {
            ++m_y;
            if (x_gridYMax <= m_y)
            {
                m_y = x_gridYMin;
                ++m_x;
            }

            return *this;
        }

        bool Done() const
        {
            return x_gridXMax <= m_x;
        }

        int GetXPhysical() const
        {
            return m_x - x_gridXMin;
        }

        int GetYPhysical() const
        {
            return m_y - x_gridYMin;
        }
    };

    Iterator begin(uint64_t* epoch)
    {
        // If nothing has changed, don't bother.
        //
        uint64_t newEpoch = m_epoch.load();
        if (newEpoch == *epoch)
        {            
            return end();
        }
        else
        {
            *epoch = newEpoch;
            return Iterator(this);
        }
    }

    Iterator end()
    {
        return Iterator();
    }
};

typedef SmartBusGeneric<uint8_t> SmartBusInput;
typedef SmartBusGeneric<Color> SmartBusOutput;
typedef SmartBusGeneric<Color> SmartBusColor;

struct SmartBus
{
    size_t x_smartBusVersion = 1;
    size_t m_version;
    SmartBusInput m_input;
    SmartBusOutput m_output;
    std::atomic<Color> m_onColor;
    std::atomic<Color> m_offColor;

    SmartBus()
        : m_version(1)
    {
    }
};

struct SmartBusHolder
{
    SmartBus m_buses[x_numGridIds];

    SmartBus* Get(size_t gridId)
    {
        return &m_buses[gridId];
    }

    void PutColor(size_t gridId, int x, int y, Color c, bool* changed)
    {
        Get(gridId)->m_output.Put(x, y, c, changed);
    }

    void PutVelocity(size_t gridId, int x, int y, uint8_t v, bool* changed)
    {
        Get(gridId)->m_input.Put(x, y, v, changed);
    }

    void PutVelocity(size_t gridId, int x, int y, uint8_t v)
    {
        bool changed = false;
        PutVelocity(gridId, x, y, v, &changed);
        if (changed)
        {
            IncrementEpoch(gridId);
        }
    }

    void PutColor(size_t gridId, int x, int y, Color c)
    {
        bool changed = false;
        PutColor(gridId, x, y, c, &changed);
        if (changed)
        {
            IncrementEpoch(gridId);
        }
    }

    Color GetColor(size_t gridId, int x, int y)
    {
        return Get(gridId)->m_output.Get(x, y);
    }

    Color GetOnColor(size_t gridId)
    {
        return m_buses[gridId].m_onColor.load();
    }

    Color GetOffColor(size_t gridId)
    {
        return m_buses[gridId].m_offColor.load();
    }

    void SetOnColor(size_t gridId, Color c)
    {
         m_buses[gridId].m_onColor.store(c);
    }

    void SetOffColor(size_t gridId, Color c)
    {
         m_buses[gridId].m_offColor.store(c);
    }

    uint8_t GetVelocity(size_t gridId, int x, int y)
    {
        return Get(gridId)->m_input.Get(x, y);
    }

    void IncrementEpoch(size_t gridId)
    {
        Get(gridId)->m_input.m_epoch.fetch_add(1);
        Get(gridId)->m_output.m_epoch.fetch_add(1);
    }

    SmartBusInput::Iterator InputBegin(size_t gridId, uint64_t* epoch)
    {
        auto itr = Get(gridId)->m_input.begin(epoch);
        return itr;
    }

    SmartBusOutput::Iterator OutputBegin(size_t gridId, uint64_t* epoch)
    {
        return Get(gridId)->m_output.begin(epoch);
    }

    void ClearVelocities(size_t gridId)
    {
        bool changed = false;
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                PutVelocity(gridId, i, j, 0, &changed);
            }
        }

        if (changed)
        {
            IncrementEpoch(gridId);
        }
    }

    void ForwardVelocity(size_t gridSrc, size_t gridTrg, uint64_t* epoch)
    {
        bool changed = false;
        for (auto itr = InputBegin(gridSrc, epoch); itr != InputEnd(); ++itr)
        {
            Message m = *itr;
            if (!m.NoMessage())
            {
                PutVelocity(gridTrg, m.m_x, m.m_y, m.m_velocity, &changed);
            }
        }

        if (changed)
        {
            IncrementEpoch(gridTrg);
        }
    }

    void ForwardColors(size_t gridSrc, size_t gridTrg, uint64_t* epoch)
    {
        bool changed = false;
        for (auto itr = OutputBegin(gridSrc, epoch); itr != OutputEnd(); ++itr)
        {
            Message m = *itr;
            if (!m.NoMessage())
            {
                PutColor(gridSrc, m.m_x, m.m_y, m.m_color, &changed);
            }
        }

        if (changed)
        {
            IncrementEpoch(gridTrg);
        }
    }
    
    SmartBusInput::Iterator InputEnd()
    {
        return SmartBusInput::Iterator();
    }

    SmartBusOutput::Iterator OutputEnd()
    {
        return SmartBusOutput::Iterator();
    }
};

#ifndef IOS_BUILD
extern SmartBusHolder g_smartBus;
#endif

inline SmartGrid::Color SmartBusGetOnColor(size_t gridId)
{
#ifndef IOS_BUILD
    return g_smartBus.GetOnColor(gridId);
#else
    return SmartGrid::Color::Off;
#endif
}

inline SmartGrid::Color SmartBusGetOffColor(size_t gridId)
{
#ifndef IOS_BUILD
    return g_smartBus.GetOffColor(gridId);
#else
    return SmartGrid::Color::Off;
#endif
}

inline SmartBusOutput::Iterator SmartBusOutputBegin(size_t gridId, uint64_t* epoch)
{
#ifndef IOS_BUILD
    return g_smartBus.OutputBegin(gridId, epoch);
#else
    return SmartBusOutput::Iterator();
#endif
}

inline SmartBusOutput::Iterator SmartBusOutputEnd()
{
    return SmartBusOutput::Iterator();
}

inline SmartBusInput::Iterator SmartBusInputBegin(size_t gridId, uint64_t* epoch)
{
#ifndef IOS_BUILD
    return g_smartBus.InputBegin(gridId, epoch);
#else
    return SmartBusInput::Iterator();
#endif
}

inline SmartBusInput::Iterator SmartBusInputEnd()
{
    return SmartBusInput::Iterator();
}

inline void SmartBusPutVelocity(size_t gridId, int x, int y, uint8_t v)
{
#ifndef IOS_BUILD
    g_smartBus.PutVelocity(gridId, x, y, v);
#endif
}

inline void SmartBusPutColor(size_t gridId, int x, int y, Color c)
{
#ifndef IOS_BUILD
    g_smartBus.PutColor(gridId, x, y, c);
#endif
}

inline void SmartBusSetOnColor(size_t gridId, Color c)
{
#ifndef IOS_BUILD
    g_smartBus.SetOnColor(gridId, c);
#endif
}

inline void SmartBusSetOffColor(size_t gridId, Color c)
{
#ifndef IOS_BUILD
    g_smartBus.SetOffColor(gridId, c);
#endif
}

inline Color SmartBusGetColor(size_t gridId, int x, int y)
{
#ifndef IOS_BUILD
    return g_smartBus.GetColor(gridId, x, y);
#else
    return SmartGrid::Color::Off;
#endif
}

inline uint8_t SmartBusGetVelocity(size_t gridId, int x, int y)
{
#ifndef IOS_BUILD
    return g_smartBus.GetVelocity(gridId, x, y);
#else
    return 0;
#endif
}

inline void SmartBusClearVelocities(size_t gridId)
{
#ifndef IOS_BUILD
    g_smartBus.ClearVelocities(gridId);
#endif
}

inline void AbstractGrid::OutputToBus()
{
    if (m_gridId != x_numGridIds)
    {
#ifndef IOS_BUILD
        bool changed = false;
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                g_smartBus.PutColor(m_gridId, i, j, GetColor(i, j), &changed);
            }
        }

        g_smartBus.SetOnColor(m_gridId, GetOnColor());
        g_smartBus.SetOffColor(m_gridId, GetOffColor());
        if (changed)
        {
            g_smartBus.IncrementEpoch(m_gridId);
        }
#endif
    }
}

inline void AbstractGrid::OutputToBus(SmartBusColor* bus)
{
    bool changed = false;
    for (int i = x_gridXMin; i < x_gridXMax; ++i)
    {
        for (int j = x_gridYMin; j < x_gridYMax; ++j)
        {
            bus->Put(i, j, GetColor(i, j), &changed);
        }
    }

    if (changed)
    {
        bus->m_epoch.fetch_add(1);
    }
}
    
inline void AbstractGrid::ApplyFromBus()
{
    if (m_gridId != x_numGridIds)
    {
        Apply(SmartBusInputBegin(m_gridId, &m_gridInputEpoch), SmartBusInputEnd());
    }
}
