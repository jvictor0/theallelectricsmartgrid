#include "LameJuis.hpp"

struct LameJuisWidget : ModuleWidget
{
    static constexpr float x_reset_x = 94.3505;
    static constexpr float x_reset_y = 111.1385;
    static constexpr float x_clock_x = 42.3505;
    static constexpr float x_intervalCV_x = 28.7505;
    static constexpr float x_intervalKnob_x = 76.935;
    static constexpr float x_interval_y[] = 
    {
        178.8435,
        251.6045,
        324.3655
    };

    static constexpr float x_input_x = 172.3505;

    static constexpr float x_s_x[] = 
    {
        210.7505, 
        250.7505,
        290.7505,
        330.7505,
        370.7505, 
        410.7505
    };

    static constexpr float x_v_x[] = 
    {
        481.6295,
        521.6295,
        561.6295
    };

    static constexpr float x_a_y[] = 
    {
        56.2495,
        89.2095,
        122.1695,
        155.1295,
        188.0895,
        221.0495
    };

    static constexpr float x_pitchSelect_y = 258.338;
    static constexpr float x_pitchCV_y = 292.8555;
    static constexpr float x_logicOut_y = 322.6875;
    static constexpr float x_voltOct_y = 352.9875;

    static constexpr float x_lightOffset = 18;
    static constexpr float x_cornerLightOffset = 12;
    
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
    
    Vec GetResetJackPX()
    {
        return Vec(x_reset_x, x_reset_y);
    }

    Vec GetClockJackPX()
    {
        return Vec(x_clock_x, x_reset_y);
    }

    Vec GetInputJackPX(size_t inputId)
    {
        return Vec(x_input_x, x_a_y[inputId]);
    }

    Vec JackToLightRight(Vec jackPos)
    {
        return jackPos.plus(Vec(x_lightOffset, 0));
    }
    
    Vec JackToLightDown(Vec jackPos)
    {
        return jackPos.plus(Vec(0, x_lightOffset));
    }

    Vec JackToLightDownRight(Vec jackPos)
    {
        return jackPos.plus(Vec(x_cornerLightOffset, x_cornerLightOffset));
    }
            
    Vec GetOperationOutputJackPX(size_t operationId)
    {
        return Vec(x_s_x[operationId], x_logicOut_y);
    }

    Vec GetMainOutputJackPX(size_t accumulatorId)
    {
        return Vec(x_v_x[accumulatorId], x_voltOct_y);
    }

    Vec GetTriggerOutputJackPX(size_t accumulatorId)
    {
        return Vec(x_v_x[accumulatorId], x_logicOut_y);
    }

    Vec GetPitchPercentileJackPX(size_t accumulatorId)
    {
        return Vec(x_v_x[accumulatorId], x_pitchCV_y);
    }

    Vec GetIntervalInputJackPX(size_t accumulatorId)
    {
        return Vec(x_intervalCV_x, x_interval_y[accumulatorId]);
    }
    
    Vec GetMatrixSwitchPX(size_t inputId, size_t operationId)
    {
        return Vec(x_s_x[operationId], x_a_y[inputId]);
    }

    Vec GetOperatorKnobPX(size_t operationId)
    {
        return Vec(x_s_x[operationId], x_pitchSelect_y);
    }

    Vec GetOperationSwitchPX(size_t operationId)
    {
        return Vec(x_s_x[operationId], x_pitchCV_y);
    }

    Vec GetIntervalKnobPX(size_t accumulatorId)
    {
        return Vec(x_intervalKnob_x, x_interval_y[accumulatorId]);
    }

    Vec GetPercentileKnobPX(size_t accumulatorId)
    {
        return Vec(x_v_x[accumulatorId], x_pitchSelect_y);
    }

    Vec GetCoMuteSwitchPX(size_t inputId, size_t accumulatorId)
    {
        return Vec(x_v_x[accumulatorId], x_a_y[inputId]);
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
                         GetInputJackPX(i),
                         module,
                         GetMainInputId(i)));

             addChild(createLightCentered<MediumLight<RedLight>>(
                          JackToLightRight(GetInputJackPX(i)),
                          module,
                          GetInputLightId(i)));

            for (size_t j = 0; j < x_numOperations; ++j)
            {
                addParam(createParamCentered<BefacoSwitch>(
                             GetMatrixSwitchPX(i, j),
                             module,
                             GetMatrixSwitchId(i, j)));
            }

            for (size_t j = 0; j < x_numAccumulators; ++j)
            {
                addParam(createLightParamCentered<VCVLightBezelLatch<MediumSimpleLight<WhiteLight>>>(
                             GetCoMuteSwitchPX(i, j),
                             module,
                             GetPitchCoMuteSwitchId(i, j),
                             GetCoMuteLightId(i, j)));                
            }
        }

        for (size_t i = 0; i < x_numOperations; ++i)
        {
            addParam(createParamCentered<Trimpot>(
                         GetOperatorKnobPX(i),
                         module,
                         GetOperatorKnobId(i)));
            addParam(createParamCentered<BefacoSwitch>(
                         GetOperationSwitchPX(i),
                         module,
                         GetOperationSwitchId(i)));
            addOutput(createOutputCentered<PJ301MPort>(
                          GetOperationOutputJackPX(i),
                          module,
                          GetOperationOutputId(i)));            
            addChild(createLightCentered<MediumLight<RedLight>>(
                         JackToLightDown(GetOperationOutputJackPX(i)),
                         module,
                         GetOperationLightId(i)));
        }

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            addOutput(createOutputCentered<PJ301MPort>(
                          GetMainOutputJackPX(i),
                          module,
                          GetMainOutputId(i)));
            addOutput(createOutputCentered<PJ301MPort>(
                          GetTriggerOutputJackPX(i),
                          module,
                          GetTriggerOutputId(i)));
            addChild(createLightCentered<MediumLight<RedLight>>(
                         JackToLightDownRight(GetTriggerOutputJackPX(i)),
                         module,
                         GetTriggerLightId(i)));

            addInput(createInputCentered<PJ301MPort>(
                         GetIntervalInputJackPX(i),
                         module,
                         GetIntervalCVInputId(i)));
            addInput(createInputCentered<PJ301MPort>(
                          GetPitchPercentileJackPX(i),
                          module,
                          GetPitchPercentileCVInputId(i)));
            
            addParam(createParamCentered<RoundBigBlackKnob>(
                         GetIntervalKnobPX(i),
                         module,
                         GetAccumulatorIntervalKnobId(i)));
            addParam(createParamCentered<RoundBlackKnob>(
                         GetPercentileKnobPX(i),
                         module,
                         GetPitchPercentileKnobId(i)));

        }

        addInput(createInputCentered<PJ301MPort>(
                     GetResetJackPX(),
                     module,
                     GetResetInputId()));
        addInput(createInputCentered<PJ301MPort>(
                     GetClockJackPX(),
                     module,
                     GetClockInputId()));
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

constexpr float LameJuisWidget::x_interval_y[];
constexpr float LameJuisWidget::x_s_x[];
constexpr float LameJuisWidget::x_v_x[];
constexpr float LameJuisWidget::x_a_y[];

Model* modelLameJuis = createModel<LameJuis, LameJuisWidget>("LameJuis");
