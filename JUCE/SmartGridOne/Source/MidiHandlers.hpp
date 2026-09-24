#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "MidiUtils.hpp"
#include "SmartGridInclude.hpp"
#include "ThreadId.hpp"
#include "MidiOutputSchedule.hpp"

struct MidiInputHandler : public juce::MidiInputCallback
{
    int m_routeId;
    std::unique_ptr<juce::MidiInput> m_midiInput;
    juce::String m_name;

    MidiInputHandler()
        : m_routeId(-1)
    {
    }

    MidiInputHandler(int routeId)
        : m_routeId(routeId)
    {
    }

    virtual ~MidiInputHandler() = default;

    void Open(const juce::String &deviceIdentifier)
    {
        INFO("Opening MIDI input: %s", deviceIdentifier.toRawUTF8());
        m_midiInput = juce::MidiInput::openDevice(deviceIdentifier, this);
        if (m_midiInput.get())
        {
            m_midiInput->start();
            m_name = m_midiInput->getName();
            INFO("MIDI input opened: %s (name %s)", deviceIdentifier.toRawUTF8(), m_name.toRawUTF8());
        }
        else
        {
            INFO("MIDI input failed to open: %s", deviceIdentifier.toRawUTF8());
        }
    }

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        ScopedThreadId scopedThreadId(ThreadId::MidiInput);

        if (message.getRawDataSize() == 3)
        {
            double timestampSeconds = message.getTimeStamp();
            size_t timestampUs = timestampSeconds * 1000 * 1000;
            const uint8_t* rawData = message.getRawData();
            SendMessage(SmartGrid::BasicMidi(timestampUs, m_routeId, rawData[0], rawData[1], rawData[2]));
        }
        else if (message.getRawDataSize() == 1)
        {
            const uint8_t* rawData = message.getRawData();
            if (SmartGrid::BasicMidi::IsSupportedRealtimeStatus(rawData[0]))
            {
                double timestampSeconds = message.getTimeStamp();
                size_t timestampUs = timestampSeconds * 1000 * 1000;
                SendMessage(SmartGrid::BasicMidi::Realtime(timestampUs, SmartGrid::MidiToMessageIn::x_realtimeRouteId, rawData[0]));
            }
        }
    }

    virtual void SendMessage(SmartGrid::BasicMidi msg) = 0;

    bool IsOpen() const
    {
        return m_midiInput != nullptr && m_midiInput->isAlive();
    }

    bool AttemptConnect()
    {
        if (m_name.isEmpty() || IsOpen())
        {
            return false;
        }

        if (m_midiInput != nullptr)
        {
            INFO("MIDI input disconnected: %s", m_name.toRawUTF8());
            m_midiInput.reset();
        }

        const auto identifier = MidiInputDeviceIdentifierFromName(m_name);
        if (identifier.isEmpty())
        {
            return false;
        }

        Open(identifier);
        return IsOpen();
    }

    JSON ToJSON(JsonArena& a)
    {
        JSON rootJ = a.Object();
        rootJ.SetNew("route_id", a.Integer(m_routeId));
        rootJ.SetNew("midi_input", a.String(m_name.toUTF8()));
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON nameJ = rootJ.Get("midi_input");
        if (!nameJ.IsNull())
        {
            m_name = juce::String(nameJ.StringValue());
        }

        AttemptConnect();
    }
};

struct MidiOutputHandler
{
    std::unique_ptr<juce::MidiOutput> m_midiOutput;
    juce::String m_name;
    std::atomic<SmartGrid::ControllerShape> m_shape;
    int m_routeId;
    SpinLock m_mutex;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_refreshRequested{false};

    MidiOutputHandler()
        : m_shape(SmartGrid::ControllerShape::LaunchPadX)
    {
    }

    virtual ~MidiOutputHandler() = default;

    bool IsOpen() const
    {
        return m_midiOutput != nullptr && m_midiOutput->isAlive();
    }

    bool AttemptConnect()
    {
        if (m_name.isEmpty() || IsOpen())
        {
            return false;
        }

        m_connected.store(false, std::memory_order_release);
        if (m_midiOutput != nullptr)
        {
            INFO("MIDI output disconnected: %s", m_name.toRawUTF8());
            AutoLockSpin lock(m_mutex);
            m_midiOutput.reset();
        }

        const auto identifier = MidiOutputDeviceIdentifierFromName(m_name);
        if (identifier.isEmpty())
        {
            return false;
        }

        Open(identifier);
        return IsOpen();
    }

    void RequestRefresh()
    {
        m_refreshRequested.store(true, std::memory_order_release);
    }

    // Called by the audio producer; it owns the writer caches and their reset.
    //
    bool PrepareProcess()
    {
        if (!m_connected.load(std::memory_order_acquire))
        {
            return false;
        }

        if (m_refreshRequested.exchange(false, std::memory_order_acq_rel))
        {
            Reset();
        }

        return true;
    }

    void Open(const juce::String &deviceIdentifier)
    {
        m_connected.store(false, std::memory_order_release);
        AutoLockSpin lock(m_mutex);
        INFO("Opening MIDI output (shape %s): %s", SmartGrid::ControllerShapeToString(m_shape), deviceIdentifier.toRawUTF8());
        m_midiOutput = juce::MidiOutput::openDevice(deviceIdentifier);
        if (!m_midiOutput.get())
        {
            INFO("MIDI output failed to open: %s", deviceIdentifier.toRawUTF8());
        }
        else
        {
            m_name = m_midiOutput->getName();
            INFO("MIDI output opened: %s (name %s)", deviceIdentifier.toRawUTF8(), m_name.toRawUTF8());
        }

        if (IsOpen())
        {
            RequestRefresh();
            m_connected.store(true, std::memory_order_release);
        }
    }

    virtual void Reset() = 0;
    virtual void Process() = 0;

    JSON ToJSON(JsonArena& a)
    {
        JSON rootJ = a.Object();
        rootJ.SetNew("midi_output", a.String(m_name.toUTF8()));
        rootJ.SetNew("shape", a.Integer(static_cast<int>(m_shape.load())));
        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        JSON nameJ = rootJ.Get("midi_output");
        if (!nameJ.IsNull())
        {
            m_name = juce::String(nameJ.StringValue());
        }

        JSON shapeJ = rootJ.Get("shape");
        if (!shapeJ.IsNull())
        {
            m_shape = static_cast<SmartGrid::ControllerShape>(shapeJ.IntegerValue());
        }

        AttemptConnect();
    }

    void SendBuffer(juce::MidiBuffer& buffer, double blockTimestampMs)
    {
        AutoLockSpin lock(m_mutex);
        if (IsOpen())
        {
            m_midiOutput->sendBlockOfMessages(buffer, blockTimestampMs, SampleTimer::x_sampleRate);
        }
    }

    void SendImmediateMessage(juce::MidiMessage& message)
    {
        AutoLockSpin lock(m_mutex);
        if (IsOpen())
        {
            m_midiOutput->sendMessageNow(message);
        }
        else
        {
            m_connected.store(false, std::memory_order_release);
            SmartGrid::MidiOutputSchedule::s_missingOutput.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void SendScheduledMessage(juce::MidiMessage& message, std::uint64_t hostTicks)
    {
        AutoLockSpin lock(m_mutex);
        if (IsOpen())
        {
            m_midiOutput->sendMessageAtHostTime(message, hostTicks);
        }
        else
        {
            m_connected.store(false, std::memory_order_release);
            SmartGrid::MidiOutputSchedule::s_missingOutput.fetch_add(1, std::memory_order_relaxed);
        }
    }
};
