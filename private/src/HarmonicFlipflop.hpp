#include "ModuleUtils.hpp"

#ifndef IOS_BUILD
struct HarmonicFlipflop : Module
{
    float m_log2Harmonics[12];
    float m_input[16][2];
    float m_output[16][2];
    
    IOMgr m_ioMgr;
    IOMgr::Input* m_inputIO[2];
    IOMgr::Output* m_outputIO[2];
    
    HarmonicFlipflop()
        : m_ioMgr(this)
    {
        m_inputIO[0] = m_ioMgr.AddInput("Input 1", true);
        m_inputIO[0]->m_scale = 0.1;
        m_inputIO[1] = m_ioMgr.AddInput("Input 2", true);
        m_inputIO[1]->m_scale = 0.1;
        m_outputIO[0] = m_ioMgr.AddOutput("Output 1", true);
        m_outputIO[1] = m_ioMgr.AddOutput("Output 2", true);

        m_log2Harmonics[0] = log2(0.25);
        m_log2Harmonics[1] = log2(0.5);
        m_log2Harmonics[2] = log2(1.0);
        m_log2Harmonics[3] = log2(1.5);

        m_log2Harmonics[4] = log2(2.0);
        m_log2Harmonics[5] = log2(2.5);
        m_log2Harmonics[6] = log2(3.0);
        m_log2Harmonics[7] = log2(4.0);

        m_log2Harmonics[8] = log2(5.0);
        m_log2Harmonics[9] = log2(6.0);
        m_log2Harmonics[10] = log2(7.0);
        m_log2Harmonics[11] = log2(8.0);
        

        for (size_t i = 0; i < 16; ++i)
        {
            for (size_t j = 0; j < 2; ++j)
            {
                m_input[i][j] = 0.0;
                m_output[i][j] = 0.0;
                m_inputIO[j]->SetTarget(i, &m_input[i][j]);
                m_outputIO[j]->SetSource(i, &m_output[i][j]);
            }
        }

        m_ioMgr.Config();
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        m_outputIO[0]->SetChannels(m_inputIO[0]->m_value.m_channels);
        m_outputIO[1]->SetChannels(m_inputIO[1]->m_value.m_channels);
        for (size_t j = 0; j < 2; ++j)
        {
            for (int i = 0; i < m_inputIO[j]->m_value.m_channels; ++i)
            {
                int index = std::min(11, std::max(0, (int)std::round(m_input[i][j] * 11)));
                m_output[i][j] = m_log2Harmonics[index];
            }
        }

        m_ioMgr.SetOutputs();
    }
};

struct HarmonicFlipflopWidget : public ModuleWidget
{
    HarmonicFlipflopWidget(HarmonicFlipflop* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/HarmonicFlipflop.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        if (module)
        {
            module->m_inputIO[0]->Widget(this, 1, 3);
            module->m_inputIO[1]->Widget(this, 1, 5);
            module->m_outputIO[0]->Widget(this, 3, 3);
            module->m_outputIO[1]->Widget(this, 3, 5);
        }
    }
};
#endif
