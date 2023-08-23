#include "LameJuis.hpp"

struct LameJuisWidget : ModuleWidget
{
    static constexpr float x_hp = 5.08;

    static constexpr float x_jackLightOffsetHP = 1.0;
    static constexpr float x_jackStartYHP = 4.25;
    static constexpr float x_jackSpacingXHP = 2.5;
    static constexpr float x_jackSpacingYHP = 3.0;

    static constexpr float x_firstMatrixSwitchXHP = 13.25;
    static constexpr float x_firstMatrixSwitchYHP = 3.35;
    static constexpr float x_switchSpacingXHP = 1.5;
    static constexpr float x_rowYSpacing = 3.5;

    static constexpr float x_operationKnobXHP = 23.5;
    static constexpr float x_firstOperatorKnobYHP = 3.15;

    static constexpr float x_operationSwitchXHP = 25.75;

    static constexpr float x_firstKnobMatrixXHP = 30.375;
    static constexpr float x_firstKnobMatrixYHP = 2.825;
    static constexpr float x_knobMatrixSpacingYHP = 4.0;

    static constexpr float x_secondKnobMatrixXHP = 35.25;
    static constexpr float x_secondKnobMatrixYHP = 4.0;
    
    static constexpr float x_firstCoMuteXHP = 29.5;
    static constexpr float x_firstCoMuteYHP = 15.75;

	struct RandomizeParamsItem : MenuItem
    {
        enum class ParamGroup
        {
            Matrix = 0,
            Intervals = 1,
            CoMutes = 2,
            Percentiles = 3,
            All = 4,
            NumGroups = 5
        };

        static const char* ParamGroupToString(ParamGroup pg)
        {
            switch (pg)
            {
                case ParamGroup::Matrix: return "Matrix";
                case ParamGroup::Intervals: return "Intervals";
                case ParamGroup::CoMutes: return "CoMutes";
                case ParamGroup::Percentiles: return "Percentiles";
                case ParamGroup::All: return "All";
                default: return nullptr;                    
            }
        }

        int m_level;
        ParamGroup m_group;
		LameJuis* m_module;

        RandomizeParamsItem(ParamGroup group, int level, LameJuis* module)
            : m_level(level)
            , m_group(group)
            , m_module(module)
        {
            if (m_group == ParamGroup::Percentiles)
            {
                text = "Randomize Percentiles";
            }
            else
            {
                text = "Level " + std::to_string(m_level);
            }
        }

        ~RandomizeParamsItem()
        {
        }
        
		void onAction(const event::Action &e) override
        {
            switch (m_group)
            {
                case ParamGroup::Matrix:
                    m_module->RandomizeMatrix(m_level);
                    break;
                case ParamGroup::Intervals:
                    m_module->RandomizeIntervals(m_level);
                    break;
                case ParamGroup::CoMutes:
                    m_module->RandomizeCoMutes(m_level);
                    break;
                case ParamGroup::Percentiles:
                    m_module->RandomizePercentiles();
                    break;
                case ParamGroup::All:
                    m_module->RandomizeMatrix(m_level);
                    m_module->RandomizeIntervals(m_level);
                    m_module->RandomizePercentiles();
                    m_module->RandomizeCoMutes(m_level);
                    break;
                default:
                    break;
            }
		}		
	};
    
    Vec GetInputJackMM(size_t inputId)
    {
        return Vec(x_hp + 2.5,
                   x_hp * (x_jackStartYHP + inputId * x_jackSpacingYHP));
    }

    Vec JackToLight(Vec jackPos)
    {
        return jackPos.plus(Vec(x_hp * x_jackLightOffsetHP, - x_hp * x_jackLightOffsetHP));
    }

    Vec GetOperationOutputJackMM(size_t operationId)
    {
        return GetInputJackMM(operationId).plus(Vec(x_hp * x_jackSpacingXHP, 0));
    }

    Vec GetMainOutputJackMM(size_t accumulatorId)
    {
        return GetInputJackMM(2 * accumulatorId).plus(Vec(2 * x_hp * x_jackSpacingXHP, 0));
    }

    Vec GetTriggerOutputJackMM(size_t accumulatorId)
    {
        return GetInputJackMM(2 * accumulatorId + 1).plus(Vec(2 * x_hp * x_jackSpacingXHP, 0));
    }

    Vec GetPitchPercentileJackMM(size_t accumulatorId)
    {
        return GetInputJackMM(accumulatorId).plus(Vec(3 * x_hp * x_jackSpacingXHP, 0));
    }

    Vec GetIntervalInputJackMM(size_t accumulatorId)
    {
        return GetInputJackMM(accumulatorId + 3).plus(Vec(3 * x_hp * x_jackSpacingXHP, 0));
    }
    
    Vec GetMatrixSwitchMM(size_t inputId, size_t operationId)
    {
        return Vec(x_hp * (x_firstMatrixSwitchXHP + inputId * x_switchSpacingXHP),
                   x_hp * (x_firstMatrixSwitchYHP + operationId * x_rowYSpacing));
    }

    Vec GetOperatorKnobMM(size_t operationId)
    {
        return Vec(x_hp * x_operationKnobXHP,
                   x_hp * (x_firstOperatorKnobYHP + operationId * x_rowYSpacing));
    }

    Vec GetOperationSwitchMM(size_t operationId)
    {
        return Vec(x_hp * x_operationSwitchXHP,
                   x_hp * (x_firstMatrixSwitchYHP + operationId * x_rowYSpacing));
    }

    Vec GetIntervalKnobMM(size_t accumulatorId)
    {
        return Vec(x_hp * x_firstKnobMatrixXHP,
                   x_hp * (x_firstKnobMatrixYHP + accumulatorId * x_knobMatrixSpacingYHP));
    }

    Vec GetPercentileKnobMM(size_t accumulatorId)
    {
        return Vec(x_hp * x_secondKnobMatrixXHP,
                   x_hp * (x_secondKnobMatrixYHP + accumulatorId * x_knobMatrixSpacingYHP));
    }

    Vec GetCoMuteSwitchMM(size_t inputId, size_t accumulatorId)
    {
        return Vec(x_hp * (x_firstCoMuteXHP + inputId * x_switchSpacingXHP),
                   x_hp * (x_firstCoMuteYHP + accumulatorId * x_rowYSpacing));
    }

	LameJuisWidget(LameJuis* module)
    {
        using namespace LameJuisConstants;   

		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LameJuis.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (size_t i = 0; i < x_numInputs; ++i)
        {
            addInput(createInputCentered<PJ301MPort>(
                         mm2px(GetInputJackMM(i)),
                         module,
                         GetMainInputId(i)));

             addChild(createLightCentered<MediumLight<RedLight>>(
                          mm2px(JackToLight(GetInputJackMM(i))),
                          module,
                          GetInputLightId(i)));

            for (size_t j = 0; j < x_numOperations; ++j)
            {
                addParam(createParamCentered<NKK>(
                             mm2px(GetMatrixSwitchMM(i, j)),
                             module,
                             GetMatrixSwitchId(i, j)));
            }

            for (size_t j = 0; j < x_numAccumulators; ++j)
            {
                addParam(createParamCentered<NKK>(
                             mm2px(GetCoMuteSwitchMM(i, j)),
                             module,
                             GetPitchCoMuteSwitchId(i, j)));                
            }
        }

        for (size_t i = 0; i < x_numOperations; ++i)
        {
            addParam(createParamCentered<RoundBlackSnapKnob>(
                         mm2px(GetOperatorKnobMM(i)),
                         module,
                         GetOperatorKnobId(i)));
            addParam(createParamCentered<NKK>(
                         mm2px(GetOperationSwitchMM(i)),
                         module,
                         GetOperationSwitchId(i)));
            addOutput(createOutputCentered<PJ301MPort>(
                          mm2px(GetOperationOutputJackMM(i)),
                          module,
                          GetOperationOutputId(i)));            
            addChild(createLightCentered<MediumLight<RedLight>>(
                         mm2px(JackToLight(GetOperationOutputJackMM(i))),
                         module,
                         GetOperationLightId(i)));
        }

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            addOutput(createOutputCentered<PJ301MPort>(
                          mm2px(GetMainOutputJackMM(i)),
                          module,
                          GetMainOutputId(i)));
            addOutput(createOutputCentered<PJ301MPort>(
                          mm2px(GetTriggerOutputJackMM(i)),
                          module,
                          GetTriggerOutputId(i)));
            addChild(createLightCentered<MediumLight<RedLight>>(
                         mm2px(JackToLight(GetTriggerOutputJackMM(i))),
                         module,
                         GetTriggerLightId(i)));

            addInput(createInputCentered<PJ301MPort>(
                         mm2px(GetIntervalInputJackMM(i)),
                         module,
                         GetIntervalCVInputId(i)));
            addInput(createInputCentered<PJ301MPort>(
                          mm2px(GetPitchPercentileJackMM(i)),
                          module,
                          GetPitchPercentileCVInputId(i)));
            
            addParam(createParamCentered<RoundBlackSnapKnob>(
                         mm2px(GetIntervalKnobMM(i)),
                         module,
                         GetAccumulatorIntervalKnobId(i)));
            addParam(createParamCentered<RoundBlackKnob>(
                         mm2px(GetPercentileKnobMM(i)),
                         module,
                         GetPitchPercentileKnobId(i)));

        }
	}

    void appendContextMenu(Menu *menu) override
    {
        LameJuis* lameJuis = dynamic_cast<LameJuis*>(module);
        
        MenuLabel* randomLabel = new MenuLabel();
		randomLabel->text = "Randomize";
		menu->addChild(randomLabel);

        menu->addChild(new MenuSeparator);
        
        for (int i = 0; i < static_cast<int>(RandomizeParamsItem::ParamGroup::NumGroups); ++i)
        {
            RandomizeParamsItem::ParamGroup group = static_cast<RandomizeParamsItem::ParamGroup>(i);
            if (group == RandomizeParamsItem::ParamGroup::Percentiles)
            {
                RandomizeParamsItem* randomizeStackMuteItem = new RandomizeParamsItem(group, 0 /*level*/, lameJuis);
                menu->addChild(randomizeStackMuteItem);
            }
            else
            {
                auto createSubmenu = [group, lameJuis] (ui::Menu* subMenu)
                {
                    for (size_t level = 0; level < 3; ++level)
                    {                    
                        RandomizeParamsItem* randomizeStackMuteItem = new RandomizeParamsItem(group, level, lameJuis);
                        subMenu->addChild(randomizeStackMuteItem);
                    }
                };

                menu->addChild(createSubmenuItem(
                                   std::string("Randomize ") + RandomizeParamsItem::ParamGroupToString(group),
                                   "",
                                   createSubmenu));
            }
        }

        menu->addChild(new MenuSeparator);

		menu->addChild(createBoolMenuItem(
                           "12 EDO Mode",
                           "",
                           [lameJuis]() { return lameJuis->m_12EDOMode; },
                           [lameJuis](bool newVal) { lameJuis->m_12EDOMode = newVal; }));

		menu->addChild(createBoolMenuItem(
                           "Time Quantize Mode",
                           "",
                           [lameJuis]() { return lameJuis->m_timeQuantizeMode; },
                           [lameJuis](bool newVal) { lameJuis->m_timeQuantizeMode = newVal; }));
    }
};

Model* modelLameJuis = createModel<LameJuis, LameJuisWidget>("LameJuis");
