#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "NonagonWrapper.hpp"
#include "support/GlobalEnv.hpp"
#include <CoreMIDI/CoreMIDI.h>
#include <atomic>
#include <functional>
#include <unistd.h>

bool WaitFor(const std::function<bool()>& predicate)
{
    for (int i = 0; i < 200; ++i)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        if (predicate())
        {
            return true;
        }
    }

    return false;
}

struct VirtualController
{
    MIDIClientRef m_client = 0;
    MIDIEndpointRef m_destination = 0;
    MIDIEndpointRef m_source = 0;
    MIDIUniqueID m_uniqueId;
    juce::String m_name;
    std::atomic<int> m_received{0};
    std::atomic<int> m_handshakes{0};
    std::atomic<unsigned> m_colorChannels{0};

    VirtualController()
    {
        static int nextId = 0;
        m_uniqueId = static_cast<MIDIUniqueID>(getpid() * 100 + ++nextId);
        m_name = "SmartGrid reconnect test " + juce::String(m_uniqueId);
        auto name = m_name.toCFString();
        const auto status = MIDIClientCreate(name, nullptr, nullptr, &m_client);
        CFRelease(name);
        DOCTEST_REQUIRE(status == noErr);
    }

    ~VirtualController()
    {
        RemoveDestination();
        RemoveSource();
        MIDIClientDispose(m_client);
    }

    static void Receive(const MIDIPacketList* packets, void* context, void*)
    {
        auto& controller = *static_cast<VirtualController*>(context);
        controller.m_received.fetch_add(static_cast<int>(packets->numPackets));
        const auto* packet = &packets->packet[0];
        for (UInt32 i = 0; i < packets->numPackets; ++i)
        {
            if (packet->length >= 9 && packet->data[0] == 0xF0
                && packet->data[1] == 0x79 && packet->data[2] == 0x74 && packet->data[3] == 0x78)
            {
                if (packet->data[7] == 0x21)
                {
                    controller.m_handshakes.fetch_add(1);
                }
                else if (packet->data[7] == 0x20 && packet->data[8] < 16)
                {
                    controller.m_colorChannels.fetch_or(1u << packet->data[8]);
                }
            }

            packet = MIDIPacketNext(packet);
        }
    }

    void AddDestination()
    {
        auto name = m_name.toCFString();
        const auto status = MIDIDestinationCreate(m_client, name, Receive, this, &m_destination);
        CFRelease(name);
        DOCTEST_REQUIRE(status == noErr);
        DOCTEST_REQUIRE(MIDIObjectSetIntegerProperty(m_destination, kMIDIPropertyUniqueID, m_uniqueId) == noErr);
        DOCTEST_REQUIRE(WaitFor([this]() { return !MidiOutputDeviceIdentifierFromName(m_name).isEmpty(); }));
    }

    void AddSource()
    {
        auto name = m_name.toCFString();
        const auto status = MIDISourceCreate(m_client, name, &m_source);
        CFRelease(name);
        DOCTEST_REQUIRE(status == noErr);
        DOCTEST_REQUIRE(MIDIObjectSetIntegerProperty(m_source, kMIDIPropertyUniqueID, -m_uniqueId) == noErr);
        DOCTEST_REQUIRE(WaitFor([this]() { return !MidiInputDeviceIdentifierFromName(m_name).isEmpty(); }));
    }

    void RemoveSource()
    {
        if (m_source != 0)
        {
            MIDIEndpointDispose(m_source);
            m_source = 0;
        }
    }

    void SendNote()
    {
        MIDIPacketList packets{};
        auto* packet = MIDIPacketListInit(&packets);
        const Byte bytes[] = {0x90, 60, 100};
        DOCTEST_REQUIRE(MIDIPacketListAdd(&packets, sizeof(packets), packet, 0, sizeof(bytes), bytes) != nullptr);
        DOCTEST_REQUIRE(MIDIReceived(m_source, &packets) == noErr);
    }

    void RemoveDestination()
    {
        if (m_destination != 0)
        {
            MIDIEndpointDispose(m_destination);
            m_destination = 0;
        }
    }
};

struct TestOutput : MidiOutputHandler
{
    int m_resets = 0;

    void Reset() override
    {
        ++m_resets;
    }

    void Process() override
    {
        PrepareProcess();
    }
};

DOCTEST_TEST_CASE("MIDI reconnect restores delivery after the same device identifier returns")
{
    VirtualController controller;
    controller.AddDestination();
    TestOutput output;
    const auto identifier = MidiOutputDeviceIdentifierFromName(controller.m_name);
    output.Open(identifier);
    auto message = juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100));
    output.SendImmediateMessage(message);
    DOCTEST_REQUIRE(WaitFor([&]() { return controller.m_received.load() > 0; }));

    controller.RemoveDestination();
    DOCTEST_REQUIRE(WaitFor([&]() { return MidiOutputDeviceIdentifierFromName(controller.m_name).isEmpty(); }));
    controller.AddDestination();
    DOCTEST_REQUIRE(MidiOutputDeviceIdentifierFromName(controller.m_name) == identifier);
    output.AttemptConnect();
    const auto received = controller.m_received.load();
    output.SendImmediateMessage(message);
    DOCTEST_CHECK(WaitFor([&]() { return controller.m_received.load() > received; }));
}

DOCTEST_TEST_CASE("MIDI opening defers writer reset to its producer")
{
    VirtualController controller;
    controller.AddDestination();
    TestOutput output;
    output.Open(MidiOutputDeviceIdentifierFromName(controller.m_name));
    DOCTEST_CHECK(output.m_resets == 0);
    std::thread producer([&]() { output.Process(); });
    producer.join();
    DOCTEST_CHECK(output.m_resets == 1);
    output.Process();
    DOCTEST_CHECK(output.m_resets == 1);
    output.RequestRefresh();
    DOCTEST_CHECK(output.m_resets == 1);
    output.Process();
    DOCTEST_CHECK(output.m_resets == 2);
}

struct TestInput : MidiInputHandler
{
    std::atomic<int> m_received{0};

    void SendMessage(SmartGrid::BasicMidi) override
    {
        m_received.fetch_add(1);
    }
};

DOCTEST_TEST_CASE("MIDI reconnect restores input after the same identifier returns")
{
    VirtualController controller;
    controller.AddSource();
    TestInput input;
    const auto identifier = MidiInputDeviceIdentifierFromName(controller.m_name);
    input.Open(identifier);
    controller.SendNote();
    DOCTEST_REQUIRE(WaitFor([&]() { return input.m_received.load() == 1; }));
    controller.RemoveSource();
    DOCTEST_REQUIRE(WaitFor([&]() { return MidiInputDeviceIdentifierFromName(controller.m_name).isEmpty(); }));
    controller.AddSource();
    DOCTEST_REQUIRE(MidiInputDeviceIdentifierFromName(controller.m_name) == identifier);
    input.AttemptConnect();
    controller.SendNote();
    DOCTEST_CHECK(WaitFor([&]() { return input.m_received.load() == 2; }));
}

DOCTEST_TEST_CASE("MIDI saved routes wait for absent devices and leave healthy connections intact")
{
    VirtualController controller;
    TestInput input;
    TestOutput output;
    JsonArena arena(4096);
    auto inputConfig = arena.Object();
    inputConfig.SetNew("midi_input", arena.String(controller.m_name.toRawUTF8()));
    input.FromJSON(inputConfig);
    auto outputConfig = arena.Object();
    outputConfig.SetNew("midi_output", arena.String(controller.m_name.toRawUTF8()));
    output.FromJSON(outputConfig);
    DOCTEST_CHECK_FALSE(input.IsOpen());
    DOCTEST_CHECK_FALSE(output.IsOpen());
    DOCTEST_CHECK_FALSE(output.PrepareProcess());
    DOCTEST_CHECK(input.m_name == controller.m_name);
    DOCTEST_CHECK(output.m_name == controller.m_name);

    controller.AddSource();
    controller.AddDestination();
    DOCTEST_CHECK(input.AttemptConnect());
    DOCTEST_CHECK(output.AttemptConnect());
    DOCTEST_CHECK(input.IsOpen());
    DOCTEST_CHECK(output.IsOpen());
    DOCTEST_CHECK(output.PrepareProcess());
    auto* originalInput = input.m_midiInput.get();
    auto* originalOutput = output.m_midiOutput.get();
    DOCTEST_CHECK_FALSE(input.AttemptConnect());
    DOCTEST_CHECK_FALSE(output.AttemptConnect());
    DOCTEST_CHECK(input.m_midiInput.get() == originalInput);
    DOCTEST_CHECK(output.m_midiOutput.get() == originalOutput);
    output.Process();
    DOCTEST_CHECK(output.m_resets == 1);

    controller.RemoveSource();
    controller.RemoveDestination();
    DOCTEST_REQUIRE(WaitFor([&]() { return !input.IsOpen() && !output.IsOpen(); }));
    auto message = juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100));
    const auto submitted = SmartGrid::MidiOutputSchedule::s_immediate.load();
    output.SendImmediateMessage(message);
    DOCTEST_CHECK(SmartGrid::MidiOutputSchedule::s_immediate.load() == submitted);
    DOCTEST_CHECK_FALSE(output.PrepareProcess());
    DOCTEST_CHECK_FALSE(input.AttemptConnect());
    DOCTEST_CHECK_FALSE(output.AttemptConnect());
    DOCTEST_CHECK(input.m_name == controller.m_name);
    DOCTEST_CHECK(output.m_name == controller.m_name);
}

DOCTEST_TEST_CASE("WRLD reconnect resends handshake and all feedback without restarting sender")
{
    VirtualController controller;
    controller.AddSource();
    controller.AddDestination();
    auto app = std::make_unique<NonagonWrapper>();
    app->OpenInputWrldBldr(MidiInputDeviceIdentifierFromName(controller.m_name));
    app->OpenOutputWrldBldr(MidiOutputDeviceIdentifierFromName(controller.m_name));
    DOCTEST_REQUIRE(WaitFor([&]() { return controller.m_handshakes.load() == 1; }));
    DOCTEST_REQUIRE(app->IsWrldBldrOpen());
    DOCTEST_REQUIRE(WaitFor([&]()
    {
        app->m_wrldBldr.ProcessFrame();
        app->m_wrldBldr.SendMidiOutput();
        return controller.m_colorChannels.load() == 0x3B;
    }));
    DOCTEST_REQUIRE(WaitFor([&]() { return app->m_midiSender.m_sysexQueue.Size() == 0; }));

    controller.RemoveDestination();
    DOCTEST_REQUIRE(WaitFor([&]() { return !app->IsWrldBldrOpen(); }));
    DOCTEST_CHECK(app->GetMidiOutputWrldBldr() == nullptr);
    controller.AddDestination();
    controller.m_colorChannels.store(0);
    app->CheckMidiConnections();
    DOCTEST_REQUIRE(WaitFor([&]() { return controller.m_handshakes.load() == 2; }));
    DOCTEST_REQUIRE(app->IsWrldBldrOpen());
    DOCTEST_REQUIRE(WaitFor([&]()
    {
        app->m_wrldBldr.ProcessFrame();
        app->m_wrldBldr.SendMidiOutput();
        return controller.m_colorChannels.load() == 0x3B;
    }));
    app->CheckMidiConnections();
    DOCTEST_CHECK(controller.m_handshakes.load() == 2);
    DOCTEST_REQUIRE(WaitFor([&]() { return app->m_midiSender.m_sysexQueue.Size() == 0; }));

    auto* originalOutput = app->GetMidiOutputWrldBldr();
    controller.RemoveSource();
    DOCTEST_REQUIRE(WaitFor([&]() { return !app->IsWrldBldrOpen(); }));
    controller.AddSource();
    controller.m_colorChannels.store(0);
    app->CheckMidiConnections();
    DOCTEST_CHECK(app->GetMidiOutputWrldBldr() == originalOutput);
    DOCTEST_REQUIRE(WaitFor([&]() { return controller.m_handshakes.load() == 3; }));
    DOCTEST_REQUIRE(WaitFor([&]()
    {
        app->m_wrldBldr.ProcessFrame();
        app->m_wrldBldr.SendMidiOutput();
        return controller.m_colorChannels.load() == 0x3B;
    }));
    DOCTEST_CHECK(app->m_midiSender.m_diagnostics.m_workerRunning.load());
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    GlobalEnv::Init();
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
