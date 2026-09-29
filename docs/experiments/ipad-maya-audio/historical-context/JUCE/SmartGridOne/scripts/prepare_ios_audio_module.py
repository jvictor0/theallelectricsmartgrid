"""Build an app-local JUCE audio module with exact frame-duration requests."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def ReadVersion(source):
    metadata = (source / "juce_audio_devices.h").read_bytes()
    match = re.search(rb"^\s*version:\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$", metadata, re.MULTILINE)
    if match is None:
        raise ValueError("JUCE audio module version is missing from module metadata")
    return match.group(1).decode()


def InstrumentNative(data):
    # Pin the audited source before adding diagnostic hooks. Never guess at a new backend.
    #
    expected = "708c0a543ddbefe7c9cb83bc1d3428e62b18dd2124133f051582bdacba112339"
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("Native probe requires the audited JUCE 8.0.15 iOS source")
    newline = b"\r\n" if b"\r\n" in data else b"\n"
    text = data.decode().replace("\r\n", "\n")
    def replace(old, new):
        nonlocal text
        if text.count(old) != 1:
            raise ValueError("Native probe hook context changed: " + old[:100])
        text = text.replace(old, new)
    replace("namespace juce\n{", '#include "../SmartGridRemoteIOProbe.hpp"\n\nnamespace juce\n{')
    replace("        const ScopedTryLock stl (callbackLock);\n\n        if (stl.isLocked() && callback != nullptr)",
            "        const ScopedTryLock stl (callbackLock);\n        bool smartGridCallbackInvoked = false;\n\n        if (stl.isLocked() && callback != nullptr)")
    replace("            callback->audioDeviceIOCallbackWithContext ((const float**) inputData,",
            "            smartGridCallbackInvoked = true;\n            callback->audioDeviceIOCallbackWithContext ((const float**) inputData,")
    replace("        return err;\n    }\n\n    void recordXruns", "        SmartGridRemoteIOOutcome (stl.isLocked(), smartGridCallbackInvoked, useInput, err);\n        return err;\n    }\n\n    void recordXruns")
    replace("        return static_cast<Pimpl*> (client)->process (flags, time, numFrames, data);", """        auto* self = static_cast<Pimpl*> (client);
        const auto entry = SmartGridRemoteIOBegin();
        const auto status = self->process (flags, time, numFrames, data);
        const auto exit = mach_absolute_time();
        SmartGridRemoteIOEnd (self->audioUnit, entry, exit, time, flags != nullptr ? *flags : 0,
                             numFrames, data, self->sampleRate, status);
        return status;""")
    replace("        AudioUnitInitialize (audioUnit);\n\n        {", "        AudioUnitInitialize (audioUnit);\n        SmartGridRemoteIOSetup (audioUnit);\n\n        {")
    replace('    NSUInteger value;\n\n    if (juce::getNotificationValueForKey (notification, AVAudioSessionInterruptionTypeKey, value))\n    {',
            '    NSUInteger value;\n\n    if (juce::getNotificationValueForKey (notification, AVAudioSessionInterruptionTypeKey, value))\n    {\n        SmartGridRemoteIOLifecycle ("notification.interruption", nullptr, static_cast<std::int64_t> (value), 0, 0);')
    replace('    if (juce::getNotificationValueForKey (notification, AVAudioSessionRouteChangeReasonKey, value))\n        audioSessionHolder->handleRouteChange ((AVAudioSessionRouteChangeReason) value);',
            '    if (juce::getNotificationValueForKey (notification, AVAudioSessionRouteChangeReasonKey, value))\n    {\n        SmartGridRemoteIOLifecycle ("notification.route", nullptr, static_cast<std::int64_t> (value), 0, 0);\n        audioSessionHolder->handleRouteChange ((AVAudioSessionRouteChangeReason) value);\n    }')
    for suffix, event in [("Reset", "mediaReset"), ("Lost", "mediaLost")]:
        old='- (void) handleMediaServices'+suffix+'\n{'
        replace(old, old+'\n    SmartGridRemoteIOLifecycle ("notification.'+event+'", nullptr, 0, 0, 0);')
    for operation in ["AudioOutputUnitStart", "AudioOutputUnitStop", "AudioUnitInitialize", "AudioComponentInstanceDispose"]:
        original=operation+" (audioUnit)"
        text=text.replace(original, 'SmartGridRemoteIOCall ("'+operation+'", audioUnit, [&] { return '+original+'; })')
    macro='#define JUCE_NSERROR_CHECK(X)     { NSError* error = nil; X; logNSError (error); }'
    replacement=macro+"\n"+r'''#define SMARTGRID_SESSION_CHECK(name, value, X) { const auto requested = static_cast<std::int64_t> (value); SmartGridRemoteIOLifecycle (name, nullptr, requested, 0, 1); NSError* error = nil; const auto success = (X); logNSError (error); SmartGridRemoteIOLifecycle (name, nullptr, requested, error != nil ? static_cast<std::int32_t> (error.code) : (success ? 0 : -1), 2); }'''
    replace(macro,replacement)
    replacements=[
        ('JUCE_NSERROR_CHECK ([[AVAudioSession sharedInstance] setCategory: category',
         'SMARTGRID_SESSION_CHECK (category == AVAudioSessionCategoryPlayAndRecord ? "session.category.PlayAndRecord" : "session.category.Playback", options, [[AVAudioSession sharedInstance] setCategory: category'),
        ('JUCE_NSERROR_CHECK ([[AVAudioSession sharedInstance] setActive: enabled',
         'SMARTGRID_SESSION_CHECK ("session.active", enabled, [[AVAudioSession sharedInstance] setActive: enabled'),
        ('JUCE_NSERROR_CHECK ([session setPreferredIOBufferDuration: bufferDuration error: &error]);',
         'SMARTGRID_SESSION_CHECK ("session.preferredBufferUs", bufferDuration * 1000000.0, [session setPreferredIOBufferDuration: bufferDuration error: &error]);'),
        ('JUCE_NSERROR_CHECK ([session setPreferredSampleRate: rate error: &error]);',
         'SMARTGRID_SESSION_CHECK ("session.preferredRate", rate, [session setPreferredSampleRate: rate error: &error]);')]
    for old,new in replacements:
        replace(old,new)
    return text.replace("\n", newline.decode()).encode()


def SingleStartNative(data):
    newline = b"\r\n" if b"\r\n" in data else b"\n"
    text = data.decode().replace("\r\n", "\n")
    def replace(old, new):
        nonlocal text
        if text.count(old) != 1:
            raise ValueError("Single-start context changed: " + old[:100])
        text = text.replace(old, new)

    begin = text.index("        // We need to activate the audio session here")
    end = text.index("        sessionHolder->activeDevices.add (this);", begin)
    text = text[:begin] + """        // Output-only startup experiment: configure once before activation.
        //
        setAudioSessionCategory (AVAudioSessionCategoryPlayback);
        auto session = [AVAudioSession sharedInstance];
        SMARTGRID_SESSION_CHECK ("session.preferredRate", 48000,
                                 [session setPreferredSampleRate: 48000 error: &error]);
        SMARTGRID_SESSION_CHECK ("session.preferredBufferUs", 512000000.0 / 48000.0,
                                 [session setPreferredIOBufferDuration: 512.0 / 48000.0 error: &error]);
        setAudioSessionActive (true);
        updateHardwareInfo();
        channelData.reconfigure ({}, {});

""" + text[end:]
    begin = text.index("        if (@available (iOS 18, *))", text.index("    static void setAudioSessionActive"))
    end = text.index("\n    int getBufferSize", begin)
    text = text[:begin] + "    }\n" + text[end:]
    replace("""        setAudioSessionActive (true);
        setAudioSessionCategory (requestedInputChannels > 0 ? AVAudioSessionCategoryPlayAndRecord
                                                            : AVAudioSessionCategoryPlayback);
        channelData.reconfigure (requestedInputChannels, requestedOutputChannels);""", """        if (requestedInputChannels > 0)
        {
            lastError = "Single-start experiment requires zero inputs";
            return lastError;
        }

        channelData.reconfigure (requestedInputChannels, requestedOutputChannels);""")
    replace("    static void fixAudioRouteIfSetToReceiver()", "    void fixAudioRouteIfSetToReceiver()")
    replace("    static void setAudioSessionActive (bool enabled)",
            "    bool m_experimentSessionActive = false;\n\n    void setAudioSessionActive (bool enabled)")
    replace("                                                                 error: &error]);\n\n    }\n\n    int getBufferSize",
            "                                                                 error: &error]);\n        m_experimentSessionActive = enabled;\n    }\n\n    int getBufferSize")
    replace("        channelData.reconfigure (requestedInputChannels, requestedOutputChannels);\n        setTargetSampleRateAndBufferSize();",
            "        if (! m_experimentSessionActive)\n        {\n            setAudioSessionActive (true);\n        }\n\n        channelData.reconfigure (requestedInputChannels, requestedOutputChannels);\n        setTargetSampleRateAndBufferSize();")
    return text.replace("\n", newline.decode()).encode()


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


def Prepare(source, destination, native_probe=False, single_start=False, scheduled_midi=False):
    source = source.resolve()
    destination = destination.resolve()
    if source == destination or source in destination.parents or destination in source.parents:
        raise ValueError("The build copy must be separate from the shared JUCE module")

    native = Path("native/juce_Audio_ios.cpp")
    data = (source / native).read_bytes()
    midi_native = Path("native/juce_CoreMidi_mac.mm")
    midi_data = (source / midi_native).read_bytes()
    midi_original_hash = hashlib.sha256(midi_data).hexdigest()
    if scheduled_midi:
        midi_data = ScheduleCoreMidi(midi_data)
    version = ReadVersion(source)
    original_hash = hashlib.sha256(data).hexdigest()
    if native_probe:
        data = InstrumentNative(data)
    if single_start:
        if not native_probe:
            raise ValueError("Single-start experiment requires native tracing")
        data = SingleStartNative(data)
    replacements_by_version = {
        "8.0.2": (
            (b"((newBufferSize + 1) / currentSampleRate)", b"(newBufferSize / currentSampleRate)"),
            (b"static constexpr int defaultBufferSize = 256;", b"static constexpr int defaultBufferSize = 512;"),
        ),
        "8.0.15": (
            (b"static constexpr int defaultBufferSize = 256;", b"static constexpr int defaultBufferSize = 512;"),
        ),
    }
    if version not in replacements_by_version:
        raise ValueError("Unsupported JUCE audio module version: " + version)

    replacements = replacements_by_version[version]
    for original, replacement in replacements:
        if data.count(original) != 1:
            raise ValueError("JUCE iOS audio source changed; review the exact-buffer adjustment: " + original.decode())
        data = data.replace(original, replacement)

    for path in source.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        target = destination / relative
        content = data if relative == native else midi_data if relative == midi_native else path.read_bytes()
        if scheduled_midi and relative == Path("midi_io/juce_MidiDevices.h"):
            if hashlib.sha256(content).hexdigest() != "30ebc5b9b9f88224b4900ab48f8860ce607818ffde12d8116fa2cb24863cc68a":
                raise ValueError("Scheduled MIDI requires the audited JUCE MIDI device header")
            anchor = b"    /** Sends out a MIDI message immediately. */"
            if content.count(anchor) != 1:
                raise ValueError("MIDI output connection diagnostic context changed")
            content = content.replace(anchor, b"    bool isConnected() const { return connection.isAlive(); }\n\n    void replaceConnectionFrom (MidiOutput& other)\n    {\n        std::swap (connection, other.connection);\n        std::swap (session, other.session);\n        std::swap (storedInfo, other.storedInfo);\n        std::swap (group, other.group);\n    }\n\n" + anchor)
        if not target.exists() or target.read_bytes() != content:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)

    if scheduled_midi:
        schedule = Path(__file__).resolve().parents[3] / "private/src/MidiOutputSchedule.hpp"
        schedule_data = schedule.read_bytes()
        schedule_target = destination / "SmartGridMidiOutputSchedule.hpp"
        if not schedule_target.exists() or schedule_target.read_bytes() != schedule_data:
            schedule_target.write_bytes(schedule_data)

    if native_probe:
        bridge = Path(__file__).resolve().parents[1] / "Source/RemoteIOProbeBridge.hpp"
        bridge_data = bridge.read_bytes()
        bridge_target = destination / "SmartGridRemoteIOProbe.hpp"
        if not bridge_target.exists() or bridge_target.read_bytes() != bridge_data:
            bridge_target.write_bytes(bridge_data)
        manifest = {"juce_version": version, "source_sha256": original_hash,
                    "instrumented_sha256": hashlib.sha256(data).hexdigest(),
                    "bridge_sha256": hashlib.sha256(bridge_data).hexdigest(),
                    "single_start": single_start, "scheduled_midi": scheduled_midi,
                    "coremidi_source_sha256": midi_original_hash,
                    "coremidi_instrumented_sha256": hashlib.sha256(midi_data).hexdigest(),
                    "schedule_header_sha256": hashlib.sha256(schedule_data).hexdigest() if scheduled_midi else None,
                    "native_probe": True, "sample_rate": 48000, "default_frames": 512}
        (destination / "smartgrid-native-probe-manifest.json").write_text(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--native-probe", action="store_true")
    parser.add_argument("--single-start", action="store_true")
    parser.add_argument("--scheduled-midi", action="store_true")
    args = parser.parse_args()
    Prepare(args.source, args.destination, args.native_probe, args.single_start, args.scheduled_midi)
