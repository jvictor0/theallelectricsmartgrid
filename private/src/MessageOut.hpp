#pragma once

namespace SmartGrid
{
    struct MessageOut
    {
        enum class Mode : int
        {
            NoMessage,
            Clock,
            Start,
            Stop,            
        };

        static MessageOut Clock()
        {
            return MessageOut(Mode::Clock);
        }

        static MessageOut Start()
        {
            return MessageOut(Mode::Start);
        }

        static MessageOut Stop()
        {
            return MessageOut(Mode::Stop);
        }

        MessageOut(Mode mode)
            : m_mode(mode)
        {
        }

        MessageOut()
            : m_mode(Mode::NoMessage)
        {
        }

        Mode m_mode;
    };

    struct MessageOutBuffer
    {
        static constexpr size_t x_maxMessages = 16;
        MessageOut m_messages[16];
        size_t m_numMessages;

        MessageOutBuffer()
            : m_numMessages(0)
        {
            memset(m_messages, 0, sizeof(m_messages));
        }

        MessageOut* begin()
        {
            return m_messages;
        }

        MessageOut* end()
        {
            return m_messages + m_numMessages;
        }

        bool Push(MessageOut msg)
        {
            if (m_numMessages >= x_maxMessages)
            {
                return false;
            }

            m_messages[m_numMessages] = msg;
            m_numMessages++;
            return true;
        }

        void Clear()
        {
            m_numMessages = 0;
        }
    };
}