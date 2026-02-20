#pragma once
#include <cstdint>
#include <cstring>

namespace SmartGrid
{
    struct BasicMidi
    {
        size_t m_timestamp;
        int m_routeId;
        uint8_t m_msg[3];

        static constexpr uint8_t x_statusNote = 0x90;
        static constexpr uint8_t x_statusNoteOff = 0x80;
        static constexpr uint8_t x_statusCC = 0xB0;
        static constexpr uint8_t x_statusPitchBend = 0xE0;
        static constexpr uint8_t x_statusAfterTouch = 0xD0;
        static constexpr uint8_t x_statusPolyAfterTouch = 0xA0;
        static constexpr uint8_t x_statusTransportStart = 0xFA;
        static constexpr uint8_t x_statusTransportStop = 0xFC;
        static constexpr uint8_t x_statusClock = 0xF8;

        BasicMidi()
            : m_timestamp(0)
            , m_routeId(-1)
            , m_msg{}
        {
            memset(m_msg, 0, sizeof(m_msg));
        }

        BasicMidi(size_t timestamp, int routeId, uint8_t status, uint8_t data1, uint8_t data2)
            : m_timestamp(timestamp)
            , m_routeId(routeId)
            , m_msg{}
        {
            m_msg[0] = status;
            m_msg[1] = data1;
            m_msg[2] = data2;
        }

        BasicMidi(size_t timestamp, int routeId, uint8_t* msg)
            : m_timestamp(timestamp)
            , m_routeId(routeId)
            , m_msg{}
        {
            memcpy(m_msg, msg, sizeof(m_msg));
        }

        static BasicMidi Note(size_t timestamp, int routeId, uint8_t channel, uint8_t note, uint8_t velocity)
        {
            if (velocity == 0)
            {
                return NoteOff(timestamp, routeId, channel, note);
            }
            else
            {
                return BasicMidi(timestamp, routeId, x_statusNote | (channel & 0x0F), note, velocity);
            }
        }

        static BasicMidi NoteOff(size_t timestamp, int routeId, uint8_t channel, uint8_t note)
        {
            return BasicMidi(timestamp, routeId, x_statusNoteOff | (channel & 0x0F), note, 0);
        }
        
        static BasicMidi CC(size_t timestamp, int routeId, uint8_t channel, uint8_t cc, uint8_t value)
        {
            return BasicMidi(timestamp, routeId, x_statusCC | (channel & 0x0F), cc, value);
        }

        static BasicMidi PitchBend(size_t timestamp, int routeId, uint8_t channel, uint16_t value)
        {
            return BasicMidi(timestamp, routeId, x_statusPitchBend | (channel & 0x0F), (value >> 7) & 0x7F, value & 0x7F);
        }
        
        static BasicMidi AfterTouch(size_t timestamp, int routeId, uint8_t channel, uint8_t value)
        {
            return BasicMidi(timestamp, routeId, x_statusAfterTouch | (channel & 0x0F), value, 0);
        }

        static BasicMidi PolyAfterTouch(size_t timestamp, int routeId, uint8_t channel, uint8_t note, uint8_t value)
        {
            return BasicMidi(timestamp, routeId, x_statusPolyAfterTouch | (channel & 0x0F), note, value);
        }

        static BasicMidi TransportStart(size_t timestamp)
        {
            return BasicMidi(timestamp, -1, x_statusTransportStart, 0, 0);
        }
        
        static BasicMidi TransportStop(size_t timestamp)
        {
            return BasicMidi(timestamp, -1, x_statusTransportStop, 0, 0);
        }

        static BasicMidi Clock(size_t timestamp)
        {
            return BasicMidi(timestamp, -1, x_statusClock, 0, 0);
        }

        uint8_t Channel()
        {
            return m_msg[0] & 0x0F;
        }

        uint8_t Status()
        {
            return m_msg[0] & 0xF0;
        }

        uint8_t GetNote()
        {
            return m_msg[1];
        }

        uint8_t GetCC()
        {
            return m_msg[1];
        }

        uint16_t GetPitchBend()
        {
            return (m_msg[1] << 7) | m_msg[2];
        }

        uint8_t GetAfterTouch()
        {
            return m_msg[1];
        }

        uint8_t GetPolyAfterTouch()
        {
            return m_msg[2];
        }

        uint8_t GetValue()
        {
            return m_msg[2];
        }

        uint8_t GetVelocity()
        {
            return m_msg[2];
        }

        bool IsClock()
        {
            return m_msg[0] == x_statusClock;
        }

        size_t Size()
        {
            return Status() == 0xF0 ? 1 : 3;
        }
    };
}
