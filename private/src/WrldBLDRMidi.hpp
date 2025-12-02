#pragma once

#include "MessageIn.hpp"
#include "BasicMidi.hpp"
#include "EncoderMidi.hpp"
#include "TheNonagonSquiggleBoyWrldBldr.hpp"
#include "YaeltexHSV.hpp"

namespace SmartGrid
{
    struct WrldBLDRMidi
    {
        static MessageIn FromMidi(BasicMidi msg)
        {
            if (msg.Channel() == 0 || msg.Channel() == 1)
            {
                MessageIn message = EncoderMidi::FromMidi(msg);
                message.m_routeId = static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::Encoder);
                return message;
            }
            else if (msg.Channel() == 2)
            {
                int routeId = static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::Analog);
                MessageIn::Mode mode = MessageIn::Mode::ParamSet7;
                return MessageIn(msg.m_timestamp, routeId, mode, msg.GetCC(), 0, msg.GetValue());
            }
            else if (msg.Channel() == 3)
            {
                int routeId = msg.GetCC() < 64 ? static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::LeftGrid) : static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::RightGrid);
                int x = msg.GetNote() % 8;
                int y = (msg.GetNote() / 8) % 8;
                MessageIn::Mode mode = msg.GetValue() > 0 ? MessageIn::Mode::PadPress : MessageIn::Mode::PadRelease;
                return MessageIn(msg.m_timestamp, routeId, mode, x, y, msg.GetValue());
            }
            else if (msg.Channel() == 4)
            {
                int routeId = static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid);
                int x = msg.GetCC() % 8;
                int y = msg.GetCC() / 8;
                MessageIn::Mode mode = msg.GetValue() > 0 ? MessageIn::Mode::PadPress : MessageIn::Mode::PadRelease;
                return MessageIn(msg.m_timestamp, routeId, mode, x, y, msg.GetValue());
            }
            else if (msg.Channel() == 14)
            {
                int routeId = static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::Analog);
                MessageIn::Mode mode = MessageIn::Mode::ParamSet7;
                return MessageIn(msg.m_timestamp, routeId, mode, msg.GetCC() + 2, 0, msg.GetValue());
            }               
            else
            {
                return MessageIn();
            }
        }
    };

    struct WrldBLDRGridMidiWriter
    {
        Color m_color[x_gridMaxSize][x_gridMaxSize];
        bool m_set[x_gridMaxSize][x_gridMaxSize];
        SmartBusColor* m_bus;
        uint8_t m_channel;
        uint8_t m_start;
        uint64_t m_epoch;

        WrldBLDRGridMidiWriter()
            : m_bus(nullptr)
            , m_channel(0)
            , m_start(0)
            , m_epoch(0)
        {
            memset(m_set, 0, sizeof(m_set));
        }

        WrldBLDRGridMidiWriter(SmartBusColor* bus, uint8_t channel, uint8_t start)
            : m_bus(bus)
            , m_channel(channel)
            , m_start(start)
            , m_epoch(0)
        {
            memset(m_set, 0, sizeof(m_set));
        }

        void Reset()
        {
            memset(m_set, 0, sizeof(m_set));
            m_epoch = 0;
        }

        struct Iterator
        {
            SmartBusOutput::Iterator m_itr;
            WrldBLDRGridMidiWriter* m_owner;
            SmartBusOutput* m_bus;
            uint8_t m_channel;
            uint8_t m_start;

            Iterator(WrldBLDRGridMidiWriter* owner)
                : m_itr(owner->m_bus)
                , m_owner(owner)
                , m_bus(owner->m_bus)
                , m_channel(owner->m_channel)
                , m_start(owner->m_start)
            {
                Advance();
            }
            
            bool Done() const
            {
                return m_itr.Done();
            }

            void Advance()
            {
                while (!Done())
                {
                    Color color = m_bus->Get(m_itr.m_x, m_itr.m_y);
                    if (!m_owner->m_set[m_itr.GetXPhysical()][m_itr.GetYPhysical()] || 
                        color != m_owner->m_color[m_itr.GetXPhysical()][m_itr.GetYPhysical()])
                    {
                        m_owner->m_set[m_itr.GetXPhysical()][m_itr.GetYPhysical()] = true;
                        m_owner->m_color[m_itr.GetXPhysical()][m_itr.GetYPhysical()] = color;
                        break;
                    }

                    ++m_itr;
                }
            }

            Iterator& operator++()
            {
                ++m_itr;
                Advance();
                return *this;
            }

            BasicMidi operator*() const
            {
                uint8_t cc = m_start + m_itr.m_y * 8 + m_itr.m_x;
                Color color = m_bus->Get(m_itr.m_x, m_itr.m_y);
                uint8_t value = RGBToYaeltexCode(color.m_red, color.m_green, color.m_blue);
                return BasicMidi::CC(0, -1, m_channel, cc, value);
            }
        };
    };

    struct WrldBLDRMidiWriter
    {
        WrldBLDRGridMidiWriter m_leftGrid;
        WrldBLDRGridMidiWriter m_rightGrid;
        WrldBLDRGridMidiWriter m_auxGrid;

        WrldBLDRMidiWriter(TheNonagonSquiggleBoyWrldBldr* wrldBldr)
            : m_leftGrid(&wrldBldr->m_uiState.m_colorBus[static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::LeftGrid) - TheNonagonSquiggleBoyWrldBldr::x_launchpadStartRouteId], 3, 0)
            , m_rightGrid(&wrldBldr->m_uiState.m_colorBus[static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::RightGrid) - TheNonagonSquiggleBoyWrldBldr::x_launchpadStartRouteId], 3, 64)
            , m_auxGrid(&wrldBldr->m_uiState.m_colorBus[static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid) - TheNonagonSquiggleBoyWrldBldr::x_launchpadStartRouteId], 4, 0)
        {
        }

        void Reset()
        {
            m_leftGrid.Reset();
            m_rightGrid.Reset();
            m_auxGrid.Reset();
        }

        struct Iterator
        {
            WrldBLDRGridMidiWriter::Iterator m_leftGridItr;
            WrldBLDRGridMidiWriter::Iterator m_rightGridItr;
            WrldBLDRGridMidiWriter::Iterator m_auxGridItr;

            Iterator(WrldBLDRMidiWriter* owner)
                : m_leftGridItr(&owner->m_leftGrid)
                , m_rightGridItr(&owner->m_rightGrid)
                , m_auxGridItr(&owner->m_auxGrid)
            {
            }

            bool Done() const
            {
                return m_leftGridItr.Done() && m_rightGridItr.Done() && m_auxGridItr.Done();
            }

            Iterator& operator++()
            {
                Advance();
                return *this;
            }

            void Advance()
            {
                if (!m_leftGridItr.Done())
                {
                    ++m_leftGridItr;
                }
                else if (!m_rightGridItr.Done())
                {
                    ++m_rightGridItr;
                }
                else if (!m_auxGridItr.Done())
                {
                    ++m_auxGridItr;
                }
            }

            BasicMidi operator*() const
            {
                if (!m_leftGridItr.Done())
                {
                    return *m_leftGridItr;
                }
                else if (!m_rightGridItr.Done())
                {
                    return *m_rightGridItr;
                }
                else if (!m_auxGridItr.Done())
                {
                    return *m_auxGridItr;
                }
                else
                {
                    return BasicMidi();
                }
            }
        };

        Iterator Begin()
        {
            return Iterator(this);
        }
    };
}