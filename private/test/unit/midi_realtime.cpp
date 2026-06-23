#include "doctest.h"

#include "BasicMidi.hpp"
#include "BitSet.hpp"
#include "InteriorGrid.hpp"
#include "MessageInLatency.hpp"
#include "MessageInBus.hpp"
#include "MidiToMessageIn.hpp"
#include "WrldBLDRMidi.hpp"

using namespace SmartGrid;

namespace
{
    MessageIn PopOrNoMessage(MessageInBus& bus, size_t timestamp)
    {
        MessageIn msg;
        if (bus.Pop(msg, timestamp))
        {
            return msg;
        }

        return MessageIn();
    }
}

DOCTEST_TEST_CASE("BasicMidi preserves supported one byte realtime statuses")
{
    BasicMidi clock = BasicMidi::Realtime(2000000, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusClock);
    BasicMidi start = BasicMidi::Realtime(2000001, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStart);
    BasicMidi cont = BasicMidi::Realtime(2000002, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportContinue);
    BasicMidi stop = BasicMidi::Realtime(2000003, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStop);
    BasicMidi transportContinue = BasicMidi::TransportContinue(2000004);

    DOCTEST_CHECK(clock.Size() == 1);
    DOCTEST_CHECK(start.Size() == 1);
    DOCTEST_CHECK(cont.Size() == 1);
    DOCTEST_CHECK(stop.Size() == 1);
    DOCTEST_CHECK(transportContinue.Size() == 1);
    DOCTEST_CHECK(clock.m_timestamp == 2000000);
    DOCTEST_CHECK(clock.m_routeId == MidiToMessageIn::x_realtimeRouteId);
    DOCTEST_CHECK(clock.m_msg[0] == BasicMidi::x_statusClock);
    DOCTEST_CHECK(cont.m_msg[0] == BasicMidi::x_statusTransportContinue);
    DOCTEST_CHECK(transportContinue.m_msg[0] == BasicMidi::x_statusTransportContinue);
    DOCTEST_CHECK(BasicMidi::IsSupportedRealtimeStatus(BasicMidi::x_statusClock));
    DOCTEST_CHECK(BasicMidi::IsSupportedRealtimeStatus(BasicMidi::x_statusTransportStart));
    DOCTEST_CHECK(BasicMidi::IsSupportedRealtimeStatus(BasicMidi::x_statusTransportContinue));
    DOCTEST_CHECK(BasicMidi::IsSupportedRealtimeStatus(BasicMidi::x_statusTransportStop));
    DOCTEST_CHECK_FALSE(BasicMidi::IsSupportedRealtimeStatus(0xFE));
}

DOCTEST_TEST_CASE("Realtime route converts clock start continue stop into timestamp gated MessageIn")
{
    MessageInBus bus;
    bus.m_midiToMessageIn.SetRouteType(MidiToMessageIn::x_realtimeRouteId, MidiToMessageIn::RouteType::Realtime);

    bus.Push(BasicMidi::Realtime(2000, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusClock));

    MessageIn early = PopOrNoMessage(bus, 21999);
    DOCTEST_CHECK(early.NoMessage());

    MessageIn clock = PopOrNoMessage(bus, 22000);
    DOCTEST_CHECK(clock.m_mode == MessageIn::Mode::MidiClock);
    DOCTEST_CHECK(clock.IsRealtime());
    DOCTEST_CHECK(clock.IsMidiClock());
    DOCTEST_CHECK(clock.m_timestamp == 22000);
    DOCTEST_CHECK(clock.m_routeId == MidiToMessageIn::x_realtimeRouteId);

    bus.Push(BasicMidi::Realtime(2001, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStart));
    bus.Push(BasicMidi::Realtime(2002, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportContinue));
    bus.Push(BasicMidi::Realtime(2003, MidiToMessageIn::x_realtimeRouteId, BasicMidi::x_statusTransportStop));

    MessageIn start = PopOrNoMessage(bus, 22001);
    MessageIn cont = PopOrNoMessage(bus, 22002);
    MessageIn stop = PopOrNoMessage(bus, 22003);

    DOCTEST_CHECK(start.m_mode == MessageIn::Mode::MidiStart);
    DOCTEST_CHECK(start.IsRealtime());
    DOCTEST_CHECK(cont.m_mode == MessageIn::Mode::MidiContinue);
    DOCTEST_CHECK(cont.IsRealtime());
    DOCTEST_CHECK(stop.m_mode == MessageIn::Mode::MidiStop);
    DOCTEST_CHECK(stop.IsRealtime());
}

DOCTEST_TEST_CASE("Unsupported realtime statuses become no message and are not queued")
{
    MessageInBus bus;
    bus.m_midiToMessageIn.SetRouteType(MidiToMessageIn::x_realtimeRouteId, MidiToMessageIn::RouteType::Realtime);

    DOCTEST_CHECK(bus.Push(BasicMidi::Realtime(3000, MidiToMessageIn::x_realtimeRouteId, 0xFE)));

    MessageIn msg = PopOrNoMessage(bus, 23000);
    DOCTEST_CHECK(msg.NoMessage());
    DOCTEST_CHECK(bus.m_queue.IsEmpty());
}

DOCTEST_TEST_CASE("Realtime helper messages use fixed route without manual route setup")
{
    MessageInBus bus;

    DOCTEST_CHECK(bus.Push(BasicMidi::Clock(4000)));
    MessageIn early = PopOrNoMessage(bus, 23999);
    DOCTEST_CHECK(early.NoMessage());

    MessageIn clock = PopOrNoMessage(bus, 24000);

    DOCTEST_CHECK(clock.m_mode == MessageIn::Mode::MidiClock);
    DOCTEST_CHECK(clock.m_routeId == MidiToMessageIn::x_realtimeRouteId);
    DOCTEST_CHECK(clock.m_timestamp == 24000);

    DOCTEST_CHECK(bus.Push(BasicMidi::TransportStart(4001)));
    DOCTEST_CHECK(bus.Push(BasicMidi::TransportContinue(4002)));
    DOCTEST_CHECK(bus.Push(BasicMidi::TransportStop(4003)));

    DOCTEST_CHECK(PopOrNoMessage(bus, 24001).m_mode == MessageIn::Mode::MidiStart);
    DOCTEST_CHECK(PopOrNoMessage(bus, 24002).m_mode == MessageIn::Mode::MidiContinue);
    DOCTEST_CHECK(PopOrNoMessage(bus, 24003).m_mode == MessageIn::Mode::MidiStop);
}

DOCTEST_TEST_CASE("Route type setup ignores invalid route ids")
{
    MidiToMessageIn converter;

    converter.SetRouteType(-1, MidiToMessageIn::RouteType::Realtime);
    converter.SetRouteType(16, MidiToMessageIn::RouteType::Realtime);

    MessageIn msg = converter.FromMidi(BasicMidi::CC(5000, 0, 0, 1, 2));

    DOCTEST_CHECK(msg.NoMessage());
}

DOCTEST_TEST_CASE("Realtime input latency schedules future timestamps")
{
    DOCTEST_CHECK(MessageInLatency::WithLatency(0) == 20000);
    DOCTEST_CHECK(MessageInLatency::WithLatency(1000000) == 1020000);
}

DOCTEST_TEST_CASE("MessageInBus applies input latency to direct MessageIn pushes")
{
    MessageInBus bus;

    DOCTEST_CHECK(bus.Push(MessageIn(6000, 1, MessageIn::Mode::PadPress, 0, 0)));

    MessageIn early = PopOrNoMessage(bus, 25999);
    MessageIn msg = PopOrNoMessage(bus, 26000);

    DOCTEST_CHECK(early.NoMessage());
    DOCTEST_CHECK(msg.m_mode == MessageIn::Mode::PadPress);
    DOCTEST_CHECK(msg.m_timestamp == 26000);
}

DOCTEST_TEST_CASE("WrldBLDRMidi maps realtime clock transport messages")
{
    MessageIn clock = WrldBLDRMidi::FromMidi(BasicMidi::Clock(5000));
    MessageIn start = WrldBLDRMidi::FromMidi(BasicMidi::TransportStart(5001));
    MessageIn cont = WrldBLDRMidi::FromMidi(BasicMidi::TransportContinue(5002));
    MessageIn stop = WrldBLDRMidi::FromMidi(BasicMidi::TransportStop(5003));

    DOCTEST_CHECK(clock.m_mode == MessageIn::Mode::MidiClock);
    DOCTEST_CHECK(clock.m_timestamp == 5000);
    DOCTEST_CHECK(clock.m_routeId == MidiToMessageIn::x_realtimeRouteId);
    DOCTEST_CHECK(start.m_mode == MessageIn::Mode::MidiStart);
    DOCTEST_CHECK(cont.m_mode == MessageIn::Mode::MidiContinue);
    DOCTEST_CHECK(stop.m_mode == MessageIn::Mode::MidiStop);
}
