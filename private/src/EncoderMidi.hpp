#pragma once

#include "Color.hpp"
#include "MessageIn.hpp"
#include "BasicMidi.hpp"
#include "EncoderUIState.hpp"

namespace SmartGrid
{
    struct EncoderMidi
    {
        static uint8_t PosToNote(int x, int y)
        {
            return y * 4 + x;
        }

        static std::pair<int, int> NoteToPos(uint8_t note)
        {
            return std::make_pair(note % 4, note / 4);
        }

        static bool ShapeSupports(int x, int y)
        {
            return x >= 0 && x < 4 && y >= 0 && y < 4;
        }

        static MessageIn FromMidi(BasicMidi msg)
        {
            if (msg.Status() != BasicMidi::x_statusCC)
            {
                return MessageIn();
            }

            std::pair<int, int> pos = NoteToPos(msg.GetCC());
            MessageIn::Mode mode;
            int64_t amount;
            if (msg.Channel() == 0)
            {
                mode = MessageIn::Mode::EncoderIncDec;
                amount = static_cast<int64_t>(msg.GetValue()) - 64;             
            }
            else if (msg.Channel() == 1)
            {
                amount = msg.GetValue() > 0 ? 1 : 0;
                mode = msg.GetValue() > 0 ? MessageIn::Mode::EncoderPush : MessageIn::Mode::EncoderRelease;
            }
            else
            {
                return MessageIn();
            }

            return MessageIn(msg.m_timestamp, msg.m_routeId, mode, pos.first, pos.second, amount);
        }
    };

    struct EncoderMidiWriter
    {
        static constexpr int x_numPhases = 3;

        bool m_sent[4][4];
        Color m_color[4][4];
        float m_brightness[4][4];
        float m_values[4][4];

        EncoderBankUIState* m_encoderBankState;

        EncoderMidiWriter(EncoderBankUIState* encoderBankState)
            : m_sent{}
            , m_color{}
            , m_brightness{}
            , m_values{}
            , m_encoderBankState(encoderBankState)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 4; ++j)
                {
                    m_sent[i][j] = false;
                    m_color[i][j] = Color(0, 0, 0);
                    m_brightness[i][j] = 0;
                    m_values[i][j] = 0;
                }
            }
        }

        void Reset()
        {
            for (size_t i = 0; i < 4; ++i)
            {
                for (size_t j = 0; j < 4; ++j)
                {
                    m_sent[i][j] = false;
                }
            }
        }

        struct Iterator
        {
            size_t m_x;
            size_t m_y;
            int m_phase;
            EncoderMidiWriter* m_owner;
            EncoderBankUIState* m_encoderBankState;

            Iterator(EncoderMidiWriter* owner)
                : m_x(0)
                , m_y(0)
                , m_phase(0)
                , m_owner(owner)
                , m_encoderBankState(owner->m_encoderBankState)
            {
                while (!Done() && !NeedsToSend())
                {
                    Advance();
                }
            }

            Iterator()
                : m_x(4)
                , m_y(0)
                , m_phase(0)
                , m_owner(nullptr)
                , m_encoderBankState(nullptr)
            {
            }

            bool operator==(const Iterator& other) const
            {
                return std::tie(m_x, m_y, m_phase) == std::tie(other.m_x, other.m_y, other.m_phase);
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
                ++m_phase;
                if (m_phase == x_numPhases)
                {
                    m_owner->m_sent[m_x][m_y] = true;
                    m_phase = 0;
                    ++m_y;
                    if (m_y == 4)
                    {
                        m_y = 0;
                        ++m_x;
                        if (m_x == 4)
                        {
                            return;
                        }
                    }
                }
            }
                
            bool NeedsToSend() const
            {
                if (!m_owner->m_sent[m_x][m_y])
                {
                    return true;
                }

                if (m_phase == 0)
                {
                    return m_owner->m_color[m_x][m_y] != m_encoderBankState->GetColor(m_x, m_y) &&
                           m_encoderBankState->GetBrightness(m_x, m_y) > 0;
                }
                else if (m_phase == 1)
                {
                    return m_owner->m_brightness[m_x][m_y] != m_encoderBankState->GetBrightness(m_x, m_y);
                }
                else if (m_phase == 2)
                {
                    return m_owner->m_values[m_x][m_y] != m_encoderBankState->GetValue(m_x, m_y, m_phase);
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
                if (m_phase == 0)
                {
                    Color color = m_encoderBankState->GetColor(m_x, m_y);
                    m_owner->m_color[m_x][m_y] = color;
                    uint8_t mfTwisterCode = color.ToTwister();
                    return BasicMidi::CC(0, -1, 1 /*channel*/, EncoderMidi::PosToNote(m_x, m_y), mfTwisterCode);
                }
                else if (m_phase == 1)
                {
                    float brightnessF = m_encoderBankState->GetBrightness(m_x, m_y);
                    uint8_t brightness = 17 + brightnessF * 30;
                    m_owner->m_brightness[m_x][m_y] = brightnessF;
                    return BasicMidi::CC(0, -1, 2 /*channel*/, EncoderMidi::PosToNote(m_x, m_y), brightness);
                }
                else if (m_phase == 2)
                {
                    size_t currentTrack = m_encoderBankState->GetCurrentTrack() * m_encoderBankState->GetNumVoices();
                    float valueF = m_encoderBankState->GetValue(m_x, m_y, currentTrack);
                    uint8_t value = valueF * 127;
                    m_owner->m_values[m_x][m_y] = valueF;
                    return BasicMidi::CC(0, -1, 0 /*channel*/, EncoderMidi::PosToNote(m_x, m_y), value);
                }
                else
                {
                    return BasicMidi();
                }
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
}

