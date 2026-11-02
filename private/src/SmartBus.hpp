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

    std::atomic<BusInput>* GetAtomic(int i, int j)
    {
        return &m_messages[i - x_gridXMin][j - x_gridYMin];
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

    void GetPhysicalFromLogical(int logicalX, int logicalY, int* outPhysicalX, int* outPhysicalY)
    {
        int physicalX = logicalX - x_gridXMin;
        int physicalY = logicalY - x_gridYMin;

        if (physicalX < 0 || x_gridMaxSize <= physicalX || physicalY < 0 || x_gridMaxSize <= physicalY)
        {
            return;
        }

        *outPhysicalX = physicalX;
        *outPhysicalY = physicalY;
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

typedef SmartBusGeneric<Color> SmartBusColor;

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
