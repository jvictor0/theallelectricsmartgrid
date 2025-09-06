#pragma once

#include <JuceHeader.h>
#include "MidiUtils.hpp"

#define IOS_BUILD
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#include "TheNonagonSquiggleBoy.hpp"
#include "LaunchPadMidi.hpp"
#include "TwisterMidi.hpp"
#pragma GCC diagnostic pop
#undef IOS_BUILD

struct NonagonWrapperQuadLaunchpadTwister
{
    struct MidiInputHandler : public juce::MidiInputCallback
    {
        int m_routeId;
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
        std::unique_ptr<juce::MidiInput> m_midiInput;
        juce::String m_name;
  
        MidiInputHandler()
            : m_routeId(-1)
            , m_owner(nullptr)
        {
        }

        MidiInputHandler(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner, int routeId)
            : m_routeId(routeId)
            , m_owner(owner)
        {
        }

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
                m_owner->SendMessage(SmartGrid::BasicMidi(timestampUs, m_routeId, rawData[0], rawData[1], rawData[2]));
            }
        }

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

        json_t* ToJSON()
        {
            json_t* rootJ = json_object();
            json_object_set_new(rootJ, "route_id", json_integer(m_routeId));
            json_object_set_new(rootJ, "midi_input", json_string(m_name.toUTF8()));
            return rootJ;
        }

        void FromJSON(json_t* rootJ)
        {
            json_t* nameJ = json_object_get(rootJ, "midi_input");
            if (nameJ)
            {
                m_name = json_string_value(nameJ);
            }
        
            AttemptConnect();
        }
    };

    struct MidiLaunchpadOutputHandler
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;
        std::unique_ptr<juce::MidiOutput> m_midiOutput[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads];
        juce::String m_name[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads];
        SmartGrid::LPSysexWriter m_sysexWriter[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads];

        MidiLaunchpadOutputHandler(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner)
            : m_owner(owner)
        {
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                m_sysexWriter[i] = SmartGrid::LPSysexWriter(SmartGrid::ControllerShape::LaunchPadX, &m_owner->m_uiState.m_colorBus[i]);
            }
        }

        void AttemptConnect()
        {
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                if (!m_name[i].isEmpty() && !m_midiOutput[i].get())
                {
                    juce::String deviceIdentifier = MidiOutputDeviceIdentifierFromName(m_name[i]);
                    if (!deviceIdentifier.isEmpty())
                    {
                        Open(i, m_sysexWriter[i].m_shape, deviceIdentifier);
                    }
                }
            }
        }

        void Open(int index, SmartGrid::ControllerShape shape, const juce::String &deviceIdentifier)
        {
            juce::Logger::writeToLog(juce::String("Opening MIDI output (shape ") + juce::String(SmartGrid::ControllerShapeToString(shape)) + "): " + deviceIdentifier);
            m_midiOutput[index] = juce::MidiOutput::openDevice(deviceIdentifier);
            if (!m_midiOutput[index].get())
            {
                juce::Logger::writeToLog("MIDI output failed to open: " + deviceIdentifier);
            }
            else
            {
                m_name[index] = m_midiOutput[index]->getName();
            }

            juce::Logger::writeToLog("MIDI output opened: " + deviceIdentifier + " (name " + m_midiOutput[index]->getName() + ")");

            m_sysexWriter[index].m_shape = shape;
            m_sysexWriter[index].Reset();
        }

        void Process()
        {
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                if (m_midiOutput[i].get())
                {
                    uint8_t buffer[SmartGrid::LPSysexWriter::x_maxMessageSize];
                    size_t size = m_sysexWriter[i].Write(buffer);
                    if (size > 0)
                    {
                        m_midiOutput[i]->sendMessageNow(juce::MidiMessage(buffer, static_cast<int>(size)));
                    }
                }
            }
        }

        json_t* ToJSON()
        {
            json_t* rootJ = json_object();
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                json_object_set_new(rootJ, std::string("launchpad_output_" + std::to_string(i)).c_str(), json_string(m_name[i].toUTF8()));
                json_object_set_new(rootJ, std::string("launchpad_output_shape_" + std::to_string(i)).c_str(), json_integer(static_cast<int>(m_sysexWriter[i].m_shape)));
            }

            return rootJ;
        }

        void FromJSON(json_t* rootJ)
        {
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                json_t* nameJ = json_object_get(rootJ, std::string("launchpad_output_" + std::to_string(i)).c_str());
                if (nameJ)
                {
                    m_name[i] = json_string_value(nameJ);
                }

                json_t* shapeJ = json_object_get(rootJ, std::string("launchpad_output_shape_" + std::to_string(i)).c_str());
                if (shapeJ)
                {
                    m_sysexWriter[i].m_shape = static_cast<SmartGrid::ControllerShape>(json_integer_value(shapeJ));
                }
            }

            AttemptConnect();
        }
    };

    struct MidiTwisterOutputHandler
    {
        TheNonagonSquiggleBoyInternal* m_owner;
        std::unique_ptr<juce::MidiOutput> m_midiOutput;
        juce::String m_name;
        SmartGrid::MFTMidiWriter m_midiWriter;

        MidiTwisterOutputHandler(TheNonagonSquiggleBoyInternal* owner)
            : m_owner(owner)
            , m_midiWriter(&owner->m_squiggleBoyUIState.m_encoderBankUIState)
        {
        }

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
            juce::Logger::writeToLog("Opening MIDI output (Twister): " + deviceIdentifier);
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

            m_midiWriter.Reset();
        }

        void Process()
        {
            if (m_midiOutput.get())
            {
                for (SmartGrid::BasicMidi msg : m_midiWriter)
                {
                    juce::MidiMessage message(msg.m_msg, 3);
                    m_midiOutput->sendMessageNow(message);
                }
            }
        }

        json_t* ToJSON()
        {
            json_t* rootJ = json_object();
            json_object_set_new(rootJ, "midi_output", json_string(m_name.toUTF8()));
            return rootJ;
        }

        void FromJSON(json_t* rootJ)
        {
            json_t* nameJ = json_object_get(rootJ, "midi_output");
            if (nameJ)
            {
                m_name = json_string_value(nameJ);
            }

            AttemptConnect();
        }
    };

    NonagonWrapperQuadLaunchpadTwister(TheNonagonSquiggleBoyInternal* internal)
        : m_nonagon(internal)
        , m_midiLaunchpadOutputHandler(&m_nonagon)
        , m_midiTwisterOutputHandler(internal)
        , m_internal(internal)
    {
        for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            m_midiInputHandler[i] = MidiInputHandler(&m_nonagon, i);
        }
    }

    void OpenInput(int index, const juce::String &deviceIdentifier)
    {
        m_midiInputHandler[index].Open(deviceIdentifier);
    }

    void OpenLaunchpadOutput(int index, SmartGrid::ControllerShape shape, const juce::String &deviceIdentifier)
    {
        m_midiLaunchpadOutputHandler.Open(index, shape, deviceIdentifier);
    }

    void OpenTwisterOutput(const juce::String &deviceIdentifier)
    {
        m_midiTwisterOutputHandler.Open(deviceIdentifier);
    }

    juce::MidiInput* GetMidiInput(int index)
    {
        return m_midiInputHandler[index].m_midiInput.get();
    }

    juce::MidiOutput* GetMidiOutput(int index)
    {
        if (index < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads)
        {
            return m_midiLaunchpadOutputHandler.m_midiOutput[index].get();
        }
        else
        {
            return m_midiTwisterOutputHandler.m_midiOutput.get();
        }
    }

    SmartGrid::ControllerShape GetControllerShape(int index)
    {
        return m_midiLaunchpadOutputHandler.m_sysexWriter[index].m_shape;
    }

    void ProcessSample(size_t timestamp)
    {
        m_nonagon.ProcessSample(timestamp);
    }

    void ProcessFrame()
    {
        m_nonagon.ProcessFrame();
    }

    void SendMidiOutput()
    {
        m_midiLaunchpadOutputHandler.Process();
        m_midiTwisterOutputHandler.Process();
    }

    json_t* ConfigToJSON()
    {
        json_t* rootJ = json_object();
        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            json_object_set_new(rootJ, std::string("midi_input_" + std::to_string(i)).c_str(), m_midiInputHandler[i].ToJSON());
        }

        json_object_set_new(rootJ, std::string("launchpad_outputs").c_str(), m_midiLaunchpadOutputHandler.ToJSON());
        json_object_set_new(rootJ, std::string("twister_output").c_str(), m_midiTwisterOutputHandler.ToJSON());

        return rootJ;
    }

    void ConfigFromJSON(json_t* config)
    {
        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            json_t* inputJ = json_object_get(config, std::string("midi_input_" + std::to_string(i)).c_str());
            if (inputJ)
            {
                m_midiInputHandler[i].FromJSON(inputJ);
            }
        }

        json_t* launchpadOutputsJ = json_object_get(config, std::string("launchpad_outputs").c_str());
        if (launchpadOutputsJ)
        {   
            m_midiLaunchpadOutputHandler.FromJSON(launchpadOutputsJ);
        }

        json_t* midiTwisterOutputJ = json_object_get(config, std::string("twister_output").c_str());
        if (midiTwisterOutputJ)
        {
            m_midiTwisterOutputHandler.FromJSON(midiTwisterOutputJ);
        }
    }

    TheNonagonSquiggleBoyQuadLaunchpadTwister m_nonagon;
    MidiInputHandler m_midiInputHandler[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes];
    MidiLaunchpadOutputHandler m_midiLaunchpadOutputHandler;
    MidiTwisterOutputHandler m_midiTwisterOutputHandler;
    TheNonagonSquiggleBoyInternal* m_internal;
};

struct NonagonWrapper
{
    TheNonagonSquiggleBoyInternal m_internal;
    NonagonWrapperQuadLaunchpadTwister m_quadLaunchpadTwister;

    NonagonWrapper()
        : m_quadLaunchpadTwister(&m_internal)
    {
    }
    
    void OpenInputQuadLaunchpadTwister(int index, const juce::String &deviceIdentifier)
    {
        m_quadLaunchpadTwister.OpenInput(index, deviceIdentifier);
    }

    void OpenLaunchpadOutputQuadLaunchpadTwister(int index, SmartGrid::ControllerShape shape, const juce::String &deviceIdentifier)
    {
        m_quadLaunchpadTwister.OpenLaunchpadOutput(index, shape, deviceIdentifier);
    }

    void OpenTwisterOutputQuadLaunchpadTwister(const juce::String &deviceIdentifier)
    {
        m_quadLaunchpadTwister.OpenTwisterOutput(deviceIdentifier);
    }

    QuadFloat ProcessSample(size_t timestamp)
    {
        m_quadLaunchpadTwister.ProcessSample(timestamp);
        return m_internal.Process();
    }

    void ProcessFrame()
    {
        m_quadLaunchpadTwister.ProcessFrame();
        m_internal.PopulateUIState();
    }

    juce::MidiInput* GetMidiInputQuadLaunchpadTwister(int index)
    {
        return m_quadLaunchpadTwister.GetMidiInput(index);
    }

    juce::MidiOutput* GetMidiOutputQuadLaunchpadTwister(int index)
    {
        return m_quadLaunchpadTwister.GetMidiOutput(index);
    }

    SmartGrid::ControllerShape GetControllerShapeQuadLaunchpadTwister(int index)
    {
        return m_quadLaunchpadTwister.GetControllerShape(index);
    }

    json_t* ConfigToJSON()
    {
        return m_quadLaunchpadTwister.ConfigToJSON();
    }

    void ConfigFromJSON(json_t* config)
    {
        m_quadLaunchpadTwister.ConfigFromJSON(config);
    }

    json_t* ToJSON()
    {
        return m_internal.ToJSON();
    }
    
    void FromJSON(json_t* patch)
    {
        m_internal.FromJSON(patch);
        m_internal.SaveJSON();
    }

    void Process(const juce::AudioSourceChannelInfo& bufferToFill)
    {
        double wallclockUs = juce::Time::getMillisecondCounterHiRes() * 1000;

        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            size_t timestamp = static_cast<size_t>(wallclockUs + i * (1000.0 / 48.0));
            QuadFloat output = ProcessSample(timestamp);
            for (int j = 0; j < 4; ++j)
            {
                bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = output[j];
            }
        }

        ProcessFrame();
        m_quadLaunchpadTwister.SendMidiOutput();
    }
};