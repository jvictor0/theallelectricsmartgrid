#pragma once

#include <JuceHeader.h>
#include "MidiHandlers.hpp"
#include "SmartGridInclude.hpp"

struct NonagonWrapperQuadLaunchpadTwister
{
    struct QuadLaunchpadTwisterMidiHandler : MidiInputHandler
    {
        TheNonagonSquiggleBoyQuadLaunchpadTwister* m_owner;

        QuadLaunchpadTwisterMidiHandler()
            : MidiInputHandler()
            , m_owner(nullptr)
        {
        }

        void Init(TheNonagonSquiggleBoyQuadLaunchpadTwister* owner, int routeId)
        {
            m_routeId = routeId;
            m_owner = owner;
        }

        void SendMessage(SmartGrid::BasicMidi msg) override
        {
            m_owner->SendMessage(msg);
        }
    };

    struct MidiLaunchpadOutputHandler : ::MidiOutputHandler
    {
        SmartGrid::LPSysexWriter m_sysexWriter;

        MidiLaunchpadOutputHandler()
        {
        }

        void Init(SmartGrid::ControllerShape shape, SmartGrid::SmartBusColor* colorBus)
        {
            m_shape = shape;
            m_sysexWriter = SmartGrid::LPSysexWriter(shape, colorBus);
        }

        void Open(SmartGrid::ControllerShape shape, const juce::String &deviceIdentifier)
        {
            m_shape = shape;
            m_sysexWriter.m_shape = shape;
            MidiOutputHandler::Open(deviceIdentifier);
        }

        void Reset() override
        {
            m_sysexWriter.m_shape = m_shape;
            m_sysexWriter.Reset();
        }

        void Process() override
        {
            if (m_midiOutput.get())
            {
                uint8_t buffer[SmartGrid::LPSysexWriter::x_maxMessageSize];
                size_t size = m_sysexWriter.Write(buffer);
                if (size > 0)
                {
                    m_midiOutput->sendMessageNow(juce::MidiMessage(buffer, static_cast<int>(size)));
                }
            }
        }
    };

    struct MidiEncoderOutputHandler : MidiOutputHandler
    {
        SmartGrid::EncoderMidiWriter m_midiWriter;

        MidiEncoderOutputHandler()
            : m_midiWriter(nullptr)
        {
            m_shape = SmartGrid::ControllerShape::MidiFighterTwister;
        }

        void Init(EncoderBankUIState* encoderBankState)
        {
            m_midiWriter = SmartGrid::EncoderMidiWriter(encoderBankState);
        }

        void Reset() override
        {
            m_midiWriter.Reset();
        }

        void Process() override
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
    };

    NonagonWrapperQuadLaunchpadTwister(TheNonagonSquiggleBoyInternal* internal)
        : m_nonagon(internal)
        , m_internal(internal)
    {
        for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            m_midiInputHandler[i].Init(&m_nonagon, i);
        }

        for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
        {
            m_midiLaunchpadOutputHandler[i].Init(SmartGrid::ControllerShape::LaunchPadX, &m_nonagon.m_uiState.m_colorBus[i]);
        }

        m_midiEncoderOutputHandler.Init(&internal->m_uiState.m_squiggleBoyUIState.m_encoderBankUIState);
    }

    void OpenInput(int index, const juce::String &deviceIdentifier)
    {
        m_midiInputHandler[index].Open(deviceIdentifier);
    }

    void OpenLaunchpadOutput(int index, SmartGrid::ControllerShape shape, const juce::String &deviceIdentifier)
    {
        m_midiLaunchpadOutputHandler[index].Open(shape, deviceIdentifier);
    }

    void OpenEncoderOutput(const juce::String &deviceIdentifier)
    {
        m_midiEncoderOutputHandler.Open(deviceIdentifier);
    }

    juce::MidiInput* GetMidiInput(int index)
    {
        return m_midiInputHandler[index].m_midiInput.get();
    }

    juce::MidiOutput* GetMidiOutput(int index)
    {
        if (index < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads)
        {
            return m_midiLaunchpadOutputHandler[index].m_midiOutput.get();
        }
        else
        {
            return m_midiEncoderOutputHandler.m_midiOutput.get();
        }
    }

    SmartGrid::ControllerShape GetControllerShape(int index)
    {
        return m_midiLaunchpadOutputHandler[index].m_shape;
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
        for (int i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
        {
            m_midiLaunchpadOutputHandler[i].Process();
        }

        m_midiEncoderOutputHandler.Process();
    }

    JSON ConfigToJSON()
    {
        JSON rootJ = JSON::Object();
        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes; ++i)
        {
            rootJ.SetNew(std::string("midi_input_" + std::to_string(i)).c_str(), m_midiInputHandler[i].ToJSON());
        }

        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
        {
            rootJ.SetNew(std::string("launchpad_output_" + std::to_string(i)).c_str(), m_midiLaunchpadOutputHandler[i].ToJSON());
        }

        rootJ.SetNew(std::string("encoder_output").c_str(), m_midiEncoderOutputHandler.ToJSON());

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

        for (size_t i = 0; i < TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads; ++i)
        {
            JSON launchpadOutputJ = config.Get(std::string("launchpad_output_" + std::to_string(i)).c_str());
            if (!launchpadOutputJ.IsNull())
            {
                m_midiLaunchpadOutputHandler[i].FromJSON(launchpadOutputJ);
            }
        }

        JSON encoderOutputJ = config.Get(std::string("encoder_output").c_str());
        if (!encoderOutputJ.IsNull())
        {
            m_midiEncoderOutputHandler.FromJSON(encoderOutputJ);
        }
    }

    PadUIGrid MkLaunchPadUIGrid(int index)
    {
        return m_nonagon.MkLaunchPadUIGrid(index);
    }

    TheNonagonSquiggleBoyQuadLaunchpadTwister m_nonagon;
    QuadLaunchpadTwisterMidiHandler m_midiInputHandler[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numRoutes];
    MidiLaunchpadOutputHandler m_midiLaunchpadOutputHandler[TheNonagonSquiggleBoyQuadLaunchpadTwister::x_numLaunchpads];
    MidiEncoderOutputHandler m_midiEncoderOutputHandler;
    TheNonagonSquiggleBoyInternal* m_internal;
};

struct NonagonWrapperWrldBldr
{
    struct MidiInputHandler : ::MidiInputHandler
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;

        MidiInputHandler(TheNonagonSquiggleBoyWrldBldr* owner)
            : ::MidiInputHandler()
            , m_owner(owner)
        {
        }

        void SendMessage(SmartGrid::BasicMidi msg) override
        {
            SmartGrid::MessageIn message = SmartGrid::WrldBLDRMidi::FromMidi(msg);
            m_owner->SendMessage(message);
        }
    };

    struct MidiOutputHandler : ::MidiOutputHandler
    {
        TheNonagonSquiggleBoyWrldBldr* m_owner;
        SmartGrid::WrldBLDRMidiWriter m_midiWriter;

        MidiOutputHandler(TheNonagonSquiggleBoyWrldBldr* owner)
            : ::MidiOutputHandler()
            , m_owner(owner)
            , m_midiWriter(owner)
        {
        }

        void Process() override
        {
            if (m_midiOutput.get())
            {
                size_t budget = 32;
                m_midiWriter.ProcessCoolDown();
                for (size_t i = 0; i < SmartGrid::WrldBLDRMidiWriter::x_numColorWriters; ++i)
                {
                    SmartGrid::YaeltexColorSysexBuffer buffer;
                    m_midiWriter.Write(buffer, budget, i);
                    if (buffer.HasAny())
                    {
                        juce::MidiMessage message(buffer.m_buffer, buffer.m_size);
                        m_midiOutput->sendMessageNow(message);
                    }
                }

                for (auto itr = m_midiWriter.m_indicatorWriter.begin(); !itr.Done(); ++itr)
                {
                    if (budget == 0)
                    {
                        break;
                    }

                    juce::MidiMessage message((*itr).m_msg, 3);
                    m_midiOutput->sendMessageNow(message);
                    --budget;
                }
            }
        }

        void Reset() override
        {
            m_midiWriter.Reset();
        }

        void ClearLEDs()
        {
            if (m_midiOutput.get())
            {
                for (size_t i = 0; i < SmartGrid::WrldBLDRMidiWriter::x_numColorWriters; ++i)
                {
                    SmartGrid::YaeltexColorSysexBuffer buffer;
                    m_midiWriter.WriteClear(buffer, i);
                    juce::MidiMessage message(buffer.m_buffer, buffer.m_size);
                    m_midiOutput->sendMessageNow(message);
                }
            }
        }
    };

    NonagonWrapperWrldBldr(TheNonagonSquiggleBoyInternal* internal)
        : m_nonagon(internal)
        , m_internal(internal)
        , m_midiInputHandler(&m_nonagon)
        , m_midiOutputHandler(&m_nonagon)   
    {
    }

    void OpenInput(const juce::String &deviceIdentifier)
    {
        m_midiInputHandler.Open(deviceIdentifier);
        if (m_midiOutputHandler.m_midiOutput.get())
        {
            SendHandshake();
        }
    }

    void OpenOutput(const juce::String &deviceIdentifier)
    {
        m_midiOutputHandler.Open(deviceIdentifier);
        if (m_midiInputHandler.m_midiInput.get())
        {
            SendHandshake();
        }
    }

    void SendHandshake()
    {
        if (m_midiOutputHandler.m_midiOutput.get())
        {
            uint8_t sysex[] = {0xF0, 0x79, 0x74, 0x78, 0x00, 0x01, 0x00, 0x21, 0xF7};
            juce::MidiMessage message(sysex, sizeof(sysex));
            m_midiOutputHandler.m_midiOutput->sendMessageNow(message);
        }
    }

    void SendMidiOutput()
    {
        m_midiOutputHandler.Process();
    }

    void ClearLEDs()
    {
        m_midiOutputHandler.ClearLEDs();
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

    JSON ConfigToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("midi_input", m_midiInputHandler.ToJSON());
        rootJ.SetNew("midi_output", m_midiOutputHandler.ToJSON());
        return rootJ;
    }

    void ConfigFromJSON(JSON config)
    {
        JSON midiInputJ = config.Get("midi_input");
        if (!midiInputJ.IsNull())
        {
            m_midiInputHandler.FromJSON(midiInputJ);
        }

        JSON midiOutputJ = config.Get("midi_output");
        if (!midiOutputJ.IsNull())
        {
            m_midiOutputHandler.FromJSON(midiOutputJ);
        }

        if (IsOpen())
        {
            SendHandshake();
        }
    }

    bool IsOpen()
    {
        return m_midiInputHandler.m_midiInput.get() && m_midiOutputHandler.m_midiOutput.get();
    }

    juce::MidiInput* GetMidiInput()
    {
        return m_midiInputHandler.m_midiInput.get();
    }

    juce::MidiOutput* GetMidiOutput()
    {
        return m_midiOutputHandler.m_midiOutput.get();
    }

    TheNonagonSquiggleBoyWrldBldr m_nonagon;
    TheNonagonSquiggleBoyInternal* m_internal;
    MidiInputHandler m_midiInputHandler;
    MidiOutputHandler m_midiOutputHandler;
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

    void PrepareToPlay(int numSamples, double sampleRate)
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

    void OpenEncoderOutputQuadLaunchpadTwister(const juce::String &deviceIdentifier)
    {
        m_quadLaunchpadTwister.OpenEncoderOutput(deviceIdentifier);
    }

    QuadFloatWithStereoAndSub ProcessSample(size_t timestamp)
    {
        m_quadLaunchpadTwister.ProcessSample(timestamp);
        m_wrldBldr.ProcessSample(timestamp);
        return m_internal.ProcessSample();
    }

    void ProcessFrame()
    {
        m_quadLaunchpadTwister.ProcessFrame();
        m_wrldBldr.ProcessFrame();
        m_internal.ProcessFrame();
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

    void OpenInputWrldBldr(const juce::String &deviceIdentifier)
    {
        m_wrldBldr.OpenInput(deviceIdentifier);
    }

    void OpenOutputWrldBldr(const juce::String &deviceIdentifier)
    {
        m_wrldBldr.OpenOutput(deviceIdentifier);
    }

    juce::MidiInput* GetMidiInputWrldBldr()
    {
        return m_wrldBldr.GetMidiInput();
    }

    juce::MidiOutput* GetMidiOutputWrldBldr()
    {
        return m_wrldBldr.GetMidiOutput();
    }

    JSON ConfigToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("quad_launchpad_twister", m_quadLaunchpadTwister.ConfigToJSON());
        rootJ.SetNew("wrld_bldr", m_wrldBldr.ConfigToJSON());
        return rootJ;
    }

    void ConfigFromJSON(JSON config)
    {
        JSON quadLaunchpadTwisterJ = config.Get("quad_launchpad_twister");
        if (!quadLaunchpadTwisterJ.IsNull())
        {
            m_quadLaunchpadTwister.ConfigFromJSON(quadLaunchpadTwisterJ);
        }

        JSON wrldBldrJ = config.Get("wrld_bldr");
        if (!wrldBldrJ.IsNull())
        {
            m_wrldBldr.ConfigFromJSON(wrldBldrJ);
        }
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

    ScopeWriter* GetAudioScopeWriter()
    {
        return &m_internal.m_uiState.m_squiggleBoyUIState.m_audioScopeWriter;
    }

    ScopeWriter* GetControlScopeWriter()
    {
        return &m_internal.m_uiState.m_squiggleBoyUIState.m_controlScopeWriter;
    }

    TheNonagonSquiggleBoyInternal::UIState* GetUIState()
    {
        return &m_internal.m_uiState;
    }

    SquiggleBoyWithEncoderBank::UIState* GetSquiggleBoyUIState()
    {
        return &m_internal.m_uiState.m_squiggleBoyUIState;
    }

    TheNonagonInternal::UIState* GetNonagonUIState()
    {
        return &m_internal.m_uiState.m_nonagonUIState;
    }

    SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode GetVisualDisplayMode()
    {
        return m_internal.m_uiState.m_squiggleBoyUIState.m_visualDisplayMode.load();
    }

    StateInterchange* GetStateInterchange()
    {
        return &m_internal.m_stateInterchange;
    }

    NonagonNoteWriter* GetNoteWriter()
    {
        return &m_internal.m_nonagon.m_nonagon.m_noteWriter;
    }

    TheNonagonSquiggleBoyWrldBldr::DisplayMode GetDisplayMode()
    {
        if (m_wrldBldr.IsOpen())
        {
            return TheNonagonSquiggleBoyWrldBldr::DisplayMode::Visualizer;
        }
        else
        {
            return m_wrldBldr.m_nonagon.m_uiState.m_displayMode.load();
        }
    }

    void SetRecordingDirectory(const char* directory)
    {
        m_internal.m_squiggleBoy.SetRecordingDirectory(directory);
    }

    void Process(const juce::AudioSourceChannelInfo& bufferToFill, bool stereo)
    {
        double wallclockUs = juce::Time::getMillisecondCounterHiRes() * 1000;

        size_t numChannels = stereo ? 2 : 4;

        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            size_t timestamp = static_cast<size_t>(wallclockUs + i * (1000.0 / 48.0));
            QuadFloatWithStereoAndSub output = ProcessSample(timestamp);
            
            if (stereo)
            {
                for (int j = 0; j < numChannels; ++j)
                {
                    bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = output.m_stereoOutput[j];
                }
            }
            else
            {
                for (int j = 0; j < numChannels; ++j)
                {
                    bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = output.m_output[j];
                }
            }

            if (bufferToFill.buffer->getNumChannels() > 4)
            {
                bufferToFill.buffer->getWritePointer(4, bufferToFill.startSample)[i] = output.m_sub;
            }
        }

        ProcessFrame();
        m_quadLaunchpadTwister.SendMidiOutput();
        m_wrldBldr.SendMidiOutput();
    }

    void ClearLEDs()
    {
        m_wrldBldr.ClearLEDs();
    }
};
