#pragma once

#include "MessageIn.hpp"
#include "BasicMidi.hpp"
#include "EncoderMidi.hpp"
#include "TheNonagonSquiggleBoyWrldBldr.hpp"
#include "YaeltexHSV.hpp"
#include "EncoderUIState.hpp"

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
            else if (msg.Channel() == 3 || msg.Channel() == 4)
            {
                int routeId = msg.Channel() == 3 ? static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::LeftGrid) : static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::RightGrid);
                int x = msg.GetNote() % 8;
                int y = msg.GetNote() / 8;
                MessageIn::Mode mode = msg.GetValue() > 0 ? MessageIn::Mode::PadPress : MessageIn::Mode::PadRelease;
                return MessageIn(msg.m_timestamp, routeId, mode, x, y, msg.GetValue());
            }
            else if (msg.Channel() == 5)
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

    struct YaeltexColorSysexBuffer
    {
        static constexpr size_t x_maxSize = 11 + 4 * 64 + 1;
        uint8_t m_buffer[x_maxSize];
        size_t m_size;

        YaeltexColorSysexBuffer()
            : m_size(0)
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            m_buffer[0] = 0xF0;
            m_buffer[1] = 0x79;
            m_buffer[2] = 0x74;
            m_buffer[3] = 0x78;
            m_buffer[4] = 0x00;
            m_buffer[5] = 0x01;
            m_buffer[6] = 0x00;
            m_buffer[7] = 0x20;
            m_size = 8;
        }

        void Reset()
        {
            m_size = 8;
        }

        bool HasAny()
        {
            return m_size > 10;
        }

        void WriteChannel(uint8_t channel)
        {
            m_buffer[8] = channel;
            ++m_size;
        }

        void WriteData(uint8_t cc, Color color)
        {
            m_buffer[m_size] = cc;
            m_buffer[m_size + 1] = color.m_red / 2;
            m_buffer[m_size + 2] = color.m_green / 2;
            m_buffer[m_size + 3] = color.m_blue / 2;
            m_size += 4;
        }

        void WriteEnd()
        {
            m_buffer[m_size] = 0xF7;
            ++m_size;
        }
    };

    struct WrldBLDRColorMidiWriter
    {
        enum class Mode
        {
            EncoderButton,
            EncoderIndicator,
            Grid
        };

        Color m_color[x_gridMaxSize][x_gridMaxSize];
        bool m_set[x_gridMaxSize][x_gridMaxSize];
        uint8_t m_cooldown[x_gridMaxSize][x_gridMaxSize];
        SmartBusColor* m_bus;
        EncoderBankUIState* m_encoderState;
        uint8_t m_channel;
        uint64_t m_epoch;
        Mode m_mode;

        WrldBLDRColorMidiWriter()
            : m_bus(nullptr)
            , m_channel(0)
            , m_epoch(0)
        {
            memset(m_set, 0, sizeof(m_set));
            memset(m_cooldown, 0, sizeof(m_cooldown));
        }

        WrldBLDRColorMidiWriter(SmartBusColor* bus, uint8_t channel, Mode mode)
            : m_bus(bus)
            , m_channel(channel)
            , m_epoch(0)
            , m_mode(mode)
        {
            memset(m_set, 0, sizeof(m_set));
            memset(m_cooldown, 0, sizeof(m_cooldown));
        }

        WrldBLDRColorMidiWriter(EncoderBankUIState* encoderState, uint8_t channel, Mode mode)
            : m_encoderState(encoderState)
            , m_channel(channel)
            , m_epoch(0)
        {
            memset(m_set, 0, sizeof(m_set));
            memset(m_cooldown, 0, sizeof(m_cooldown));
            m_mode = mode;
        }

        void Reset()
        {
            memset(m_set, 0, sizeof(m_set));
            m_epoch = 0;
            memset(m_cooldown, 0, sizeof(m_cooldown));
        }

        void ProcessCoolDown()
        {
            SmartBusOutput::Iterator coolDownItr(m_bus);
            while (!coolDownItr.Done())
            {
                if (m_cooldown[coolDownItr.GetXPhysical()][coolDownItr.GetYPhysical()] > 0)
                {
                    --m_cooldown[coolDownItr.GetXPhysical()][coolDownItr.GetYPhysical()];
                }

                ++coolDownItr;
            }
        }

        void Write(YaeltexColorSysexBuffer& buffer, size_t& budget)
        {
            buffer.WriteChannel(m_channel);
            if (m_mode == Mode::EncoderButton || m_mode == Mode::EncoderIndicator)
            {
                WriteEncoder(buffer, budget);
            }
            else
            {
                WriteGrid(buffer, budget);
            }

            buffer.WriteEnd();
        }

        void WriteEncoder(YaeltexColorSysexBuffer& buffer, size_t& budget)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 4; ++j)
                {
                    if (budget == 0)
                    {
                        return;
                    }

                    if (m_cooldown[i][j] > 0)
                    {
                        continue;
                    }

                    Color color; 
                    if (m_mode == Mode::EncoderButton)
                    {
                        color = m_encoderState->GetColor(i, j).AdjustBrightness(m_encoderState->GetBrightness(i, j));
                    }
                    else if (m_mode == Mode::EncoderIndicator)
                    {
                        size_t voice = m_encoderState->GetNumVoices() * m_encoderState->GetCurrentTrack();
                        color = m_encoderState->GetIndicatorColor(voice);
                    }

                    if (!m_set[i][j] || color != m_color[i][j])
                    {
                        buffer.WriteData(i + j * 4, color);
                        m_cooldown[i][j] = 1;
                        m_set[i][j] = true;
                        m_color[i][j] = color;
                        --budget;
                    }
                }
            }
        }

        void WriteGrid(YaeltexColorSysexBuffer& buffer, size_t& budget)
        {
            SmartBusOutput::Iterator itr(m_bus);
            for (; !itr.Done(); ++itr)
            {
                if (budget == 0)
                {
                    return;
                }

                if (itr.m_x < 0 || itr.m_y < 0 || 8 <= itr.m_x || 8 <= itr.m_y)
                {
                    continue;
                }

                if (m_cooldown[itr.GetXPhysical()][itr.GetYPhysical()] > 0)          
                {
                    continue;
                }

                Color color = m_bus->Get(itr.m_x, itr.m_y);
                if (!m_set[itr.GetXPhysical()][itr.GetYPhysical()] || 
                    color != m_color[itr.GetXPhysical()][itr.GetYPhysical()])
                {
                    uint8_t cc = itr.m_x + itr.m_y * 8;
                    buffer.WriteData(cc, color);
                    m_cooldown[itr.GetXPhysical()][itr.GetYPhysical()] = 1;
                    m_set[itr.GetXPhysical()][itr.GetYPhysical()] = true;
                    m_color[itr.GetXPhysical()][itr.GetYPhysical()] = color;
                    --budget;
                }
            }
        }

        void WriteFill(YaeltexColorSysexBuffer& buffer, Color color)
        {
            buffer.WriteChannel(m_channel);
            if (m_mode == Mode::EncoderButton || m_mode == Mode::EncoderIndicator)
            {
                for (size_t i = 0; i < 4; ++i)
                {
                    for (size_t j = 0; j < 4; ++j)
                    {
                        buffer.WriteData(i + j * 4, color);
                    }
                }
            }
            else
            {
                for (size_t y = 0; y < 8; ++y)
                {
                    for (size_t x = 0; x < 8; ++x)
                    {
                        buffer.WriteData(x + y * 8, color);
                    }
                }
            }

            buffer.WriteEnd();
        }
    };

    struct WrldBLDRIndicatorMidiWriter
    {
        uint8_t m_values[4][4];
        bool m_sent[4][4];
        uint8_t m_cooldown[4][4];
        EncoderBankUIState* m_encoderState;

        WrldBLDRIndicatorMidiWriter()
            : m_encoderState(nullptr)
        {
            memset(m_values, 0, sizeof(m_values));
            memset(m_sent, 0, sizeof(m_sent));
            memset(m_cooldown, 0, sizeof(m_cooldown));
        }

        WrldBLDRIndicatorMidiWriter(EncoderBankUIState* encoderState)
            : m_encoderState(encoderState)
        {
            memset(m_values, 0, sizeof(m_values));
            memset(m_sent, 0, sizeof(m_sent));
            memset(m_cooldown, 0, sizeof(m_cooldown));
        }

        void Reset()
        {
            memset(m_sent, 0, sizeof(m_sent));
            memset(m_cooldown, 0, sizeof(m_cooldown));
        }

        void ProcessCoolDown()
        {
            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 4; ++j)
                {
                    if (m_cooldown[i][j] > 0)
                    {
                        --m_cooldown[i][j];
                    }
                }
            }
        }

        struct Iterator
        {
            size_t m_x;
            size_t m_y;
            WrldBLDRIndicatorMidiWriter* m_owner;

            Iterator(WrldBLDRIndicatorMidiWriter* owner)
                : m_x(0)
                , m_y(0)
                , m_owner(owner)
            {
                while (!Done() && !NeedsToSend())
                {
                    Advance();
                }
            }

            Iterator()
                : m_x(4)
                , m_y(0)
                , m_owner(nullptr)
            {
            }

            bool operator==(const Iterator& other) const
            {
                return m_x == other.m_x && m_y == other.m_y;
            }

            bool operator!=(const Iterator& other) const
            {
                return !(*this == other);
            }

            bool Done() const
            {
                return m_x >= 4;
            }

            void Advance()
            {
                ++m_y;
                if (m_y == 4)
                {
                    m_y = 0;
                    ++m_x;
                }
            }

            bool NeedsToSend() const
            {
                if (m_owner->m_cooldown[m_x][m_y] > 0)
                {
                    return false;
                }

                size_t currentTrack = m_owner->m_encoderState->GetCurrentTrack() * m_owner->m_encoderState->GetNumVoices();
                float value = m_owner->m_encoderState->GetValue(m_x, m_y, currentTrack);

                if (!m_owner->m_sent[m_x][m_y] || 
                    m_owner->m_values[m_x][m_y] != static_cast<uint8_t>(value * 127))
                {
                    return true;
                }

                return false;
            }

            Iterator& operator++()
            {
                Advance();
                while (!Done() && !NeedsToSend())
                {
                    Advance();
                }

                return *this;
            }

            BasicMidi operator*() const
            {
                size_t currentTrack = m_owner->m_encoderState->GetCurrentTrack() * m_owner->m_encoderState->GetNumVoices();
                float valueF = m_owner->m_encoderState->GetValue(m_x, m_y, currentTrack);
                uint8_t value = static_cast<uint8_t>(valueF * 127);
                m_owner->m_values[m_x][m_y] = value;
                m_owner->m_sent[m_x][m_y] = true;
                m_owner->m_cooldown[m_x][m_y] = 1;
                return BasicMidi::CC(0, -1, 0 /*channel*/, EncoderMidi::PosToNote(m_x, m_y), value);
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

    struct WrldBLDRMidiWriter
    {
        static constexpr size_t x_numColorWriters = 5;
        WrldBLDRColorMidiWriter m_colorWriters[x_numColorWriters];
        WrldBLDRIndicatorMidiWriter m_indicatorWriter;

        WrldBLDRMidiWriter(TheNonagonSquiggleBoyWrldBldr* wrldBldr)
            : m_colorWriters{
                WrldBLDRColorMidiWriter(&wrldBldr->m_uiState.m_colorBus[static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::LeftGrid) - TheNonagonSquiggleBoyWrldBldr::x_launchpadStartRouteId], 3, WrldBLDRColorMidiWriter::Mode::Grid),
                WrldBLDRColorMidiWriter(&wrldBldr->m_uiState.m_colorBus[static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::RightGrid) - TheNonagonSquiggleBoyWrldBldr::x_launchpadStartRouteId], 4, WrldBLDRColorMidiWriter::Mode::Grid),
                WrldBLDRColorMidiWriter(&wrldBldr->m_uiState.m_colorBus[static_cast<int>(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid) - TheNonagonSquiggleBoyWrldBldr::x_launchpadStartRouteId], 5, WrldBLDRColorMidiWriter::Mode::Grid),
                WrldBLDRColorMidiWriter(&wrldBldr->m_internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState, 1, WrldBLDRColorMidiWriter::Mode::EncoderButton),
                WrldBLDRColorMidiWriter(&wrldBldr->m_internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState, 0, WrldBLDRColorMidiWriter::Mode::EncoderIndicator)
            }
            , m_indicatorWriter(&wrldBldr->m_internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState)
        {
        }

        void Reset()
        {
            for (size_t i = 0; i < x_numColorWriters; ++i)
            {
                m_colorWriters[i].Reset();
            }

            m_indicatorWriter.Reset();
        }

        void ProcessCoolDown()
        {
            for (size_t i = 0; i < x_numColorWriters; ++i)
            {
                m_colorWriters[i].ProcessCoolDown();
            }

            m_indicatorWriter.ProcessCoolDown();
        }

        void Write(YaeltexColorSysexBuffer& buffer, size_t& budget, size_t index)
        {
            m_colorWriters[index].Write(buffer, budget);
        }

        void WriteClear(YaeltexColorSysexBuffer& buffer, size_t index)
        {
            m_colorWriters[index].WriteFill(buffer, Color::Off);
        }
    };
}