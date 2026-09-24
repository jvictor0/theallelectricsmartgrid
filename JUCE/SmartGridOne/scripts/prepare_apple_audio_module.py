"""Build the pinned JUCE audio-device overlay for Apple MIDI scheduling."""

import argparse
import hashlib
from pathlib import Path
import re


def Replace(text, old, new, count=1):
    if text.count(old) != count:
        raise ValueError("Pinned JUCE context changed: " + old[:100])
    return text.replace(old, new)


def PatchCoreMidi(data):
    expected = "aba1e1e58e2a14afb3731bfe92da486ab2af6054f9b432dad569b009605270d7"
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("Scheduled MIDI requires the audited JUCE 8.0.15 CoreMIDI source")

    newline = "\r\n" if b"\r\n" in data else "\n"
    text = data.decode().replace("\r\n", "\n")
    text = Replace(text, "namespace juce\n{",
                   '#include "../SmartGridMidiOutputSchedule.hpp"\n\nnamespace juce\n{')
    text = Replace(text,
                   "virtual bool send (const MidiPortAndEndpoint& portAndEndpoint, ump::Iterator b, ump::Iterator e) = 0;",
                   "virtual bool send (const MidiPortAndEndpoint& portAndEndpoint, ump::Iterator b, ump::Iterator e, uint64_t hostTicks) = 0;")
    text = Replace(text,
                   "bool send (const MidiPortAndEndpoint& portAndEndpoint, ump::Iterator b, ump::Iterator e) override",
                   "bool send (const MidiPortAndEndpoint& portAndEndpoint, ump::Iterator b, ump::Iterator e, uint64_t hostTicks) override", 2)
    text = Replace(text,
                   "bool send (const MidiPortAndEndpoint& portAndEndpoint, ump::Iterator b, ump::Iterator e)\n",
                   "bool send (const MidiPortAndEndpoint& portAndEndpoint, ump::Iterator b, ump::Iterator e, uint64_t hostTicks)\n")
    text = Replace(text,
                   "const MIDITimeStamp timeStamp = mach_absolute_time();",
                   "const MIDITimeStamp timeStamp = SmartGrid::MidiOutputSchedule::Timestamp (hostTicks, mach_absolute_time());", 2)
    text = Replace(text,
                   "const MIDITimeStamp timeStamp = AudioGetCurrentHostTime();",
                   "const MIDITimeStamp timeStamp = SmartGrid::MidiOutputSchedule::Timestamp (hostTicks, AudioGetCurrentHostTime());", 2)
    text = Replace(text, "sendBytes (portAndEndpoint, msg.bytes);",
                   "sendBytes (portAndEndpoint, msg.bytes, hostTicks);")
    text = Replace(text,
                   "void sendBytes (const MidiPortAndEndpoint& portAndEndpoint, Span<const std::byte> message)",
                   "void sendBytes (const MidiPortAndEndpoint& portAndEndpoint, Span<const std::byte> message, uint64_t hostTicks)")
    text = Replace(text, "return outputInterface->send (portAndEndpoint, b, e);",
                   "return outputInterface->send (portAndEndpoint, b, e, hostTicks);")
    text = Replace(text,
                   "bool send (ump::Iterator b, ump::Iterator e)\n",
                   "bool send (ump::Iterator b, ump::Iterator e, uint64_t hostTicks)\n")
    text = Replace(text, "return output.send (connection.portAndEndpoint, b, e);",
                   "return output.send (connection.portAndEndpoint, b, e, hostTicks);")
    text = Replace(text,
                   "bool send (ump::Iterator b, ump::Iterator e) override",
                   "bool send (ump::Iterator b, ump::Iterator e, uint64_t hostTicks) override")
    text = Replace(text, "return connection->send (b, e);",
                   "return connection->send (b, e, hostTicks);")
    text = Replace(text, """                JUCE_CHECK_ERROR (portAndEndpoint.getPort() != 0 ? MIDISendEventList (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), &stackList)
                                                                 : MIDIReceivedEventList (portAndEndpoint.getEndpoint(), &stackList));""", """                const auto submittedAt = mach_absolute_time();
                if (SmartGrid::MidiOutputSchedule::DropLate (hostTicks, submittedAt))
                    return;

                const auto status = portAndEndpoint.getPort() != 0
                    ? MIDISendEventList (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), &stackList)
                    : MIDIReceivedEventList (portAndEndpoint.getEndpoint(), &stackList);
                SmartGrid::MidiOutputSchedule::RecordSubmit (status, hostTicks, submittedAt);
                JUCE_CHECK_ERROR (status);""")
    text = Replace(text, """            if (portAndEndpoint.getPort() != 0)
                MIDISend (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), packetToSend);
            else
                MIDIReceived (portAndEndpoint.getEndpoint(), packetToSend);""", """            const auto submittedAt = mach_absolute_time();
            if (SmartGrid::MidiOutputSchedule::DropLate (hostTicks, submittedAt))
                return;

            const auto status = portAndEndpoint.getPort() != 0
                ? MIDISend (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), packetToSend)
                : MIDIReceived (portAndEndpoint.getEndpoint(), packetToSend);
            SmartGrid::MidiOutputSchedule::RecordSubmit (status, hostTicks, submittedAt);""")
    text = Replace(text, """            if (x == portAndEndpoint.getEndpoint())
                disconnectListeners.call ([] (auto& c) { c.disconnected(); });""", """            if (x == portAndEndpoint.getEndpoint())
            {
                SmartGrid::MidiOutputSchedule::s_disconnected.fetch_add (1, std::memory_order_relaxed);
                disconnectListeners.call ([] (auto& c) { c.disconnected(); });
            }
""")
    return text.replace("\n", newline).encode()


def PatchMidiHeader(data):
    if hashlib.sha256(data).hexdigest() != "30ebc5b9b9f88224b4900ab48f8860ce607818ffde12d8116fa2cb24863cc68a":
        raise ValueError("Scheduled MIDI requires the audited JUCE 8.0.15 MIDI output header")

    newline = "\r\n" if b"\r\n" in data else "\n"
    text = data.decode().replace("\r\n", "\n")
    text = Replace(text, "    void stop();", "    void stop();\n    bool isAlive() const;")
    text = Replace(text, "    /** Sends out a MIDI message immediately. */", """    bool isAlive() const
    {
        return connection.isAlive();
    }

    void sendMessageAtHostTime (const MidiMessage& message, uint64_t hostTicks)
    {
        convertAndSend (mainPackets, Span { &message, 1 }, hostTicks);
    }

    /** Sends out a MIDI message immediately. */""")
    text = Replace(text, "convertAndSend (mainPackets, Span { &message, 1 });",
                   "convertAndSend (mainPackets, Span { &message, 1 }, 0);")
    text = Replace(text, "convertAndSend (mainPackets, buffer);",
                   "convertAndSend (mainPackets, buffer, 0);")
    text = Replace(text, "void convertAndSend (ump::Packets& packets, Range&& range)",
                   "void convertAndSend (ump::Packets& packets, Range&& range, uint64_t hostTicks)")
    text = Replace(text, "connection.send (packets.begin(), packets.end());", """if (hostTicks == 0)
            connection.send (packets.begin(), packets.end());
        else
            connection.sendAtHostTime (packets.begin(), packets.end(), hostTicks);""")
    text = Replace(text, "convertAndSend (backgroundPackets, Span { &message, 1 });",
                   "convertAndSend (backgroundPackets, Span { &message, 1 }, 0);")
    return text.replace("\n", newline).encode()


def PatchMidiCpp(data):
    newline = "\r\n" if b"\r\n" in data else "\n"
    text = data.decode().replace("\r\n", "\n")
    text = Replace(text, "    void start()\n", """    bool isAlive() const
    {
        return connection.isAlive();
    }

    void start()
""")
    text = Replace(text, "void MidiInput::start()", """bool MidiInput::isAlive() const
{
    return pimpl->isAlive();
}

void MidiInput::start()""")
    return text.replace("\n", newline).encode()


def PatchUMPHeader(data):
    newline = "\r\n" if b"\r\n" in data else "\n"
    text = data.decode().replace("\r\n", "\n")
    text = Replace(text, "    bool send (Iterator beginIterator, Iterator endIterator);",
                   "    bool send (Iterator beginIterator, Iterator endIterator);\n    bool sendAtHostTime (Iterator beginIterator, Iterator endIterator, uint64_t hostTicks);")
    return text.replace("\n", newline).encode()


def PatchUMPCpp(data):
    newline = "\r\n" if b"\r\n" in data else "\n"
    text = data.decode().replace("\r\n", "\n")
    text = Replace(text, "virtual bool send (Iterator b, Iterator e) = 0;",
                   "virtual bool send (Iterator b, Iterator e, uint64_t hostTicks) = 0;")
    text = Replace(text, "bool send (Iterator beginIterator, Iterator endIterator)\n",
                   "bool send (Iterator beginIterator, Iterator endIterator, uint64_t hostTicks)\n")
    text = Replace(text, "return isAlive() && native->send (beginIterator, endIterator);",
                   "return isAlive() && native->send (beginIterator, endIterator, hostTicks);")
    text = Replace(text, "return impl->send (beginIterator, endIterator);",
                   "return impl->send (beginIterator, endIterator, 0);")
    text = Replace(text, "void Output::addDisconnectionListener (DisconnectionListener& x)", """bool Output::sendAtHostTime (Iterator beginIterator, Iterator endIterator, uint64_t hostTicks)
{
    jassert (isAlive());

    if (impl != nullptr)
        return impl->send (beginIterator, endIterator, hostTicks);

    return false;
}

void Output::addDisconnectionListener (DisconnectionListener& x)""")
    return text.replace("\n", newline).encode()


def Prepare(source, destination, ios_buffer):
    source = source.resolve()
    destination = destination.resolve()
    if source == destination or source in destination.parents or destination in source.parents:
        raise ValueError("The build overlay must be separate from the pinned JUCE module")

    metadata = (source / "juce_audio_devices.h").read_bytes()
    match = re.search(rb"^\s*version:\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$", metadata, re.MULTILINE)
    if match is None or match.group(1) != b"8.0.15":
        raise ValueError("Expected pinned JUCE 8.0.15")

    patches = {
        Path("native/juce_CoreMidi_mac.mm"): PatchCoreMidi,
        Path("midi_io/juce_MidiDevices.h"): PatchMidiHeader,
        Path("midi_io/juce_MidiDevices.cpp"): PatchMidiCpp,
        Path("midi_io/ump/juce_UMPOutput.h"): PatchUMPHeader,
        Path("midi_io/ump/juce_UMPOutput.cpp"): PatchUMPCpp,
    }
    if ios_buffer:
        def PatchAudio(data):
            if data.count(b"static constexpr int defaultBufferSize = 256;") != 1:
                raise ValueError("JUCE iOS default buffer context changed")
            return data.replace(b"static constexpr int defaultBufferSize = 256;",
                                b"static constexpr int defaultBufferSize = 512;")
        patches[Path("native/juce_Audio_ios.cpp")] = PatchAudio

    for path in source.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        target = destination / relative
        content = patches[relative](path.read_bytes()) if relative in patches else path.read_bytes()
        if not target.exists() or target.read_bytes() != content:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)

    schedule_header = Path(__file__).resolve().parents[3] / "private/src/MidiOutputSchedule.hpp"
    target = destination / "SmartGridMidiOutputSchedule.hpp"
    content = schedule_header.read_bytes()
    if not target.exists() or target.read_bytes() != content:
        target.write_bytes(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--ios-buffer", action="store_true")
    args = parser.parse_args()
    Prepare(args.source, args.destination, args.ios_buffer)
