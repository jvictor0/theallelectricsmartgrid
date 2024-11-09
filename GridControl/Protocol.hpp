#pragma once
#include "Socket.hpp"
#include "Event.hpp"
#include <vector>
#include <cstdint>
#include <cassert>

struct MultiWriter
{
    std::vector<std::vector<uint8_t>> m_buffers[static_cast<int>(Event::Type::NumTypes)];
    ColorRemember m_colorRemember;

    void Clear()
    {
        for (int i = 0; i < static_cast<int>(Event::Type::NumTypes); ++i)
        {
            if (!m_buffers[i].empty())
            {
                m_buffers[i].resize(1);
                m_buffers[i].back().clear();
            }
        }
    }

    std::vector<uint8_t>& GetWorkingBuffer(Event::Type type)
    {
        return m_buffers[static_cast<int>(type)].back();
    }

    void Write(const Event& event)
    {
        if (!event.RememberColor(m_colorRemember))
        {
            return;
        }
        
        if (m_buffers[static_cast<int>(event.m_type)].empty())
        {
            m_buffers[static_cast<int>(event.m_type)].push_back(std::vector<uint8_t>());
        }

        if (GetWorkingBuffer(event.m_type).size() > 2 && GetWorkingBuffer(event.m_type)[1] == 127)
        {
            m_buffers[static_cast<int>(event.m_type)].push_back(std::vector<uint8_t>());
        }

        if (GetWorkingBuffer(event.m_type).empty())
        {
            GetWorkingBuffer(event.m_type).push_back(static_cast<uint8_t>(event.m_type));
            GetWorkingBuffer(event.m_type).push_back(0);
        }

        GetWorkingBuffer(event.m_type)[1] += 1;

        GetWorkingBuffer(event.m_type).push_back(event.m_index);

        for (int i = 0; i < Event::NumValues(event.m_type); ++i)
        {
            GetWorkingBuffer(event.m_type).push_back(event.m_value[i]);
        }
    }

    struct Iterator
    {
        size_t m_type;
        size_t m_index;
        MultiWriter* m_writer;

        Iterator(MultiWriter* writer)
            : m_type(0)
            , m_index(0)
            , m_writer(writer)
        {
            Advance();
        }

        Iterator()
            : m_type(static_cast<size_t>(Event::Type::NumTypes))
            , m_index(0)
            , m_writer(nullptr)
        {
        }

        void Advance()
        {
            while (m_type < static_cast<size_t>(Event::Type::NumTypes) &&
                   (m_index >= m_writer->m_buffers[m_type].size() || m_writer->m_buffers[m_type][m_index].empty()))
            {
                m_type += 1;
                m_index = 0;
            }
        }

        void operator++()
        {
            m_index += 1;
            Advance();
        }

        bool operator!=(const Iterator& other) const
        {
            return m_type != other.m_type || m_index != other.m_index;
        }

        std::vector<uint8_t>& operator*()
        {
            return m_writer->m_buffers[m_type][m_index];
        }
    };

    Iterator begin()
    {
        return Iterator(this);
    }

    Iterator end()
    {
        return Iterator();
    }
};

struct Protocol
{
    Socket m_socket;
    std::vector<Event> m_toSend;
    ColorRemember m_colorRemember;
    
    void GetEvents(std::vector<Event>& events)
    {
        Event::Type type;
        if (m_socket.Read(reinterpret_cast<uint8_t*>(&type), sizeof(type), false) == 0)
        {
            return;
        }
        
        uint8_t size = 0;
        m_socket.Read(reinterpret_cast<uint8_t*>(&size), sizeof(size), true);

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

        m_socket.Write(reinterpret_cast<uint8_t*>(&m_toSend[0].m_type), sizeof(m_toSend[0].m_type));
        uint8_t size = m_toSend.size();
        m_socket.Write(reinterpret_cast<uint8_t*>(&size), sizeof(size));

        for (const Event& event : m_toSend)
        {
            event.Serialize(&m_socket);
        }

        m_socket.Flush();
        m_toSend.clear();
    }

    void AddEvent(const Event& event)
    {
        if (!event.RememberColor(m_colorRemember))
        {
            return;
        }
        
        if (!m_toSend.empty() && m_toSend[0].m_type != event.m_type)
        {
            SendEvents();
        }
        
        m_toSend.push_back(event);
        if (m_toSend.size() == 255)
        {
            SendEvents();
        }
    }

    void SendWriter(MultiWriter& writer)
    {
        for (std::vector<uint8_t>& buffer : writer)
        {
            if (buffer.empty())
            {
                continue;
            }

            m_socket.Write(buffer.data(), buffer.size());
        }

        m_socket.Flush();
        writer.Clear();
    }
    
    bool IsConnected() const
    {
        return m_socket.IsOpen();
    }
    
    void Connect(const char* ip, uint16_t port)
    {
        m_socket.Connect(ip, port);
    }

    void Handshake(uint8_t clientId)
    {
        m_socket.Write(reinterpret_cast<uint8_t*>(&clientId), sizeof(clientId));
        m_socket.Flush();
    }
};

inline std::string ArrayToString(const uint8_t* data, size_t size)
{
    std::string result = "[";
    for (size_t i = 0; i < size; ++i)
    {
        result += std::to_string(data[i]) + " ";
    }

    result += "]";
    return result;
}

inline std::string VectorToString(const std::vector<uint8_t>& vec)
{
    return ArrayToString(vec.data(), vec.size());
}
