#pragma once

namespace LameJuisConstants
{
    static constexpr size_t x_maxPoly = 16;

    enum class ParamType : int
    {
        MatrixSwitch = 0,
        OperationSwitch = 1,
        OperatorKnob = 2,
        AccumulatorIntervalKnob = 3,
        PitchCoMuteSwitch = 4,
        PitchPercentileKnob = 5,
        NumParamTypes = 6
    };

    static constexpr size_t x_numInputs = 6;
    static constexpr size_t x_numOperations = 6;
    static constexpr size_t x_numAccumulators = 3;

    static constexpr size_t x_numParamsPerType[] =
    {
        x_numInputs * x_numOperations /*MatrixSwitch*/,
        x_numOperations /*OperationSwitch*/,
        x_numOperations /*OperatorKnob*/,
        x_numAccumulators /*AccumulatorIntervalKnob*/,
        x_numInputs * x_numAccumulators /*PitchCoMuteSwitch*/,
        x_numAccumulators /*PitchPercentileKnob*/,
    };

    static constexpr size_t x_paramStartPerType[] = {
        0,
        x_numParamsPerType[0],
        x_numParamsPerType[0] + x_numParamsPerType[1],
        x_numParamsPerType[0] + x_numParamsPerType[1] + x_numParamsPerType[2],
        x_numParamsPerType[0] + x_numParamsPerType[1] + x_numParamsPerType[2] + x_numParamsPerType[3],
        x_numParamsPerType[0] + x_numParamsPerType[1] + x_numParamsPerType[2] + x_numParamsPerType[3] + x_numParamsPerType[4],
        x_numParamsPerType[0] + x_numParamsPerType[1] + x_numParamsPerType[2] + x_numParamsPerType[3] + x_numParamsPerType[4] + x_numParamsPerType[5],
     }; 

    static constexpr size_t GetParamId(ParamType paramType, size_t paramId)
    {
        return x_paramStartPerType[static_cast<int>(paramType)] + paramId;
    }

    static constexpr size_t GetMatrixSwitchId(size_t inputId, size_t operationId)
    {
        return GetParamId(ParamType::MatrixSwitch, inputId + operationId * x_numInputs);
    }
    
    static constexpr size_t GetOperationSwitchId(size_t operationId)
    {
        return GetParamId(ParamType::OperationSwitch, operationId);
    }

    static constexpr size_t GetOperatorKnobId(size_t operationId)
    {
        return GetParamId(ParamType::OperatorKnob, operationId);
    }

    static constexpr size_t GetAccumulatorIntervalKnobId(size_t accumulatorId)
    {
        return GetParamId(ParamType::AccumulatorIntervalKnob, accumulatorId);
    }

    static constexpr size_t GetPitchCoMuteSwitchId(size_t inputId, size_t accumulatorId)
    {
        return GetParamId(ParamType::PitchCoMuteSwitch, inputId + accumulatorId * x_numInputs);
    }

    static constexpr size_t GetPitchPercentileKnobId(size_t accumulatorId)
    {
        return GetParamId(ParamType::PitchPercentileKnob, accumulatorId);
    }

    static constexpr size_t GetNumParams()
    {
        return x_paramStartPerType[static_cast<size_t>(ParamType::NumParamTypes)];
    }

    enum class InputType : int
    {
        MainInput = 0,
        PitchPercentileCV = 1,
        IntervalCV = 2,
        NumInputTypes = 3,
    };

    static constexpr size_t x_numInputsPerType[] =
    {
        x_numInputs,
        x_numAccumulators,
        x_numAccumulators
    };

    static constexpr size_t x_inputStartPerType[] =
    {
        0,
        x_numInputsPerType[0],
        x_numInputsPerType[0] + x_numInputsPerType[1],
        x_numInputsPerType[0] + x_numInputsPerType[1] + x_numInputsPerType[2]
    };

    static constexpr size_t GetInputId(InputType inputType, size_t inputId)
    {
        return x_inputStartPerType[static_cast<int>(inputType)] + inputId;
    }

    static constexpr size_t GetMainInputId(size_t inputId)
    {
        return GetInputId(InputType::MainInput, inputId);
    }

    static constexpr size_t GetPitchPercentileCVInputId(size_t accumulatorId)
    {
        return GetInputId(InputType::PitchPercentileCV, accumulatorId);
    }

    static constexpr size_t GetIntervalCVInputId(size_t accumulatorId)
    {
        return GetInputId(InputType::IntervalCV, accumulatorId);
    }

    static constexpr size_t GetNumInputs()
    {
        return x_inputStartPerType[static_cast<int>(InputType::NumInputTypes)];
    }

    enum class OutputType : int
    {
        OperationOutput = 0,
        MainOutput = 1,
        TriggerOutput = 2,
        NumOutputTypes = 3
    };

    static constexpr size_t x_numOutputsPerType[] =
    {
        x_numOperations,
        x_numAccumulators,
        x_numAccumulators
    };

    static constexpr size_t x_outputStartPerType[] =
    {
        0,
        x_numOutputsPerType[0],
        x_numOutputsPerType[0] + x_numOutputsPerType[1],
        x_numOutputsPerType[0] + x_numOutputsPerType[1] + x_numOutputsPerType[2],
    };

    static constexpr size_t GetOutputId(OutputType outputType, size_t outputId)
    {
        return x_outputStartPerType[static_cast<int>(outputType)] + outputId;
    }

    static constexpr size_t GetOperationOutputId(size_t outputId)
    {
        return GetOutputId(OutputType::OperationOutput, outputId);
    }

    static constexpr size_t GetMainOutputId(size_t outputId)
    {
        return GetOutputId(OutputType::MainOutput, outputId);
    }

    static constexpr size_t GetTriggerOutputId(size_t outputId)
    {
        return GetOutputId(OutputType::TriggerOutput, outputId);
    }

    static constexpr size_t GetNumOutputs()
    {
        return x_outputStartPerType[static_cast<int>(OutputType::NumOutputTypes)];
    }

    enum class LightType : int
    {
        InputLight = 0,
        OperationLight = 1,
        TriggerLight = 2,
        NumLightTypes = 3
    };

    static constexpr size_t x_numLightsPerType[] =
    {
        x_numInputs,
        x_numOperations,
        x_numAccumulators
    };

    static constexpr size_t x_lightStartPerType[] =
    {
        0,
        x_numLightsPerType[0],
        x_numLightsPerType[0] + x_numLightsPerType[1],
        x_numLightsPerType[0] + x_numLightsPerType[1] + x_numLightsPerType[2]
    };

    static constexpr size_t GetLightId(LightType lightType, size_t lightId)
    {
        return x_lightStartPerType[static_cast<int>(lightType)] + lightId;
    }

    static constexpr size_t GetInputLightId(size_t lightId)
    {
        return GetLightId(LightType::InputLight, lightId);
    }

    static constexpr size_t GetOperationLightId(size_t lightId)
    {
        return GetLightId(LightType::OperationLight, lightId);
    }

    static constexpr size_t GetTriggerLightId(size_t lightId)
    {
        return GetLightId(LightType::TriggerLight, lightId);
    }

    static constexpr size_t GetNumLights()
    {
        return x_lightStartPerType[static_cast<int>(LightType::NumLightTypes)];
    }
}
