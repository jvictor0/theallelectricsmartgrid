#pragma once

#include "Color.hpp"
#include "MessageIn.hpp"
#include "SmartGrid.hpp"
#include "BasicMidi.hpp"
#include "SmartBus.hpp"

namespace SmartGrid
{
    struct LPMidi
    {
        static uint8_t PosToNote(int x, int y)
        {
            y = x_baseGridSize - y - 1;
            
            // Remap y to match the bottom two button rows of LPProMk3
            //
            if (y == -1)
            {
                y = 9;
            }
            else if (y == -2)
            {
                y = -1;
            }
            
            return 11 + 10 * y + x;
        }
        
        static std::pair<int, int> NoteToPos(uint8_t note)
        {
            if (note < 10)
            {
                // The LaunchpadPro's bottom two rows are in the "wrong" order.
                // Remap.
                //
                return std::make_pair(note - 1, 9);
            }
            
            int y = (note - 11) / 10;
            int x = (note - 11) % 10;

            // Send overflows -1
            //
            if (y == 9)
            {
                y = -1;
            }
            
            if (x == 9)
            {
                x = -1;
                y += 1;
            }                
            
            return std::make_pair(x, 7 - y);
        }

        static bool ShapeSupports(ControllerShape shape, int x, int y)
        {
            if (shape == ControllerShape::LaunchPadX || 
                shape == ControllerShape::LaunchPadMiniMk3)
            {
                return x >= 0 && x < 9 && y >= -1 && y < 8;
            }
            else if (shape == ControllerShape::LaunchPadProMk3)
            {
                return x >= -1 && x < 9 && y >= -1 && y < 10;
            }

            return false;
        }

        static MessageIn FromMidi(BasicMidi msg)
        {
            MessageIn::Mode mode;
            switch (msg.Status())
            {
                case BasicMidi::x_statusNote:
                case BasicMidi::x_statusCC:
                {
                    mode = MessageIn::Mode::PadPress;
                    break;  
                }
                case BasicMidi::x_statusNoteOff:
                {
                    mode = MessageIn::Mode::PadRelease;
                    break;
                }
                case BasicMidi::x_statusPolyAfterTouch:
                {
                    mode = MessageIn::Mode::PadPressure;
                    break;
                }
                default:
                {
                    return MessageIn();
                }
            }

            if (msg.GetValue() == 0)
            {
                mode = MessageIn::Mode::PadRelease;
            }

            std::pair<int, int> pos = NoteToPos(msg.GetNote());

            return MessageIn(msg.m_timestamp, msg.m_routeId, mode, pos.first, pos.second, msg.GetValue());
        }
    };

    struct LPSysexWriter
    {
        static constexpr size_t x_maxMessageSize = 8 + 5 * x_gridMaxSize * x_gridMaxSize;

        ControllerShape m_shape;
        Color m_color[x_gridMaxSize][x_gridMaxSize];
        bool m_set[x_gridMaxSize][x_gridMaxSize];
        SmartBusColor* m_bus;
        uint64_t m_epoch;

        LPSysexWriter()
            : m_shape(ControllerShape::LaunchPadX)
            , m_bus(nullptr)
            , m_epoch(0)
        {
            memset(m_set, 0, sizeof(m_set));
        }

        LPSysexWriter(ControllerShape shape, SmartBusColor* bus)
            : m_shape(shape)
            , m_bus(bus)
            , m_epoch(0)
        {
            memset(m_set, 0, sizeof(m_set));
        }

        void Reset()
        {
            memset(m_set, 0, sizeof(m_set));
            m_epoch = 0;
        }

        size_t Write(uint8_t* buffer)
        {
            bool writtenAny = false;
            size_t pos = 0;

            buffer[pos++] = 240;
            buffer[pos++] = 0;
            buffer[pos++] = 32;
            buffer[pos++] = 41;
            buffer[pos++] = 2;
            
            if (m_shape == ControllerShape::LaunchPadX)
            {
                buffer[pos++] = 12;
            }
            else if (m_shape == ControllerShape::LaunchPadProMk3)
            {
                buffer[pos++] = 14;
            }
            
            buffer[pos++] = 3;
            
            SmartBusOutput::Iterator itr = m_bus->begin(&m_epoch);
            while (!itr.Done()) 
            {
                if (!LPMidi::ShapeSupports(m_shape, itr.m_x, itr.m_y))
                {
                    continue;
                }

                Color color = m_bus->Get(itr.m_x, itr.m_y);
                if (!m_set[itr.GetXPhysical()][itr.GetYPhysical()] || 
                    color != m_color[itr.GetXPhysical()][itr.GetYPhysical()])
                {
                    writtenAny = true;
                    buffer[pos++] = 3;
                    buffer[pos++] = LPMidi::PosToNote(itr.m_x, itr.m_y);
                    buffer[pos++] = color.m_red / 2;
                    buffer[pos++] = color.m_green / 2;
                    buffer[pos++] = color.m_blue / 2;

                    m_set[itr.GetXPhysical()][itr.GetYPhysical()] = true;
                    m_color[itr.GetXPhysical()][itr.GetYPhysical()] = color;
                }

                ++itr;
            }

            if (!writtenAny)
            {
                return 0;
            }
            
            buffer[pos++] = 247;
            return pos;
        }
    };
}