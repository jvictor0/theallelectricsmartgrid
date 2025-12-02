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
            LaunchPad,
            Encoder,
            Param14,
            Param7,
        };

        RouteType m_routeTypes[16];

        void SetRouteType(int route, RouteType routeType)
        {
            m_routeTypes[route] = routeType;
        }

        MessageIn FromMidi(BasicMidi msg)
        {
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
            else
            {
                return MessageIn();
            }
        }
    };
}