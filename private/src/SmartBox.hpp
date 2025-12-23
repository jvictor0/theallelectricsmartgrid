#include "plugin.hpp"
#include "plugincontext.hpp"

#include "SmartGrid.hpp"
#include "../../GridControl/Protocol.hpp"

#include <iostream>
#include <fstream>

namespace SmartGrid
{

struct MidiInterchangeSingle
{
    CardinalPluginContext* const m_pcontext;
    uint32_t m_lastProcessCounter;
    MultiWriter m_multiWriter;
    Protocol m_protocol;

    std::ofstream m_logFile;

    MidiInterchangeSingle()
        : m_pcontext(static_cast<CardinalPluginContext*>(APP))
        , m_lastProcessCounter(0)
    {
        m_logFile.open("/tmp/joyolog.txt");
        m_logFile << ("MidiInterchangeSingle") << std::endl;
        m_protocol.Connect("127.0.0.1", 7040);
        m_protocol.Handshake(1);
    }
    
    void ApplyMidiToBus(int64_t frame, size_t gridId)
    {
        if (gridId != x_numGridIds)
        {
            const uint32_t processCounter = m_pcontext->processCounter;
            const bool processCounterChanged = m_lastProcessCounter != processCounter;

            if (processCounterChanged)
            {
                m_lastProcessCounter = processCounter;

                std::vector<Event> events;
                m_protocol.GetEvents(events);
                for (const Event& event : events)
                {
                    m_logFile << "event " << event.ToString().c_str() << std::endl;
                    switch (event.m_type)
                    {
                        case Event::Type::GridTouch:
                        {
                            SmartBusPutVelocity(gridId, event.GetX(), event.GetY(), event.GetVelocity());
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }
            }
        }
    }

    void SendMidiFromBus(int64_t frame, size_t gridId)
    {
        if (gridId != x_numGridIds)
        {
            m_multiWriter.Clear();
            for (auto itr = SmartBusOutputBegin(gridId, true /*ignoreChanged*/); itr != SmartBusOutputEnd(); ++itr)
            {
                Message msg = *itr;
                if (!msg.NoMessage() && GridButton::InRange(msg.m_x, msg.m_y))
                {
                    Event event;
                    event.m_type = Event::Type::GridColor;
                    event.SetGridIndex(msg.m_x, msg.m_y);
                    event.SetGridColor(msg.m_color.m_red, msg.m_color.m_green, msg.m_color.m_blue);
                    m_multiWriter.Write(event);
                }
            }

            m_protocol.SendWriter(m_multiWriter);
        }
    }    
};

}
