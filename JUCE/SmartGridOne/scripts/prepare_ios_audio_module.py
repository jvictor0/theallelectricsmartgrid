"""Build the pinned JUCE iOS audio-device overlay used for MIDI scheduling."""
import argparse
import hashlib
from pathlib import Path
import re


def ReadVersion(source):
    metadata = (source / "juce_audio_devices.h").read_bytes()
    match = re.search(rb"^\s*version:\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$", metadata, re.MULTILINE)
    if match is None:
        raise ValueError("JUCE audio module version is missing")
    return match.group(1).decode()


def ScheduleCoreMidi(data):
    expected = "aba1e1e58e2a14afb3731bfe92da486ab2af6054f9b432dad569b009605270d7"
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("Scheduled MIDI requires the audited JUCE 8.0.15 CoreMIDI source")
    newline = "\r\n" if b"\r\n" in data else "\n"
    text = data.decode().replace("\r\n", "\n")
    def replace(old, new, count=1):
        nonlocal text
        if text.count(old) != count:
            raise ValueError("Scheduled MIDI hook context changed: " + old[:100])
        text = text.replace(old, new)
    replace("namespace juce\n{", '#include "../SmartGridMidiOutputSchedule.hpp"\n\nnamespace juce\n{')
    replace("const MIDITimeStamp timeStamp = mach_absolute_time();",
            "const MIDITimeStamp timeStamp = SmartGrid::MidiOutputSchedule::Timestamp (mach_absolute_time());", 2)
    replace("""                JUCE_CHECK_ERROR (portAndEndpoint.getPort() != 0 ? MIDISendEventList (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), &stackList)
                                                                 : MIDIReceivedEventList (portAndEndpoint.getEndpoint(), &stackList));""", """                const auto submittedAt = mach_absolute_time();
                if (SmartGrid::MidiOutputSchedule::DropLate (submittedAt))
                    return;

                const auto status = portAndEndpoint.getPort() != 0
                    ? MIDISendEventList (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), &stackList)
                    : MIDIReceivedEventList (portAndEndpoint.getEndpoint(), &stackList);
                SmartGrid::MidiOutputSchedule::RecordSubmit (status, submittedAt);
                JUCE_CHECK_ERROR (status);""")
    replace("""            if (portAndEndpoint.getPort() != 0)
                MIDISend (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), packetToSend);
            else
                MIDIReceived (portAndEndpoint.getEndpoint(), packetToSend);""", """            const auto submittedAt = mach_absolute_time();
            if (SmartGrid::MidiOutputSchedule::DropLate (submittedAt))
                return;

            const auto status = portAndEndpoint.getPort() != 0
                ? MIDISend (portAndEndpoint.getPort(), portAndEndpoint.getEndpoint(), packetToSend)
                : MIDIReceived (portAndEndpoint.getEndpoint(), packetToSend);
            SmartGrid::MidiOutputSchedule::RecordSubmit (status, submittedAt);""")
    replace("""            if (x == portAndEndpoint.getEndpoint())
                disconnectListeners.call ([] (auto& c) { c.disconnected(); });""", """            if (x == portAndEndpoint.getEndpoint())
            {
                SmartGrid::MidiOutputSchedule::s_disconnected.fetch_add (1, std::memory_order_relaxed);
                disconnectListeners.call ([] (auto& c) { c.disconnected(); });
            }
""")
    return text.replace("\n", newline).encode()



def Prepare(source, destination):
    source = source.resolve()
    destination = destination.resolve()
    if source == destination or source in destination.parents or destination in source.parents:
        raise ValueError("The build overlay must be separate from the pinned JUCE module")
    if ReadVersion(source) != "8.0.15":
        raise ValueError("Expected pinned JUCE 8.0.15")

    midi_native = Path("native/juce_CoreMidi_mac.mm")
    midi_data = ScheduleCoreMidi((source / midi_native).read_bytes())
    audio_native = Path("native/juce_Audio_ios.cpp")
    audio_data = (source / audio_native).read_bytes()
    old_default = b"static constexpr int defaultBufferSize = 256;"
    if audio_data.count(old_default) != 1:
        raise ValueError("JUCE iOS default buffer context changed")
    audio_data = audio_data.replace(old_default, b"static constexpr int defaultBufferSize = 512;")

    midi_header = Path("midi_io/juce_MidiDevices.h")
    header = (source / midi_header).read_bytes()
    if hashlib.sha256(header).hexdigest() != "30ebc5b9b9f88224b4900ab48f8860ce607818ffde12d8116fa2cb24863cc68a":
        raise ValueError("JUCE MIDI device header changed")
    anchor = b"    /** Sends out a MIDI message immediately. */"
    if header.count(anchor) != 1:
        raise ValueError("JUCE MIDI output connection context changed")
    header = header.replace(anchor, b"    bool isConnected() const { return connection.isAlive(); }\n\n    void replaceConnectionFrom (MidiOutput& other)\n    {\n        std::swap (connection, other.connection);\n        std::swap (session, other.session);\n        std::swap (storedInfo, other.storedInfo);\n        std::swap (group, other.group);\n    }\n\n" + anchor)

    for path in source.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        target = destination / relative
        content = (midi_data if relative == midi_native else
                   audio_data if relative == audio_native else
                   header if relative == midi_header else path.read_bytes())
        if not target.exists() or target.read_bytes() != content:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)

    schedule_header = Path(__file__).resolve().parents[3] / "private/src/MidiOutputSchedule.hpp"
    (destination / "SmartGridMidiOutputSchedule.hpp").write_bytes(schedule_header.read_bytes())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    Prepare(args.source, args.destination)
