#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "WavWriter.hpp"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "Noise.hpp"

struct QuadMixerInternal
{
    static constexpr size_t x_numSends = 2;

    QuadFloat m_output;
    QuadFloat m_send[x_numSends];
    const WaveTable* m_sin;
    MultichannelWavWriter m_wavWriter;
    std::string m_recordingDirectory;
    bool m_isRecording = false;
    TanhSaturator<true> m_saturator;
    PinkNoise m_pinkNoise;

    QuadMixerInternal()
    {
        m_sin = &WaveTable::GetSine();
    }
    
    struct Input
    {
        size_t m_numInputs;
        float m_input[16];
        float m_gain[16];
        float m_sendGain[16][x_numSends];
        float m_x[16];
        float m_y[16];
        QuadFloat m_return[x_numSends];
        float m_returnGain[x_numSends];
        bool m_noiseMode;

        Input()
        {
            m_noiseMode = false;
            m_numInputs = 0;
            for (size_t i = 0; i < 16; ++i)
            {
                m_input[i] = 0.0f;
                m_gain[i] = 1.0f;
                m_x[i] = 0.0f;
                m_y[i] = 0.0f;
            }
            
            for (size_t i = 0; i < x_numSends; ++i)
            {
                m_returnGain[i] = 1.0f;
            }
        }
    };

    bool Open(size_t numInputs, const std::string& filename, uint32_t sampleRate)
    {
        return m_wavWriter.OpenQuad(static_cast<uint16_t>(numInputs + x_numSends), filename, sampleRate);
    }

    void Close()
    {
        m_wavWriter.Close();
    }

    void StartRecording(size_t numInputs, uint32_t sampleRate)
    {
        if (m_isRecording || m_recordingDirectory.empty())
        {
            return;
        }

        // Find an empty slot
        //
        size_t ordinal = 1;
        std::string filename;
        
        do
        {
            std::ostringstream oss;
            oss << m_recordingDirectory << "/recording-" << std::setfill('0') << std::setw(5) << ordinal << ".wav";
            filename = oss.str();
            ordinal++;
            INFO("filename: %s", filename.c_str());
        } while (rack::system::exists(filename));

        // Open the wave writer with the appropriate number of channels
        //
        if (!Open(numInputs, filename, sampleRate))
        {
            assert(false);
        }   

        m_isRecording = true;
    }

    void StopRecording()
    {
        if (!m_isRecording)
        {
            return;
        }

        Close();
        m_isRecording = false;
    }

    void ToggleRecording(size_t numChannels, uint32_t sampleRate)
    {
        if (m_isRecording)
        {
            StopRecording();
        }
        else
        {
            StartRecording(numChannels, sampleRate);
        }
    }

    void ProcessInputs(const Input& input)
    {
        m_output = QuadFloat();
        for (size_t i = 0; i < x_numSends; ++i)
        {
            m_send[i] = QuadFloat();
        }
            
        if (input.m_noiseMode)
        {
            float pink = m_pinkNoise.Generate();
            m_output += QuadFloat(pink, pink, pink, pink);
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                m_output += QuadFloat(input.m_input[i], input.m_input[i], input.m_input[i], input.m_input[i]) * input.m_gain[i];
            }
        }
        else
        {
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                QuadFloat pan = QuadFloat::Pan(input.m_x[i], input.m_y[i], input.m_input[i], m_sin);
                QuadFloat postFader = pan * input.m_gain[i];
                m_output += postFader;
                
                // Write post-fader input to wave file
                //
                m_wavWriter.WriteSampleIfOpen(static_cast<uint16_t>(i), postFader);
                
                for (size_t j = 0; j < x_numSends; ++j)
                {
                    m_send[j] += pan * input.m_sendGain[i][j];
                }
            }
        }
    }

    QuadFloat ProcessReturns(const Input& input)
    {
        if (!input.m_noiseMode)
        {
            for (size_t j = 0; j < x_numSends; ++j)
            {
                QuadFloat postReturn = input.m_return[j] * input.m_returnGain[j];
                m_output += postReturn;
                
                // Write post-return to wave file
                //
                m_wavWriter.WriteSampleIfOpen(static_cast<uint16_t>(input.m_numInputs + j), postReturn);
            }
        }

        return m_saturator.Process(m_output);
    }

    QuadFloat Process(const Input& input)
    {
        ProcessInputs(input);
        return ProcessReturns(input);
    }
};

#ifndef IOS_BUILD
struct QuadMixer : Module
{
    QuadMixerInternal m_internal;
    QuadMixerInternal::Input m_state;
    float m_recGateValue = 0.0f;

    IOMgr m_ioMgr;
    IOMgr::Input* m_input;
    IOMgr::Input* m_gain;
    IOMgr::Input* m_sendGain[QuadMixerInternal::x_numSends];
    IOMgr::Input* m_x;
    IOMgr::Input* m_y;
    IOMgr::Input* m_return[QuadMixerInternal::x_numSends];
    IOMgr::Input* m_returnGain[QuadMixerInternal::x_numSends];
    IOMgr::Input* m_recGate;

    IOMgr::Output* m_output;
    IOMgr::Output* m_send[QuadMixerInternal::x_numSends];

    QuadMixer()
        : m_ioMgr(this)
    {
        m_input = m_ioMgr.AddInput("Input", true);

        m_gain = m_ioMgr.AddInput("Gain", true);
        m_gain->m_scale = 0.1;
        
        m_x = m_ioMgr.AddInput("X", true);
        m_x->m_scale = 0.1;
        
        m_y = m_ioMgr.AddInput("Y", true);
        m_y->m_scale = 0.1;

        m_output = m_ioMgr.AddOutput("Output", true);

        for (size_t i = 0; i < 16; ++i)
        {
            m_input->SetTarget(i, &m_state.m_input[i]);
            m_gain->SetTarget(i, &m_state.m_gain[i]);
            m_x->SetTarget(i, &m_state.m_x[i]);
            m_y->SetTarget(i, &m_state.m_y[i]);
        }

        for (size_t j = 0; j < 4; ++j)
        {
            m_output->SetSource(j, &m_internal.m_output[j]);
        }

        for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
        {
            m_sendGain[i] = m_ioMgr.AddInput("Send Gain " + std::to_string(i + 1), true);
            m_sendGain[i]->m_scale = 0.1;

            m_return[i] = m_ioMgr.AddInput("Return " + std::to_string(i + 1), true);

            m_returnGain[i] = m_ioMgr.AddInput("Return Gain " + std::to_string(i + 1), true);
            m_returnGain[i]->m_scale = 0.1;

            m_send[i] = m_ioMgr.AddOutput("Send " + std::to_string(i + 1), true);

            for (size_t j = 0; j < 16; ++j)
            {
                m_sendGain[i]->SetTarget(j, &m_state.m_sendGain[j][i]);
            }

            for (size_t j = 0; j < 4; ++j)
            {
                m_return[i]->SetTarget(j, &m_state.m_return[i][j]);
                m_send[i]->SetSource(j, &m_internal.m_send[i][j]);
            }

            m_returnGain[i]->SetTarget(0, &m_state.m_returnGain[i]);
        }

        m_recGate = m_ioMgr.AddInput("Record Gate", true);
        m_recGate->SetTarget(0, &m_recGateValue);

        m_ioMgr.Config();
        m_output->SetChannels(4);

        for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
        {
            m_send[i]->SetChannels(4);
        }
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        m_state.m_numInputs = m_input->m_value.m_channels;

        if (m_recGateValue >= 5.0f)
        {
            m_internal.StartRecording(m_state.m_numInputs + QuadMixerInternal::x_numSends, args.sampleRate);
        }
        else 
        {
            m_internal.StopRecording();
        }
        
        m_internal.Process(m_state);
        m_ioMgr.SetOutputs();
    }

    virtual json_t* dataToJson() override
    {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "recordingDirectory", json_string(m_internal.m_recordingDirectory.c_str()));
        return rootJ;
    }

    virtual void dataFromJson(json_t* rootJ) override
    {
        json_t* dirJ = json_object_get(rootJ, "recordingDirectory");
        if (dirJ)
        {
            m_internal.m_recordingDirectory = json_string_value(dirJ);
        }
    }
};

struct QuadMixerWidget : public ModuleWidget
{
    QuadMixerWidget(QuadMixer* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadMixer.svg")));

        if (module)
        {
            module->m_input->Widget(this, 1, 1);
            module->m_gain->Widget(this, 1, 2);
            module->m_x->Widget(this, 1, 3);
            module->m_y->Widget(this, 1, 4);
            module->m_recGate->Widget(this, 1, 5);

            for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
            {
                module->m_send[i]->Widget(this, 2 + i, 1);
                module->m_sendGain[i]->Widget(this, 2 + i, 2);
                module->m_return[i]->Widget(this, 2 + i, 3);
                module->m_returnGain[i]->Widget(this, 2 + i, 4);
            }

            module->m_output->Widget(this, 2 + QuadMixerInternal::x_numSends, 1);
        }
    }

    void appendContextMenu(Menu* menu) override
    {
        QuadMixer* module = dynamic_cast<QuadMixer*>(this->module);
        if (!module)
        {
            return;
        }

        menu->addChild(new MenuSeparator());

        // Add menu item to select recording directory
        //
        menu->addChild(createSubmenuItem("Recording Directory", "", [=](Menu* menu) {
            // Show current directory
            //
            std::string currentDir = module->m_internal.m_recordingDirectory.empty() ? "Not set" : module->m_internal.m_recordingDirectory;
            menu->addChild(createMenuLabel("Current: " + currentDir));
            
            menu->addChild(new MenuSeparator());
            
            // Add option to select directory
            //
            menu->addChild(createMenuItem("Select Directory", "", [=]() {
                const char* path = osdialog_file(OSDIALOG_OPEN_DIR, module->m_internal.m_recordingDirectory.empty() ? nullptr : module->m_internal.m_recordingDirectory.c_str(), nullptr, nullptr);
                if (path)
                {
                    module->m_internal.m_recordingDirectory = path;
                    free((void*)path);
                }
            }));
        }));
    }
};
#endif
            

