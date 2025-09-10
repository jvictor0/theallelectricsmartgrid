#pragma once

#include <JuceHeader.h>
#include "MidiUtils.hpp"
#include "SmartGridInclude.hpp"

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

        JSON ToJSON()
        {
            JSON rootJ = JSON::Object();
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                rootJ.SetNew(std::string("launchpad_output_" + std::to_string(i)).c_str(), JSON::String(m_name[i].toUTF8()));
                rootJ.SetNew(std::string("launchpad_output_shape_" + std::to_string(i)).c_str(), JSON::Integer(static_cast<int>(m_sysexWriter[i].m_shape)));
            }

            return rootJ;
        }

        void FromJSON(JSON rootJ)
        {
            for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
            {
                JSON nameJ = rootJ.Get(std::string("launchpad_output_" + std::to_string(i)).c_str());
                if (!nameJ.IsNull())
                {
                    m_name[i] = juce::String(nameJ.StringValue());
                }

                JSON shapeJ = rootJ.Get(std::string("launchpad_output_shape_" + std::to_string(i)).c_str());
                if (!shapeJ.IsNull())
                {
                    m_sysexWriter[i].m_shape = static_cast<SmartGrid::ControllerShape>(shapeJ.IntegerValue());
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

        JSON ToJSON()
        {
            JSON rootJ = JSON::Object();
            rootJ.SetNew("midi_output", JSON::String(m_name.toUTF8()));
            return rootJ;
        }

        void FromJSON(JSON rootJ)
        {
            JSON nameJ = rootJ.Get("midi_output");
            if (!nameJ.IsNull())
            {
                m_name = juce::String(nameJ.StringValue());
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

    JSON ConfigToJSON()
    {
        JSON rootJ = JSON::Object();
        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            rootJ.SetNew(std::string("midi_input_" + std::to_string(i)).c_str(), m_midiInputHandler[i].ToJSON());
        }

        rootJ.SetNew(std::string("launchpad_outputs").c_str(), m_midiLaunchpadOutputHandler.ToJSON());
        rootJ.SetNew(std::string("twister_output").c_str(), m_midiTwisterOutputHandler.ToJSON());

        return rootJ;
    }

    void ConfigFromJSON(JSON config)
    {
        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            JSON inputJ = config.Get(std::string("midi_input_" + std::to_string(i)).c_str());
            if (!inputJ.IsNull())
            {
                m_midiInputHandler[i].FromJSON(inputJ);
            }
        }

        JSON launchpadOutputsJ = config.Get(std::string("launchpad_outputs").c_str());
        if (!launchpadOutputsJ.IsNull())
        {   
            m_midiLaunchpadOutputHandler.FromJSON(launchpadOutputsJ);
        }

        JSON midiTwisterOutputJ = config.Get(std::string("twister_output").c_str());
        if (!midiTwisterOutputJ.IsNull())
        {
            m_midiTwisterOutputHandler.FromJSON(midiTwisterOutputJ);
        }
    }

    PadUIGrid MkLaunchPadUIGrid(int index)
    {
        return m_nonagon.MkLaunchPadUIGrid(index);
    }

    TheNonagonSquiggleBoyQuadLaunchpadTwister m_nonagon;
    MidiInputHandler m_midiInputHandler[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes];
    MidiLaunchpadOutputHandler m_midiLaunchpadOutputHandler;
    MidiTwisterOutputHandler m_midiTwisterOutputHandler;
    TheNonagonSquiggleBoyInternal* m_internal;
};

struct NonagonWrapperWrldBldr
{
    NonagonWrapperWrldBldr(TheNonagonSquiggleBoyInternal* internal)
        : m_nonagon(internal)
        , m_internal(internal)
    {
    }

    void ProcessSample(size_t timestamp)
    {
        m_nonagon.ProcessSample(timestamp);
    }

    void ProcessFrame()
    {
        m_nonagon.ProcessFrame();
    }

    PadUIGrid MkLaunchPadUIGrid(TheNonagonSquiggleBoyWrldBldr::Routes route)
    {
        return m_nonagon.MkLaunchPadUIGrid(route);
    }

    EncoderBankUI MkEncoderBankUI()
    {
        return m_nonagon.MkEncoderBankUI();
    }

    AnalogUI MkAnalogUI(int ix)
    {
        return m_nonagon.MkAnalogUI(ix);
    }

    TheNonagonSquiggleBoyWrldBldr m_nonagon;
    TheNonagonSquiggleBoyInternal* m_internal;
}; 

struct NonagonWrapper
{
    TheNonagonSquiggleBoyInternal m_internal;
    NonagonWrapperQuadLaunchpadTwister m_quadLaunchpadTwister;
    NonagonWrapperWrldBldr m_wrldBldr;

    NonagonWrapper()
        : m_quadLaunchpadTwister(&m_internal)
        , m_wrldBldr(&m_internal)
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
        m_wrldBldr.ProcessSample(timestamp);
        return m_internal.Process();
    }

    void ProcessFrame()
    {
        m_quadLaunchpadTwister.ProcessFrame();
        m_wrldBldr.ProcessFrame();
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

    JSON ConfigToJSON()
    {
        return m_quadLaunchpadTwister.ConfigToJSON();
    }

    void ConfigFromJSON(JSON config)
    {
        m_quadLaunchpadTwister.ConfigFromJSON(config);
    }

    JSON ToJSON()
    {
        return m_internal.ToJSON();
    }
    
    void FromJSON(JSON patch)
    {
        m_internal.FromJSON(patch);
        m_internal.SaveJSON();
    }

    PadUIGrid MkLaunchPadUIGridQuadLaunchpadTwister(int index)
    {
        return m_quadLaunchpadTwister.MkLaunchPadUIGrid(index);
    }

    PadUIGrid MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes route)
    {
        return m_wrldBldr.MkLaunchPadUIGrid(route);
    }

    EncoderBankUI MkEncoderBankUI()
    {
        return m_wrldBldr.MkEncoderBankUI();
    }

    AnalogUI MkAnalogUI(int ix)
    {
        return m_wrldBldr.MkAnalogUI(ix);
    }

    void Process(const juce::AudioSourceChannelInfo& bufferToFill)
    {
        double wallclockUs = juce::Time::getMillisecondCounterHiRes() * 1000;

        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            size_t timestamp = static_cast<size_t>(wallclockUs + i * (1000.0 / 48.0));
            QuadFloat output = ProcessSample(timestamp);
            for (int j = 0; j < std::min(4, bufferToFill.buffer->getNumChannels()); ++j)
            {
                bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = output[j];
            }
        }

        ProcessFrame();
        m_quadLaunchpadTwister.SendMidiOutput();
    }
};
