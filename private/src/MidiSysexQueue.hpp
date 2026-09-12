#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "CircularQueue.hpp"

namespace SmartGrid
{
    // One audio producer copies complete packets; one MIDI worker consumes them.
    // Peek retains the consumer slot until Pop, including during output submission.
    // Pop requires a non-null Peek and must be called only by the consumer.
    //
    template <size_t MaxMessageBytes, size_t QueueSize>
    struct MidiSysexQueue
    {
        static_assert(MaxMessageBytes > 0);
        static_assert(QueueSize > 0);

        struct Packet
        {
            int m_routeId;
            size_t m_size;
            uint8_t m_data[MaxMessageBytes];
        };

        CircularQueue<Packet, QueueSize> m_queue;

        bool TryPush(const uint8_t* data, size_t size, int routeId)
        {
            if (data == nullptr || size == 0 || size > MaxMessageBytes)
            {
                return false;
            }

            Packet* packet = m_queue.NextToPush();
            if (packet == nullptr)
            {
                return false;
            }

            packet->m_routeId = routeId;
            packet->m_size = size;
            std::memcpy(packet->m_data, data, size);
            m_queue.CompletePush();
            return true;
        }

        Packet* Peek()
        {
            return m_queue.PeekPtr();
        }

        void Pop()
        {
            m_queue.Pop();
        }

        // Approximate diagnostic snapshot: concurrent head/tail reads may span
        // consumer progress. Clamp the observation; capacity uses NextToPush.
        //
        size_t Size() const
        {
            return std::min(m_queue.Size(), QueueSize);
        }
    };
}
