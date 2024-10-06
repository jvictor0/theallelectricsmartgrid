

struct SampleRateDivider : public Module
{
    static constexpr size_t x_maxPoly = 16;
    size_t m_counter;
    size_t m_divider[x_maxPoly];

    SampleRateDivider()
    {
        config(0, 2, 1, 0);

        configInput(0, "Input");
        configInput(1, "Divider");
        configOutput(0, "Output");
        
        m_counter = 0;
        for (size_t i = 0; i < x_maxPoly; i++)
        {
            m_divider[i] = 1;
        }
    }

    void process(const ProcessArgs &args) override
    {
        if (m_counter % 100 == 0)
        {
            for (int i = 0; i < inputs[0].getChannels(); i++)
            {
                m_divider[i] = 1.59 * inputs[1].getVoltage(i % inputs[1].getChannels()) + 1;
            }

            outputs[0].setChannels(inputs[0].getChannels());
        }   
        
        m_counter++;

        for (int i = 0; i < inputs[0].getChannels(); i++)
        {
            if (m_counter % m_divider[i] == 0)
            {
                outputs[0].setVoltage(inputs[0].getVoltage(i), i);
            }
        }
    }
};

struct SampleRateDividerWidget : ModuleWidget
{
    SampleRateDividerWidget(SampleRateDivider *module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SampleRateDivider.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInput<PJ301MPort>(Vec(10, 30), module, 0));
        addInput(createInput<PJ301MPort>(Vec(10, 60), module, 1));
        addOutput(createOutput<PJ301MPort>(Vec(10, 90), module, 0));
    }
};
