#pragma once

#include <JuceHeader.h>
#include "MidiUtils.hpp"
#include "SmartGridInclude.hpp"

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
        juce::Logger::writeToLog("Opening MIDI input: " + deviceIdentifier);
        m_midiInput = juce::MidiInput::openDevice(deviceIdentifier, this);
        if (m_midiInput.get())
        {
            m_midiInput->start();
            m_name = m_midiInput->getName();
            juce::Logger::writeToLog("MIDI input opened: " + deviceIdentifier + "(name " + m_midiInput->getName() + ")");
        }
        else
        {
            juce::Logger::writeToLog("MIDI input failed to open: " + deviceIdentifier);
        }
    }

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override
    {
        if (message.getRawDataSize() == 3)
        {
            double timestampSeconds = message.getTimeStamp();
            size_t timestampUs = timestampSeconds * 1000 * 1000;
            const uint8_t* rawData = message.getRawData();
            SendMessage(SmartGrid::BasicMidi(timestampUs, m_routeId, rawData[0], rawData[1], rawData[2]));
        }
    }

    virtual void SendMessage(SmartGrid::BasicMidi msg) = 0;

    void AttemptConnect()
    {
        if (!m_name.isEmpty() && !m_midiInput.get())
        {
            juce::String deviceIdentifier = MidiInputDeviceIdentifierFromName(m_name);
            if (!deviceIdentifier.isEmpty())
            {
                Open(deviceIdentifier);
            }
        }
    }

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("route_id", JSON::Integer(m_routeId));
        rootJ.SetNew("midi_input", JSON::String(m_name.toUTF8()));
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
    SmartGrid::ControllerShape m_shape;
    int m_routeId;
    SpinLock m_mutex;

    MidiOutputHandler()
        : m_shape(SmartGrid::ControllerShape::LaunchPadX)
    {
    }

    virtual ~MidiOutputHandler() = default;

    void AttemptConnect()
    {
        if (!m_name.isEmpty() && !m_midiOutput.get())
        {
            juce::String deviceIdentifier = MidiOutputDeviceIdentifierFromName(m_name);
            if (!deviceIdentifier.isEmpty())
            {
                Open(deviceIdentifier);
            }
        }
    }

    void Open(const juce::String &deviceIdentifier)
    {
        AutoLockSpin lock(m_mutex);
        juce::Logger::writeToLog(juce::String("Opening MIDI output (shape ") + juce::String(SmartGrid::ControllerShapeToString(m_shape)) + "): " + deviceIdentifier);
        m_midiOutput = juce::MidiOutput::openDevice(deviceIdentifier);
        if (!m_midiOutput.get())
        {
            juce::Logger::writeToLog("MIDI output failed to open: " + deviceIdentifier);
        }
        else
        {
            m_name = m_midiOutput->getName();
            juce::Logger::writeToLog("MIDI output opened: " + deviceIdentifier + " (name " + m_midiOutput->getName() + ")");
        }

        Reset();
    }

    virtual void Reset() = 0;
    virtual void Process() = 0;

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("midi_output", JSON::String(m_name.toUTF8()));
        rootJ.SetNew("shape", JSON::Integer(static_cast<int>(m_shape)));
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
        if (m_midiOutput.get())
        {
            m_midiOutput->sendBlockOfMessages(buffer, blockTimestampMs, SampleTimer::x_sampleRate);
        }
    }

    void SendMessage(juce::MidiMessage& message)
    {
        AutoLockSpin lock(m_mutex);
        if (m_midiOutput.get())
        {
            m_midiOutput->sendMessageNow(message);
        }
    }
};

