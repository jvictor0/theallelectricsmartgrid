#pragma once

#include <JuceHeader.h>
#include "MidiHandlers.hpp"
#include "SmartGridInclude.hpp"
#include "MidiSender.hpp"

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

    NonagonWrapperQuadLaunchpadTwister(TheNonagonSquiggleBoyInternal* internal, MidiSender* midiSender)
        : m_nonagon(internal)
        , m_internal(internal)
        , m_midiSender(midiSender)
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

        m_midiSender->AllocateRoute(&m_midiLaunchpadOutputHandler[0]);
        m_midiSender->AllocateRoute(&m_midiLaunchpadOutputHandler[1]);
        m_midiSender->AllocateRoute(&m_midiLaunchpadOutputHandler[2]);
        m_midiSender->AllocateRoute(&m_midiLaunchpadOutputHandler[3]);
        m_midiSender->AllocateRoute(&m_midiEncoderOutputHandler);
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

    void ProcessSample()
    {
        m_nonagon.ProcessSample();
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
    MidiSender* m_midiSender;
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
        MidiSender* m_midiSender;

        MidiOutputHandler(TheNonagonSquiggleBoyWrldBldr* owner, MidiSender* midiSender)
            : ::MidiOutputHandler()
            , m_owner(owner)
            , m_midiWriter(owner)
            , m_midiSender(midiSender)
        {
        }

        void Process() override
        {
            if (m_midiOutput.get())
            {
                size_t budget = 256;
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

                    SmartGrid::BasicMidi msg = (*itr);
                    m_midiSender->SendMessage(msg, m_routeId);
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

    struct KMixMidiOutputHandler : ::MidiOutputHandler
    {
        TheNonagonSquiggleBoyInternal* m_internal;

        KMixMidiOutputHandler(TheNonagonSquiggleBoyInternal* internal)
            : ::MidiOutputHandler()
            , m_internal(internal)
        {
        }

        void Process() override
        {
            if (m_midiOutput.get())
            {
                for (auto msg : m_internal->m_ioState.m_kMixMidi)
                {
                    juce::MidiMessage message(msg.m_msg, 3);
                    m_midiOutput->sendMessageNow(message);
                }
            }
        }

        void Reset() override
        {
            m_internal->m_ioState.m_kMixMidi.Reset();
        }
    };

    NonagonWrapperWrldBldr(TheNonagonSquiggleBoyInternal* internal, MidiSender* midiSender)
        : m_nonagon(internal)
        , m_internal(internal)
        , m_midiInputHandler(&m_nonagon)
        , m_midiOutputHandler(&m_nonagon, midiSender)
        , m_kMixMidiOutputHandler(internal)
        , m_midiSender(midiSender)
    {
        m_midiSender->AllocateRoute(&m_midiOutputHandler);
        m_midiSender->m_clockRouteId = m_midiOutputHandler.m_routeId;
        m_midiSender->AllocateRoute(&m_kMixMidiOutputHandler);
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

    void OpenKMixOutput(const juce::String &deviceIdentifier)
    {
        m_kMixMidiOutputHandler.Open(deviceIdentifier);
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
        m_kMixMidiOutputHandler.Process();
    }

    void ClearLEDs()
    {
        m_midiOutputHandler.ClearLEDs();
    }

    void ProcessSample()
    {
        m_nonagon.ProcessSample();
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

    juce::MidiOutput* GetKMixOutput()
    {
        return m_kMixMidiOutputHandler.m_midiOutput.get();
    }

    JSON ConfigToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("midi_input", m_midiInputHandler.ToJSON());
        rootJ.SetNew("midi_output", m_midiOutputHandler.ToJSON());
        rootJ.SetNew("kmix_output", m_kMixMidiOutputHandler.ToJSON());
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

        JSON kmixOutputJ = config.Get("kmix_output");
        if (!kmixOutputJ.IsNull())
        {
            m_kMixMidiOutputHandler.FromJSON(kmixOutputJ);
        }

        if (IsOpen())
        {
            SendHandshake();
        }
    }

    TheNonagonSquiggleBoyWrldBldr m_nonagon;
    TheNonagonSquiggleBoyInternal* m_internal;
    MidiInputHandler m_midiInputHandler;
    MidiOutputHandler m_midiOutputHandler;
    KMixMidiOutputHandler m_kMixMidiOutputHandler;
    MidiSender* m_midiSender;
}; 

struct NonagonWrapper
{
    MidiSender m_midiSender;
    TheNonagonSquiggleBoyInternal m_internal;
    NonagonWrapperQuadLaunchpadTwister m_quadLaunchpadTwister;
    NonagonWrapperWrldBldr m_wrldBldr;

    struct IOInfo
    {
        int m_numInputs;
        int m_numOutputs;
        int m_numChannels;
        bool m_stereo;
    };

    NonagonWrapper()
        : m_quadLaunchpadTwister(&m_internal, &m_midiSender)
        , m_wrldBldr(&m_internal, &m_midiSender)
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

    QuadFloatWithStereoAndSub ProcessSample(const AudioInputBuffer& audioInputBuffer)
    {
        m_quadLaunchpadTwister.ProcessSample();
        m_wrldBldr.ProcessSample();
        QuadFloatWithStereoAndSub output = m_internal.ProcessSample(audioInputBuffer);
        m_midiSender.ProcessMessagesOut(m_internal.m_messageOutBuffer, SampleTimer::GetAbsTimeUs());
        return output;
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

    void OpenKMixOutput(const juce::String &deviceIdentifier)
    {
        m_wrldBldr.OpenKMixOutput(deviceIdentifier);
    }

    juce::MidiOutput* GetKMixOutput()
    {
        return m_wrldBldr.GetKMixOutput();
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

    void Process(const juce::AudioSourceChannelInfo& bufferToFill, IOInfo ioInfo)
    {
        double wallclockUs = juce::Time::getMillisecondCounterHiRes() * 1000;
        SampleTimer::StartFrame(wallclockUs);

        AudioInputBuffer audioInputBuffer;
        audioInputBuffer.m_numInputs = std::min(ioInfo.m_numInputs, 4);

        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            for (int j = 0; j < audioInputBuffer.m_numInputs; ++j)
            {
                audioInputBuffer.m_input[j] = bufferToFill.buffer->getReadPointer(j, bufferToFill.startSample)[i];
            }

            SampleTimer::IncrementSample();
            QuadFloatWithStereoAndSub output = ProcessSample(audioInputBuffer);
            
            if (ioInfo.m_stereo)
            {
                for (int j = 0; j < ioInfo.m_numChannels; ++j)
                {
                    bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = output.m_stereoOutput[j];
                }
            }
            else
            {
                for (int j = 0; j < ioInfo.m_numChannels; ++j)
                {
                    bufferToFill.buffer->getWritePointer(j, bufferToFill.startSample)[i] = output.m_output[j];
                }
            }

            if (ioInfo.m_numOutputs > 4)
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
