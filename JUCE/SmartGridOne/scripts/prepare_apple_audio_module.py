"""Build the pinned JUCE overlay for Apple MIDI scheduling and iOS audio reset."""

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


def PatchIOSAudioHeader(data):
    if hashlib.sha256(data).hexdigest() != "11f7426a9d415275a2720cf4b05e899221e5e6460c81982325e1c8a6ef1412a9":
        raise ValueError("Audio reset requires the audited JUCE 8.0.15 iOS header")
    return Replace(data, b"    void close() override;",
                   b"    ~iOSAudioIODevice() override;\r\n    String ResetAudio();\r\n    void close() override;")


def PatchIOSAudio(data):
    if hashlib.sha256(data).hexdigest() != "708c0a543ddbefe7c9cb83bc1d3428e62b18dd2124133f051582bdacba112339":
        raise ValueError("Audio reset requires the audited JUCE 8.0.15 iOS backend")
    text = data.decode().replace("\r\n", "\n")
    text = Replace(text, "static constexpr int defaultBufferSize = 256;",
                   "static constexpr int defaultBufferSize = 512;")
    # Preserve JUCE's iOS 18 activation warm-up, with checked construction and
    # callback storage that survives a failed native disposal.
    begin = text.index("class SubstituteAudioUnit")
    end = text.index("struct iOSAudioIODevice::Pimpl", begin)
    warmup = text[begin:end]
    warmup = Replace(warmup, "public:\n", """public:
    static String WarmUp()
    {
        auto unit = std::make_unique<SubstituteAudioUnit>();
        unit->waitForAudioCallback();
        auto error = unit->m_error;
        if (unit->audioUnit != nullptr)
        {
            AudioOutputUnitStop(unit->audioUnit);
            AudioUnitUninitialize(unit->audioUnit);
            const auto status = AudioComponentInstanceDispose(unit->audioUnit);
            if (status != noErr)
            {
                error += " Warm-up AudioComponentInstanceDispose failed (OSStatus " + String(status) + ")";
                // A native callback may still signal this unit's event.
                //
                unit.release();
            }
        }

        return error;
    }

    bool CheckStatus(OSStatus status, const char* operation)
    {
        if (status == noErr)
        {
            return true;
        }

        m_error = String("Audio warm-up ") + operation + " failed (OSStatus " + String(status) + ")";
        return false;
    }

    String m_error;

""")
    warmup = Replace(warmup, """        if (audioUnit != nullptr)
        {
            AudioComponentInstanceDispose (audioUnit);
            audioUnit = nullptr;
        }

""", "")
    warmup = Replace(warmup, """        AudioComponentInstanceNew (comp, &audioUnit);

        if (audioUnit == nullptr)
            return false;""", """        if (comp == nullptr)
        {
            m_error = "Audio warm-up RemoteIO component unavailable";
            return false;
        }

        if (!CheckStatus(AudioComponentInstanceNew(comp, &audioUnit), "AudioComponentInstanceNew"))
        {
            return false;
        }

        if (audioUnit == nullptr)
        {
            m_error = "Audio warm-up returned no unit";
            return false;
        }
""")
    warmup = re.sub(r"^( +)(Audio(?:Unit(?:SetProperty|Initialize)|OutputUnitStart) \([^;]+\));$",
                   lambda m: m[1] + 'if (!CheckStatus(' + m[2] + ', "' + m[2].split(' ')[0] + '"))\n' + m[1] + '{\n' + m[1] + '    return false;\n' + m[1] + '}\n',
                   warmup, flags=re.MULTILINE)
    warmup = Replace(warmup, """        AudioComponentInstanceDispose (audioUnit);
        audioUnit = nullptr;

        return result;""", """        if (!result)
        {
            m_error = "Audio warm-up timed out waiting for the native callback";
        }

        return result;""")
    text = text[:begin] + warmup + text[end:]
    text = Replace(text, """                SubstituteAudioUnit au;
                [[maybe_unused]] const auto success = au.waitForAudioCallback();
                jassert (success);""", """                const auto error = SubstituteAudioUnit::WarmUp();
                if (error.isNotEmpty())
                {
                    Logger::writeToLog(error);
                }
""")
    reset = Path(__file__).with_name("SmartGridIOSAudioReset.inc").read_text()
    text = Replace(text, "    String open (const BigInteger& inputChannelsWanted,", reset + "\n    String open (const BigInteger& inputChannelsWanted,")
    text = Replace(text, """        close();

        firstHostTime = true;""", """        close();
        if (audioUnit != nullptr)
        {
            return lastError;
        }

        m_resetPending = false;

        firstHostTime = true;""")
    begin = text.index("    void close()\n", text.index("struct iOSAudioIODevice::Pimpl"))
    end = text.index("    void start (", begin)
    text = text[:begin] + """    void close()
    {
        stop();
        m_resetPending = true;
        cancelPendingUpdate();
        isRunning = false;
        if (!DisposeAudioUnit())
        {
            Logger::writeToLog(lastError);
            return;
        }

        setAudioSessionActive(false);
    }

""" + text[end:]
    begin = text.index("    void stop()\n", begin)
    end = text.index("    bool setAudioPreprocessingEnabled", begin)
    text = text[:begin] + """    void stop()
    {
        AudioIODeviceCallback* lastCallback;
        {
            const ScopedLock lock(callbackLock);
            lastCallback = std::exchange(callback, nullptr);
            m_resumeCallback = nullptr;
        }

        if (lastCallback != nullptr)
        {
            lastCallback->audioDeviceStopped();
        }
    }

""" + text[end:]
    # Session notifications are delivered on arbitrary native threads. Queue
    # lifecycle handling using the holder, never a raw device pointer.
    for signature, args, capture in [
        ("void AudioSessionHolder::handleStatusChange (bool enabled, const char* reason) const", "enabled, reason", "enabled, reason"),
        ("void AudioSessionHolder::handleRouteChange (AVAudioSessionRouteChangeReason reason)", "reason", "reason"),
    ]:
        name = signature.split("::")[1].split(" (")[0]
        text = Replace(text, signature + "\n{", signature + "\n{" + f"""
    if (!MessageManager::getInstance()->isThisTheMessageThread())
    {{
        MessageManager::callAsync([holder = SharedResourcePointer<AudioSessionHolder>(), {capture}]()
        {{
            holder->{name}({args});
        }});
        return;
    }}
""")
    for signature in ["    void handleStatusChange (bool enabled, const char* reason)",
                      "    void handleRouteChange (AVAudioSessionRouteChangeReason reason)",
                      "    void handleAsyncUpdate() override"]:
        text = Replace(text, signature + "\n    {", signature + "\n    {\n        if (m_resetPending)\n        {\n            return;\n        }\n")
    text = Replace(text,
        "        static_cast<Pimpl*> (data)->handleAudioUnitPropertyChange (unit, propertyID, scope, element);",
        """        // Native property callbacks must not inspect or reconstruct the unit.
        // The async update runs on the same message thread as ResetAudio.
        //
        static_cast<Pimpl*>(data)->triggerAsyncUpdate();""")
    # Take the render lock before touching the unit or its channel buffers.
    text = Replace(text, """        OSStatus err = noErr;

        recordXruns (time, numFrames);""", """        const ScopedTryLock stl(callbackLock);
        if (!stl.isLocked())
        {
            for (UInt32 c = 0; c < data->mNumberBuffers; ++c)
            {
                zeromem(data->mBuffers[c].mData, data->mBuffers[c].mDataByteSize);
            }

            return noErr;
        }

        OSStatus err = noErr;

        recordXruns (time, numFrames);""")
    text = Replace(text, "        const ScopedTryLock stl (callbackLock);\n", "")
    # Check the operations used to construct the replacement RemoteIO unit.
    begin = text.index("    bool createAudioUnit()")
    end = text.index("    void fillHostCallbackInfo", begin)
    create = text[begin:end]
    create = Replace(create, """        if (audioUnit != nullptr)
        {
            AudioComponentInstanceDispose (audioUnit);
            audioUnit = nullptr;
        }""", """        if (!DisposeAudioUnit())
        {
            return false;
        }
""")
    create = Replace(create, "        AudioComponentInstanceNew (comp, &audioUnit);", """        if (comp == nullptr)
        {
            lastError = "RemoteIO audio component unavailable";
            return false;
        }

        if (!CheckAudioStatus(AudioComponentInstanceNew(comp, &audioUnit), "AudioComponentInstanceNew"))
        {
            return false;
        }
""")
    create = Replace(create, """        if (audioUnit == nullptr)
            return false;""", """        if (audioUnit == nullptr)
        {
            lastError = "AudioComponentInstanceNew returned no audio unit";
            return false;
        }
""")
    create = re.sub(r"^( +)(AudioUnit(?:SetProperty|Initialize|AddPropertyListener) \([^;]+\));$",
                    lambda m: m[1] + 'if (!CheckAudioStatus(' + m[2] + ', "' + m[2].split(' ')[0] + '"))\n' + m[1] + '{\n' + m[1] + '    return false;\n' + m[1] + '}\n',
                    create, flags=re.MULTILINE)
    text = text[:begin] + create + text[end:]
    text = Replace(text, "void iOSAudioIODevice::close()", """String iOSAudioIODevice::ResetAudio()
{
    return pimpl->ResetAudio();
}

iOSAudioIODevice::~iOSAudioIODevice()
{
    pimpl->close();
    if (pimpl->audioUnit != nullptr)
    {
        // Failed native disposal may still reference this callback context.
        // Retain it until process exit; there is no safe owner to return it to.
        //
        pimpl->sessionHolder->activeDevices.removeFirstMatchingValue(pimpl.get());
        pimpl.release();
    }
}

void iOSAudioIODevice::close()""")
    return text.replace("\n", "\r\n").encode()


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
        patches[Path("native/juce_Audio_ios.cpp")] = PatchIOSAudio
        patches[Path("native/juce_Audio_ios.h")] = PatchIOSAudioHeader

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
