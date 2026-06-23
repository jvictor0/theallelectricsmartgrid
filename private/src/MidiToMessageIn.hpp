#pragma once

#include "MessageIn.hpp"
#include "BasicMidi.hpp"
#include "LaunchPadMidi.hpp"
#include "EncoderMidi.hpp"

namespace SmartGrid
{
    struct MidiToMessageIn
    {
        enum class RouteType
        {
            None,
            LaunchPad,
            Encoder,
            Param14,
            Param7,
            Realtime
        };

        static constexpr int x_realtimeRouteId = 15;

        RouteType m_routeTypes[16];

        MidiToMessageIn()
            : m_routeTypes{}
        {
            for (size_t i = 0; i < 16; ++i)
            {
                m_routeTypes[i] = RouteType::None;
            }
        }

        void SetRouteType(int route, RouteType routeType)
        {
            if (route < 0 || route >= 16)
            {
                return;
            }

            m_routeTypes[route] = routeType;
        }

        MessageIn FromRealtimeMidi(BasicMidi msg)
        {
            if (msg.m_msg[0] == BasicMidi::x_statusClock)
            {
                return MessageIn(msg.m_timestamp, msg.m_routeId, MessageIn::Mode::MidiClock, 0, 0);
            }
            else if (msg.m_msg[0] == BasicMidi::x_statusTransportStart)
            {
                return MessageIn(msg.m_timestamp, msg.m_routeId, MessageIn::Mode::MidiStart, 0, 0);
            }
            else if (msg.m_msg[0] == BasicMidi::x_statusTransportContinue)
            {
                return MessageIn(msg.m_timestamp, msg.m_routeId, MessageIn::Mode::MidiContinue, 0, 0);
            }
            else if (msg.m_msg[0] == BasicMidi::x_statusTransportStop)
            {
                return MessageIn(msg.m_timestamp, msg.m_routeId, MessageIn::Mode::MidiStop, 0, 0);
            }
            else
            {
                return MessageIn();
            }
        }

        MessageIn FromMidi(BasicMidi msg)
        {
            if (BasicMidi::IsSupportedRealtimeStatus(msg.m_msg[0]))
            {
                msg.m_routeId = x_realtimeRouteId;
                return FromRealtimeMidi(msg);
            }

            if (msg.m_routeId < 0 || msg.m_routeId >= 16)
            {
                return MessageIn();
            }

            if (m_routeTypes[msg.m_routeId] == RouteType::LaunchPad)
            {
                return LPMidi::FromMidi(msg);
            }
            else if (m_routeTypes[msg.m_routeId] == RouteType::Encoder)
            {
                return EncoderMidi::FromMidi(msg);
            }
            else if (m_routeTypes[msg.m_routeId] == RouteType::Param14)
            {
                return MessageIn();
            }
            else if (m_routeTypes[msg.m_routeId] == RouteType::Param7)
            {
                return MessageIn(msg.m_timestamp, msg.m_routeId, MessageIn::Mode::ParamSet7, msg.GetNote(), 0, msg.GetValue());
            }
            else if (m_routeTypes[msg.m_routeId] == RouteType::Realtime)
            {
                return FromRealtimeMidi(msg);
            }
            else
            {
                return MessageIn();
            }
        }
    };
}
