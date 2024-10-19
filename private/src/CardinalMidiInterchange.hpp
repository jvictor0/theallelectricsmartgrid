#include "plugin.hpp"
#include "plugincontext.hpp"

#include "SmartGrid.hpp"
#include "../../GridControl/Protocol.hpp"

namespace SmartGrid
{

struct MidiInterchangeSingle
{
    CardinalPluginContext* const m_pcontext;
    const CardinalDISTRHO::MidiEvent* m_midiEvents;
    uint32_t m_midiEventsLeft;
    uint32_t m_midiEventFrame;
    uint32_t m_lastProcessCounter;
    MidiWriter m_midiWriter;

    MidiInterchangeSingle()
        : m_pcontext(static_cast<CardinalPluginContext*>(APP))
        , m_midiEvents(nullptr)
        , m_midiEventsLeft(0)
        , m_midiEventFrame(0)
        , m_lastProcessCounter(0)
    {
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

                m_midiEvents = m_pcontext->midiEvents;
                m_midiEventsLeft = m_pcontext->midiEventCount;

                m_midiEventFrame = 0;
            }

            while (m_midiEventsLeft != 0)
            {
                const CardinalDISTRHO::MidiEvent& midiEvent(*m_midiEvents);

                if (midiEvent.frame > m_midiEventFrame)
                {
                    break;
                }

                ++m_midiEvents;
                --m_midiEventsLeft;

                const uint8_t* data;
                
                if (midiEvent.size > CardinalDISTRHO::MidiEvent::kDataSize)
                {
                    data = midiEvent.dataExt;
                }
                else
                {
                    data = midiEvent.data;
                }

                Apply(data, midiEvent.size, gridId);
            }

            ++m_midiEventFrame;
        }
    }

    void Apply(const uint8_t* data, size_t size, size_t gridId)
    {
        MidiReader reader(data, size);
        Event event;
        while (reader.GetNextEvent(event))
        {
            switch (event.m_type)
            {
                case Event::Type::GridTouch:
                {
                    g_smartBus.PutVelocity(gridId, event.GetX(), event.GetY(), event.GetVelocity());
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    void SendMidiFromBus(int64_t frame, size_t gridId)
    {
        if (gridId != x_numGridIds)
        {
            for (auto itr = g_smartBus.OutputBegin(gridId, true /*ignoreChanged*/); itr != g_smartBus.OutputEnd(); ++itr)
            {
                Message msg = *itr;
                if (!msg.NoMessage())
                {
                    Event event;
                    event.m_type = Event::Type::GridColor;
                    event.SetGridIndex(msg.m_x, msg.m_y);
                    event.SetGridColor(msg.m_color.m_red, msg.m_color.m_green, msg.m_color.m_blue);
                    m_midiWriter.Write(event);
                }
            }

            for (auto buf : m_midiWriter)
            {
                midi::Message msg;
                msg.setFrame(frame);
                msg.bytes.clear();
                msg.bytes.resize(buf.size());
                memcpy(msg.bytes.data(), buf.data(), buf.size());
                m_pcontext->writeMidiMessage(msg, 1);
            }
        }
    }    
};
}
