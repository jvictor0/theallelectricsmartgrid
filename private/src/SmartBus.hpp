#pragma once
#include "SmartGrid.hpp"
#include <atomic>

template<class BusInput>
struct SmartBusGeneric
{
    std::atomic<bool> m_changed;
    std::atomic<BusInput> m_messages[x_gridMaxSize][x_gridMaxSize];

    SmartBusGeneric()
        : m_changed(false)
    {
    }
    
    void Put(int i, int j, BusInput payload)
    {
        Store(i + 1, j + 2, payload);
    }

    BusInput Get(int i, int j)
    {
        return Load(i + 1, j + 2);
    }

    void Store(size_t i, size_t j, BusInput payload)
    {
        BusInput newPayload = m_messages[i][j].exchange(payload);
        if (payload != newPayload)
        {
            m_changed.store(true);
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
    };

    Iterator begin(bool ignoreChanged)
    {
        if (ignoreChanged)
        {
            return Iterator(this);
        }
                
        // If nothing has changed, don't bother.
        //
        if (!m_changed.load())
        {
            return end();
        }
        else
        {
            m_changed.store(false);
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

struct SmartBus
{
    SmartBusInput m_input;
    SmartBusOutput m_output;
};

struct SmartBusHolder
{
    SmartBus m_buses[x_numGridIds];

    SmartBus* Get(size_t gridId)
    {
        return &m_buses[gridId];
    }

    void PutColor(size_t gridId, int x, int y, Color c)
    {
        Get(gridId)->m_output.Put(x, y, c);
    }

    void PutVelocity(size_t gridId, int x, int y, uint8_t v)
    {
        Get(gridId)->m_input.Put(x, y, v);
    }

    Color GetColor(size_t gridId, int x, int y)
    {
        return Get(gridId)->m_output.Get(x, y);
    }

    uint8_t GetVelocity(size_t gridId, int x, int y)
    {
        return Get(gridId)->m_input.Get(x, y);
    }

    SmartBusInput::Iterator InputBegin(size_t gridId, bool ignoreChanged)
    {
        return Get(gridId)->m_input.begin(ignoreChanged);
    }

    SmartBusOutput::Iterator OutputBegin(size_t gridId, bool ignoreChanged)
    {
        return Get(gridId)->m_output.begin(ignoreChanged);
    }

    void ClearVelocities(size_t gridId)
    {
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                PutVelocity(gridId, i, j, 0);
            }
        }        
    }

    void ForwardVelocity(size_t gridSrc, size_t gridTrg)
    {
        for (auto itr = InputBegin(gridSrc, false /*ignoreChanged*/); itr != InputEnd(); ++itr)
        {
            Message m = *itr;
            if (!m.NoMessage())
            {
                PutVelocity(gridTrg, m.m_x, m.m_y, m.m_velocity);
            }
        }
    }

    void ForwardColors(size_t gridSrc, size_t gridTrg)
    {
        for (auto itr = OutputBegin(gridSrc, false /*ignoreChanged*/); itr != OutputEnd(); ++itr)
        {
            Message m = *itr;
            if (!m.NoMessage())
            {
                PutColor(gridSrc, m.m_x, m.m_y, m.m_color);
            }
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

extern SmartBusHolder g_smartBus;

inline void AbstractGrid::OutputToBus()
{
    if (m_gridId != x_numGridIds)
    {
        for (int i = x_gridXMin; i < x_gridXMax; ++i)
        {
            for (int j = x_gridYMin; j < x_gridYMax; ++j)
            {
                g_smartBus.PutColor(m_gridId, i, j, GetColor(i, j));
            }
        }
    }
}

inline void AbstractGrid::ApplyFromBus(bool ignoreChanged)
{
    if (m_gridId != x_numGridIds)
    {
        Apply(g_smartBus.InputBegin(m_gridId, ignoreChanged), g_smartBus.InputEnd());
    }
}
