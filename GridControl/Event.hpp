#pragma once
#include "Socket.hpp"
#include "GridButton.hpp"
#include <cstdint>
#include <stdexcept>

struct Event
{
    enum class Type : uint8_t
    {
        None,
        GridTouch,
        GridColor,
    };

    Type m_type;
    uint8_t m_index;
    uint8_t m_value[4];

    Event()
        : m_type(Type::None),
          m_index(0)
    {
    }

    static size_t NumValues(Type type)
    {
        switch (type)
        {
        case Type::GridTouch:
            return 1;
        case Type::GridColor:
            return 3;
        case Type::None:
        default:
            throw std::runtime_error("Invalid event type");
        }
    }

    uint8_t GetX() const
    {
        return GridButton::ToX(m_index);
    }

    uint8_t GetY() const
    {
        return GridButton::ToY(m_index);
    }

    void SetGridIndex(uint8_t x, uint8_t y)
    {
        m_index = GridButton::ToIndex(x, y);
    }

    uint8_t GetVelocity() const
    {
        return m_value[0];
    }

    void SetGridVelocity(uint8_t value)
    {
        m_value[0] = value;
    }

    void SetGridColor(uint8_t r, uint8_t g, uint8_t b)
    {
        m_value[0] = r;
        m_value[1] = g;
        m_value[2] = b;
    }

    void Serialize(Socket* socket) const
    {
        socket->Write(reinterpret_cast<const char*>(&m_index), sizeof(m_index));
        socket->Write(reinterpret_cast<const char*>(&m_value), sizeof(uint8_t) * NumValues(m_type));
    }

    static Event Deserialize(Socket* socket, Type type)
    {
        Event event;
        event.m_type = type;
        socket->Read(reinterpret_cast<char*>(&event.m_index), sizeof(event.m_index), true);
        socket->Read(reinterpret_cast<char*>(&event.m_value), sizeof(uint8_t) * NumValues(type), true);
        return event;
    }
};