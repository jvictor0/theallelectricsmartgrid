#pragma once

#include "SmartBus.hpp"
#include "MessageInBus.hpp"

struct PadUI
{
    int m_x;
    int m_y;

    int m_routeId;

    SmartGrid::SmartBusColor* m_smartBus;
    SmartGrid::MessageInBus* m_messageBus;

    PadUI(int x, int y, int routeId, SmartGrid::SmartBusColor* smartBus, SmartGrid::MessageInBus* messageBus)
        : m_x(x)
        , m_y(y)
        , m_routeId(routeId)
        , m_smartBus(smartBus)
        , m_messageBus(messageBus)
    {
    }

    SmartGrid::Color GetColor()
    {
        return m_smartBus->Get(m_x, m_y);
    }

    SmartGrid::MessageIn PressMessage(size_t timestamp)
    {
        return SmartGrid::MessageIn(timestamp, m_routeId, SmartGrid::MessageIn::Mode::PadPress, m_x, m_y, 1);
    }

    SmartGrid::MessageIn ReleaseMessage(size_t timestamp)
    {
        return SmartGrid::MessageIn(timestamp, m_routeId, SmartGrid::MessageIn::Mode::PadRelease, m_x, m_y, 0);
    }

    void OnPress(size_t timestamp)
    {
        m_messageBus->Push(PressMessage(timestamp));
    }

    void OnRelease(size_t timestamp)
    {
        m_messageBus->Push(ReleaseMessage(timestamp));
    }
};

struct PadUIGrid
{
    int m_routeId;

    SmartGrid::SmartBusColor* m_smartBus;
    SmartGrid::MessageInBus* m_messageBus;

    PadUIGrid(int routeId, SmartGrid::SmartBusColor* smartBus, SmartGrid::MessageInBus* messageBus)
        : m_routeId(routeId)
        , m_smartBus(smartBus)
        , m_messageBus(messageBus)
    {
    }
    
    PadUI MkPadUI(int x, int y)
    {
        return PadUI(x, y, m_routeId, m_smartBus, m_messageBus);
    }
};