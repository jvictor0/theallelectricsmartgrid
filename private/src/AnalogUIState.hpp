#pragma once

#include <atomic>
#include "MessageInBus.hpp"

struct AnalogUI
{
    int m_routeId;
    std::atomic<float>* m_value;
    SmartGrid::MessageInBus* m_messageBus;
    int m_x;

    AnalogUI(int routeId, int x, std::atomic<float>* value, SmartGrid::MessageInBus* messageBus)
        : m_routeId(routeId)
        , m_value(value)
        , m_messageBus(messageBus)
        , m_x(x)
    {
    }

    float GetValue()
    {
        return m_value->load();
    }

    void SendValue(size_t timestamp, float value)
    {
        m_messageBus->Push(SmartGrid::MessageIn(timestamp, m_routeId, SmartGrid::MessageIn::Mode::ParamSet14, m_x, 0, value * ((1 << 14) - 1)));
    }
};

template<size_t Size>
struct AnalogUIState
{
    std::atomic<float> m_values[Size];

    AnalogUIState()
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_values[i] = 0;
        }
    }

    AnalogUI MkAnalogUI(int routeId, int x, SmartGrid::MessageInBus* messageBus)
    {
        return AnalogUI(routeId, x, &m_values[x], messageBus);
    }

    void SetValue(size_t i, float value)
    {
        m_values[i].store(value);
    }
};

