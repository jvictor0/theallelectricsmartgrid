#pragma once
#include "Socket.hpp"
#include "Event.hpp"
#include <vector>
#include <cstdint>

struct Protocol
{
    Socket m_socket;
    std::vector<Event> m_toSend;
    
    void GetEvents(std::vector<Event>& events)
    {
        Event::Type type;
        if (m_socket.Read(reinterpret_cast<char*>(&type), sizeof(type), false) == 0)
        {
            return;
        }
        
        uint8_t size = 0;
        m_socket.Read(reinterpret_cast<char*>(&size), sizeof(size), true);

        for (uint8_t i = 0; i < size; ++i)
        {
            events.push_back(Event::Deserialize(&m_socket, type));
        }
    }

    void SendEvents()
    {
        if (m_toSend.empty())
        {
            return;
        }

        m_socket.Write(reinterpret_cast<char*>(&m_toSend[0].m_type), sizeof(m_toSend[0].m_type));
        uint8_t size = m_toSend.size();
        m_socket.Write(reinterpret_cast<char*>(&size), sizeof(size));

        for (const Event& event : m_toSend)
        {
            event.Serialize(&m_socket);
        }

        m_socket.Flush();
        m_toSend.clear();
    }

    void AddEvent(const Event& event)
    {
        m_toSend.push_back(event);
        if (m_toSend.size() == 255)
        {
            SendEvents();
        }
    }

    bool IsConnected() const
    {
        return m_socket.IsOpen();
    }
    
    void Connect(const char* ip, uint16_t port)
    {
        m_socket.Connect(ip, port);
    }
};
