#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>

struct MultiFunctionKnob : Module
{
    MultiFunctionKnob()
    {
        config(4, 0, 3, 0);
	m_counter = 0;
	m_sinceChange = 0;
	m_wasZero = true;
	m_prevCurIx = -1;

	configParam(0, 0.f, 1.f, 0.f, "");
	configParam(1, 1.f, 16.f, 0.f, "");
	configParam(2, 0.f, 127.f, 0.f, "");

	configOutput(0, "");
	configOutput(1, "");
	configOutput(2, "");
    }

    float m_values[16];
    int m_counter;
    int m_prevCurIx;
    bool m_wasZero;
    int m_sinceChange;
  
    void process(const ProcessArgs &args) override
    {
        if (m_counter == 0)
        {
    	    m_counter = 100;
	    m_sinceChange++;
	    int numChans = params[1].getValue();
	    int curIx = params[0].getValue() * numChans;
	    curIx = std::min(curIx, numChans - 1);
	    curIx = std::max(curIx, 0);

	    if (curIx != m_prevCurIx)
	    {
	        m_prevCurIx = curIx;
	    }

	    float val = params[2].getValue();
	    if (val == 0 && !m_wasZero)
	    {
	        m_sinceChange = 0;
  	        m_wasZero = true;
	    }
	    else if (val > 0 && m_wasZero)
	    {
  	        m_wasZero = false;
		m_sinceChange = 0;
  	        if (val != m_values[curIx])
		{
  	            m_values[curIx] = val;
		}
		else
		{
 		    m_values[curIx] = 0;
		}
	    }

	    outputs[0].setChannels(numChans);
	    outputs[1].setChannels(numChans);
	    for (int i = 0; i < numChans; ++i)
	    {
  	        outputs[0].setVoltage(m_values[i] / 12.7, i);
		outputs[1].setVoltage(i == curIx ? 10 : 0, i);
	    }

	    if (m_sinceChange > 5 && m_sinceChange < 10)
	    {
 	        outputs[2].setVoltage(m_values[curIx] > 0 ? 0 : 10);
	    }
	    else
	    {
	        outputs[2].setVoltage(m_values[curIx] / 12.7);
	    }
	}
	else
	{
    	    --m_counter;
	}
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();
	for (int i = 0; i < 16; ++i)
	{
  	    json_object_set_new(rootJ, ("value" + std::to_string(i)).c_str(), json_real(m_values[i]));
	}

	return rootJ;
    }

    void dataFromJson(json_t* rootJ) override
    {
      	for (int i = 0; i < 16; ++i)
	{
  	    json_t* val = json_object_get(rootJ, ("value" + std::to_string(i)).c_str());
	    if (val)
	    {
	        m_values[i] = json_number_value(val);
	    }
	}
    }
};

struct MultiFunctionKnobWidget : ModuleWidget
{
    MultiFunctionKnobWidget(MultiFunctionKnob* module)
    {
	setModule(module);
	setPanel(createPanel(asset::plugin(pluginInstance, "res/MultiFunctionKnob.svg")));

	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	addParam(createParamCentered<Trimpot>(Vec(100, 100), module, 0));
	addParam(createParamCentered<RoundBigBlackKnob>(Vec(100, 150), module, 1));
	addParam(createParamCentered<RoundBigBlackKnob>(Vec(100, 200), module, 2));

	addOutput(createOutputCentered<PJ301MPort>(Vec(200, 100), module, 0));
	addOutput(createOutputCentered<PJ301MPort>(Vec(200, 150), module, 1));
	addOutput(createOutputCentered<PJ301MPort>(Vec(200, 200), module, 2));

    }
};

