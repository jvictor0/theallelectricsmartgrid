#pragma once

#include "Filter.hpp"
#include "ModuleUtils.hpp"

struct SaturationBlock : Module
{
    TanhSaturator<true> m_saturator[16];
    OPBaseWidthFilter m_baseWidthFilter[16];

    float m_base[16];
    float m_width[16];

    float m_inputValue[16];
    float m_outputValue[16];

    IOMgr m_ioMgr;
    IOMgr::Input* m_input;
    IOMgr::Input* m_saturationInputGain;
    IOMgr::Input* m_bwBase;
    IOMgr::Input* m_bwWidth;

    IOMgr::Output* m_output;

    SaturationBlock()
        : m_ioMgr(this)
    {
        for (int i = 0; i < 16; ++i)
        {
            m_base[i] = 0;
            m_width[i] = 1;
        }

        m_input = m_ioMgr.AddInput("input", true);
        m_input->m_scale = 0.2;
        
        m_saturationInputGain = m_ioMgr.AddInput("Saturationt Input Gain", true);
        m_saturationInputGain->m_scale = 5.0 / 10;
        m_saturationInputGain->m_offset = 0.5;
        
        m_bwBase = m_ioMgr.AddInput("Base", true);
        m_bwWidth = m_ioMgr.AddInput("Width", true);
        m_bwWidth->m_scale = 11.0 / 10;
        
        m_output = m_ioMgr.AddOutput("Output", true);
        m_output->m_scale = 5;

        for (size_t i = 0; i < 16; ++i)
        {
            m_input->SetTarget(i, &m_inputValue[i]);
            m_saturationInputGain->SetTarget(i, &m_saturator[i].m_inputGain);
            m_bwBase->SetTarget(i, &m_base[i]);
            m_bwWidth->SetTarget(i, &m_width[i]);
            m_output->SetSource(i, &m_outputValue[i]);
        }

        m_ioMgr.Config();
    }

    void SetBaseWidth()
    {
        for (int i = 0; i < m_input->m_value.m_channels; ++i)
        {
            m_baseWidthFilter[i].SetBaseWidth(
                powf(2, m_base[i]) / 2048,
                powf(2, m_width[i]));
        }
    }

    void process(const ProcessArgs& args) override
    {
        m_ioMgr.Process();
        SetBaseWidth();

        m_output->SetChannels(m_input->m_value.m_channels);
        for (int i = 0; i < m_input->m_value.m_channels; ++i)
        {
            m_outputValue[i] = m_baseWidthFilter[i].Process(m_saturator[i].Process(m_inputValue[i]));
        }

        m_ioMgr.SetOutputs();
    }
};

struct SaturationBlockWidget : ModuleWidget
{
    SaturationBlockWidget(SaturationBlock* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SaturationBlock.svg")));
        
        if (module)
        {
            module->m_input->Widget(this, 1, 1);
            module->m_saturationInputGain->Widget(this, 1, 2);
            module->m_bwBase->Widget(this, 1, 3);
            module->m_bwWidth->Widget(this, 1, 4);
            module->m_output->Widget(this, 2, 1);
        }
    }
};
