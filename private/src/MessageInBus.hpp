#pragma once

#include "CircularQueue.hpp"
#include "MessageIn.hpp"
#include "BasicMidi.hpp"
#include "MidiToMessageIn.hpp"

namespace SmartGrid
{
    struct MessageInBus
    {
        CircularQueue<MessageIn, 16384> m_queue;
        MidiToMessageIn m_midiToMessageIn;

        bool Push(MessageIn msg)
        {
            if (!m_queue.Push(msg))
            {
                INFO("MessageInBus push failed");
                return false;
            }

            return true;
        }

        bool Push(BasicMidi msg)
        {
            MessageIn msgIn = m_midiToMessageIn.FromMidi(msg);
            if (!m_queue.Push(msgIn))
            {
                INFO("MessageInBus push failed");
                return false;
            }

            return true;
        }

        void SetRouteType(int route, MidiToMessageIn::RouteType routeType)
        {
            m_midiToMessageIn.SetRouteType(route, routeType);
        }

        bool Pop(MessageIn& msg, size_t timestamp)
        {
            if (!m_queue.Peek(msg))
            {
                return false;
            }
            
            if (msg.Visible(timestamp))
            {
                m_queue.Pop();
                return true;
            }
            else
            {
                return false;
            }
        }

        template<class T>
        void ProcessMessages(T* processor, size_t timestamp)
        {
            MessageIn msg;
            while (Pop(msg, timestamp))
            {
                processor->Apply(msg);
            }
        }
    };
}