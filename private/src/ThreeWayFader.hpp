#pragma once

#include "ModuleUtils.hpp"

#ifndef IOS_BUILD
struct ThreeWayFader : Module
{
    float m_input[16][3];
    float m_fade[16];

    float m_output[16];

    IOMgr m_ioMgr;
    IOMgr::Input* m_inputInput[3];
    IOMgr::Input* m_fadeInput;
    IOMgr::Output* m_outputOutput;

    ThreeWayFader()
        : m_ioMgr(this)
    {
        m_fadeInput = m_ioMgr.AddInput("Fade", true);
        m_outputOutput = m_ioMgr.AddOutput("Output", true);

        m_fadeInput->m_scale = 0.1;

        for (size_t i = 0; i < 3; ++i)
        {
            m_inputInput[i] = m_ioMgr.AddInput("Input " + std::to_string(i + 1), true);
            for (size_t j = 0; j < 16; ++j)
            {
                m_inputInput[i]->SetTarget(j, &m_input[j][i]);
            }
        }

        for (size_t j = 0; j < 16; ++j)
        {
            m_fadeInput->SetTarget(j, &m_fade[j]);
            m_outputOutput->SetSource(j, &m_output[j]);
        }

        m_ioMgr.Config();
    }

    void process(const ProcessArgs& args) override
    {
        m_ioMgr.Process();
        for (size_t j = 0; j < m_fadeInput->m_value.m_channels; ++j)
        {
            float fade = m_fade[j];
            if (fade < 0.5)
            {
                m_output[j] = m_input[j][0] * (1.0f - 2.0f * fade) + m_input[j][1] * (2.0f * fade);
            }
            else
            {
                m_output[j] = m_input[j][1] * (1.0f - 2.0f * (fade - 0.5f)) + m_input[j][2] * (2.0f * (fade - 0.5f));
            }
        }

        m_outputOutput->SetChannels(m_fadeInput->m_value.m_channels);
        
        m_ioMgr.SetOutputs();
    }
};

struct ThreeWayFaderWidget : public ModuleWidget
{
    ThreeWayFaderWidget(ThreeWayFader* module)
    {
        setModule(module);
        
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ThreeWayFader.svg")));
        
        if (module)
        {
            module->m_inputInput[0]->Widget(this, 1, 1);
            module->m_inputInput[1]->Widget(this, 1, 2);
            module->m_inputInput[2]->Widget(this, 1, 3);
            module->m_fadeInput->Widget(this, 2, 1);
            module->m_outputOutput->Widget(this, 3, 1);
        }
    }
};
#endif
            


    
    
