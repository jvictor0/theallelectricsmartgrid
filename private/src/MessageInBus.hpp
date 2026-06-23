#pragma once

#include "CircularQueue.hpp"
#include "MessageIn.hpp"
#include "BasicMidi.hpp"
#include "MessageInLatency.hpp"
#include "MidiToMessageIn.hpp"

namespace SmartGrid
{
    struct MessageInBus
    {
        CircularQueue<MessageIn, 16384> m_queue;
        MidiToMessageIn m_midiToMessageIn;

        MessageInBus()
            : m_queue{}
            , m_midiToMessageIn{}
        {
        }

        bool Push(MessageIn msg)
        {
            msg.m_timestamp = MessageInLatency::WithLatency(msg.m_timestamp);

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
            if (msgIn.NoMessage())
            {
                return true;
            }

            return Push(msgIn);
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
