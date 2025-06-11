#pragma once
#include "ModuleUtils.hpp"
#include "Slew.hpp"
#include "WaveTable.hpp"

struct LissajousLFOInternal
{    
    struct Input
    {
        const WaveTable* m_sinTable;
        const WaveTable* m_cosTable;

        float m_multX;
        float m_multY;

        float m_centerX;
        float m_centerY;

        float m_phaseShift;

        float m_radius;

        Input()
            : m_multX(1.0f)
            , m_multY(1.0f)
            , m_centerX(0)
            , m_centerY(0)
            , m_phaseShift(0.0f)
            , m_radius(1.0f)
        {
            m_cosTable = &WaveTable::GetCosine();
            m_sinTable = &WaveTable::GetSine();
        }

        std::pair<float, float> Compute(float t)
        {
            float ampX = 1;
            float ampY = 1;
            t += m_phaseShift;
            float tx = (t - std::floorf(t)) * m_multX;
            float ty = (t - std::floorf(t)) * m_multY;
            float floorMultX = std::floorf(m_multX);
            float floorMultY = std::floorf(m_multY);

            if (floorMultX < tx)
            {
                ampX = m_multX - floorMultX;
                tx = (tx - floorMultX) / ampX;
            }
            else
            {
                tx = tx - std::floorf(tx);
            }

            if (floorMultY < ty)
            {
                ampY = m_multY - floorMultY;
                ty = (ty - floorMultY) / ampY;
            }
            else
            {
                ty = ty - std::floorf(ty);
            }

            return std::make_pair(
                m_radius * ampX * m_cosTable->Evaluate(tx) + (1 - m_radius) * m_centerX,
                m_radius * ampY * m_sinTable->Evaluate(ty) + (1 - m_radius) * m_centerY);
        }
    };

    LissajousLFOInternal()
        : m_outputX(0.0f)
        , m_outputY(0.0f)
    {
    }
    
    float m_outputX;
    float m_outputY;

    void Process(float in, Input& input)
    {
        std::pair<float, float> out = input.Compute(in);
        m_outputX = out.first;
        m_outputY = out.second;
    }
};
            
#ifndef IOS_BUILD
struct LissajousLFO : Module
{
    LissajousLFOInternal m_lfo[16];
    LissajousLFOInternal::Input m_state[16];

    IOMgr m_ioMgr;
    IOMgr::Input* m_input;
    IOMgr::Input* m_phaseShift;
    IOMgr::Input* m_multX;
    IOMgr::Input* m_multY;
    IOMgr::Input* m_centerX;
    IOMgr::Input* m_centerY;
    IOMgr::Input* m_radius;
    IOMgr::Output* m_outputX;
    IOMgr::Output* m_outputY;

    float m_inputVal;

    LissajousLFO()
        : m_ioMgr(this)
    {
        m_input = m_ioMgr.AddInput("Input", true);
        m_input->m_scale = 0.1;

        m_phaseShift = m_ioMgr.AddInput("Phase Shift", true);
        m_phaseShift->m_scale = 0.1;

        m_multX = m_ioMgr.AddInput("Mult X", true);
        m_multX->m_scale = 0.5;
        m_multX->m_offset = 1;

        m_multY = m_ioMgr.AddInput("Mult Y", true);
        m_multY->m_scale = 0.5;
        m_multY->m_offset = 1;

        m_centerX = m_ioMgr.AddInput("Center X", true);
        m_centerX->m_scale = 0.2;
        m_centerX->m_offset = -1;

        m_centerY = m_ioMgr.AddInput("Center Y", true);
        m_centerY->m_scale = 0.2;
        m_centerY->m_offset = -1;

        m_radius = m_ioMgr.AddInput("Radius", true);
        m_radius->m_scale = 0.1;        

        m_outputX = m_ioMgr.AddOutput("Output X", true);
        m_outputX->m_scale = 5;
        m_outputX->m_offset = 5;

        m_outputY = m_ioMgr.AddOutput("Output Y", true);
        m_outputY->m_scale = 5;
        m_outputY->m_offset = 5;

        m_input->SetTarget(0, &m_inputVal);

        for (size_t i = 0; i < 16; ++i)
        {
            m_phaseShift->SetTarget(i, &m_state[i].m_phaseShift);
            m_multX->SetTarget(i, &m_state[i].m_multX);
            m_multY->SetTarget(i, &m_state[i].m_multY);
            m_centerX->SetTarget(i, &m_state[i].m_centerX);
            m_centerY->SetTarget(i, &m_state[i].m_centerY);
            m_radius->SetTarget(i, &m_state[i].m_radius);
            
            m_outputX->SetSource(i, &m_lfo[i].m_outputX);
            m_outputY->SetSource(i, &m_lfo[i].m_outputY);
        }

        m_ioMgr.Config();
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        m_outputX->SetChannels(m_phaseShift->m_value.m_channels);
        m_outputY->SetChannels(m_phaseShift->m_value.m_channels);
        for (int i = 0; i < m_phaseShift->m_value.m_channels; ++i)
        {
            m_lfo[i].Process(m_inputVal, m_state[i]);
        }

        m_ioMgr.SetOutputs();
    }
};

struct LissajousLFOWidget : public ModuleWidget
{
    LissajousLFOWidget(LissajousLFO* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LissajousLFO.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        if (module)
        {
            module->m_input->Widget(this, 1, 1);
            module->m_phaseShift->Widget(this, 2, 1);
            module->m_multX->Widget(this, 1, 2);
            module->m_multY->Widget(this, 2, 2);
            module->m_centerX->Widget(this, 1, 3);
            module->m_centerY->Widget(this, 2, 3);
            module->m_radius->Widget(this, 1, 4);
            module->m_outputX->Widget(this, 1, 6);
            module->m_outputY->Widget(this, 2, 6);
        }
    }
};
#endif
