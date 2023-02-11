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
};

Model* modelLameJuis = createModel<LameJuis, LameJuisWidget>("LameJuis");
