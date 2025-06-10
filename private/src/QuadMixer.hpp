#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"

struct QuadMixerInternal
{
    static constexpr size_t x_numSends = 2;

    QuadFloat m_output;
    QuadFloat m_send[x_numSends];
    const WaveTable* m_sin;

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

        Input()
        {
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

    void Process(const Input& input)
    {
        m_output = QuadFloat();
        for (size_t i = 0; i < x_numSends; ++i)
        {
            m_send[i] = QuadFloat();
        }

        for (size_t i = 0; i < input.m_numInputs; ++i)
        {
            QuadFloat pan = QuadFloat::Pan(input.m_x[i], input.m_y[i], input.m_input[i], m_sin);
            m_output += pan * input.m_gain[i];
            for (size_t j = 0; j < x_numSends; ++j)
            {
                m_send[j] += pan * input.m_sendGain[i][j];
            }
        }

        for (size_t j = 0; j < x_numSends; ++j)
        {
            m_output += input.m_return[j] * input.m_returnGain[j];
        }
    }
};

struct QuadMixer : Module
{
    QuadMixerInternal m_internal;
    QuadMixerInternal::Input m_state;

    IOMgr m_ioMgr;
    IOMgr::Input* m_input;
    IOMgr::Input* m_gain;
    IOMgr::Input* m_sendGain[QuadMixerInternal::x_numSends];
    IOMgr::Input* m_x;
    IOMgr::Input* m_y;
    IOMgr::Input* m_return[QuadMixerInternal::x_numSends];
    IOMgr::Input* m_returnGain[QuadMixerInternal::x_numSends];

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
        m_internal.Process(m_state);
        m_ioMgr.SetOutputs();
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
};
            

