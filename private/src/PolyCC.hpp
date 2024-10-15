#pragma once
#include "plugin.hpp"
#include "PeriodChecker.hpp"

struct PolyCC : public Module
{
    midi::Output m_output;

    StateSaver m_stateSaver;

    uint8_t m_lastValue[16][16];

    PeriodChecker m_checker;
    
    PolyCC()
    {        
        config(0, 16, 0, 0);
        
        for (size_t i = 0; i < 16; ++i)
        {
            for (size_t j = 0; j < 16; ++j)
            {
                m_lastValue[i][j] = 255;
            }
        }
    }

    void process(const ProcessArgs& args) override
    {
        if (!m_checker.Process(args.sampleTime))
        {
            return;
        }
        
        for (size_t i = 0; i < 16; ++i)
        {
            if (inputs[i].isConnected())
            {
                for (size_t j = 0; j < static_cast<size_t>(inputs[i].getChannels()); ++j)
                {
                    midi::Message msg;

                    uint8_t value = std::max<float>(0, std::min<float>(127, inputs[i].getVoltage(j) * 128 / 10));
                    if (m_lastValue[i][j] == value)
                    {
                        continue;
                    }
                    
                    m_lastValue[i][j] = value;

                    msg.setStatus(0xb);
                    msg.setNote(i);
                    msg.setValue(value);
                    msg.setFrame(args.frame);
                    m_output.setChannel(j);
                    m_output.sendMessage(msg);                                    
                }
            }
        }

        m_output.setChannel(0);
    }

    json_t* dataToJson() override
    {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "midiOut", m_output.toJson());
        json_object_set_new(rootJ, "state", m_stateSaver.ToJSON());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
    {
        json_t* midiJ = json_object_get(rootJ, "midiOut");
		if (midiJ)
        {
			m_output.fromJson(midiJ);
        }

        midiJ = json_object_get(rootJ, "state");
        if (midiJ)
        {
            m_stateSaver.SetFromJSON(midiJ);
        }
	}
};

struct PolyCCWidget : public ModuleWidget
{
    PolyCCWidget(PolyCC* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PolyCV_MIDICC.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SmartGrid::MidiWidget* midiOutputWidget = createWidget<SmartGrid::MidiWidget>(Vec(10.0f, 107.4f));
		midiOutputWidget->box.size = Vec(130.0f, 67.0f);
		midiOutputWidget->SetMidiPort(module ? &module->m_output : NULL, true);
		addChild(midiOutputWidget);

        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {                        
                addInput(createInputCentered<ThemedPJ301MPort>(Vec(10 + 25 * i, 200 + 25 * j), module, i + 4 * j));
            }
        }
    }
};
