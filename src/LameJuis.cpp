#include "LameJuis.hpp"

template<typename Enum>
Enum FloatToEnum(float in)
{
    return static_cast<Enum>(static_cast<int>(in + 0.5));
}

void LameJuis::Input::SetValue(LameJuis::Input* prev)
{
    using namespace LameJuisConstants;
    bool oldValue = m_value;
        
    // If a cable is connected, use that value.
    //
    if (m_port->isConnected())
    {
        float cableValue = m_port->getVoltage();
        m_schmittTrigger.process(cableValue);
        m_value = m_schmittTrigger.isHigh();
        if (m_value && !oldValue)
        {
            // Count down instead of up so each input is high on its "even" beats.
            //
            --m_counter;
        }
    }
    
    // Each input (except the first) is normaled to divide-by-two of the previous input,
    // but in order to keep the phases in line, they are actually a divide-by-2-to-the-n
    // of first plugged input above, where n is the number of unplugged inputs between.
    // This is easiest to implement as cascading counters.
    //
    else if (prev)
    {
        m_value = prev->m_counter % 2 != 0;
        m_counter = prev->m_counter / 2;        
    }

    if (oldValue != m_value)
    {
        m_light->setBrightness(m_value ? 1.f : 0.f);
    }
}

LameJuis::MatrixElement::SwitchVal
LameJuis::MatrixElement::GetSwitchVal()
{
    return FloatToEnum<SwitchVal>(m_switch->getValue());
}

size_t LameJuis::InputVector::CountSetBits()
{
    static const uint8_t x_bitsSet [16] =
    {
        0, 1, 1, 2, 1, 2, 2, 3, 
        1, 2, 2, 3, 2, 3, 3, 4
    };

    return x_bitsSet[m_bits & 0x0F] + x_bitsSet[m_bits >> 4];
}

LameJuis::LogicOperation::Operator
LameJuis::LogicOperation::GetOperator()
{
    return FloatToEnum<Operator>(m_operatorKnob->getValue());
}

LameJuis::LogicOperation::SwitchVal
LameJuis::LogicOperation::GetSwitchVal()
{
    return FloatToEnum<SwitchVal>(m_switch->getValue());
}

bool LameJuis::LogicOperation::GetValue(InputVector inputVector)
{
    using namespace LameJuisConstants;   

    // And with m_active to mute the muted inputs.
    // Xor with m_inverted to invert the inverted ones.
    //
    inputVector.m_bits &= m_active.m_bits;
    inputVector.m_bits ^= m_inverted.m_bits;
    
    size_t countTotal = m_active.CountSetBits();
    size_t countHigh = inputVector.CountSetBits();

    bool ret = false;
    switch (GetOperator())
    {
        case Operator::Or: ret = (countHigh > 0); break;
        case Operator::And: ret = (countHigh == countTotal); break;
        case Operator::Xor: ret = (countHigh % 2 == 1); break;
        case Operator::AtLeastTwo: ret = (countHigh >= 2); break;
        case Operator::Majority: ret = (2 * countHigh > countTotal); break;
    }

    return ret;
}

LameJuis::MatrixEvalResultWithPitch LameJuis::EvalMatrix(InputVector inputVector)
{
    using namespace LameJuisConstants;
    MatrixEvalResult& result = m_evalResults[inputVector.m_bits];
    
    if (!m_isEvaluated[inputVector.m_bits])
    {
        result.Clear();
        for (size_t i = 0; i < x_numOperations; ++i)
        {
            size_t outputId = m_operations[i].GetOutputTarget();
            bool isHigh = m_operations[i].GetValue(inputVector);
            if (isHigh)
            {
                ++result.m_high[outputId];
            }
        }

        m_isEvaluated[inputVector.m_bits] = true;
    }

    return MatrixEvalResultWithPitch(result, m_accumulators);
}

LameJuis::InputVectorIterator::InputVectorIterator(InputVector coMuteVector, InputVector defaultVector)
    : m_coMuteVector(coMuteVector)
    , m_coMuteSize(m_coMuteVector.CountSetBits())
    , m_defaultVector(defaultVector)
{
    size_t j = 0;
    for (size_t i = 0; i < m_coMuteSize; ++i)
    {
        while (!m_coMuteVector.Get(j))
        {
            ++j;
        }

        m_forwardingIndices[i] = j;
        ++j;
    }
}

LameJuis::InputVector
LameJuis::InputVectorIterator::Get()
{
    InputVector result = m_defaultVector;
    
    // Shift the bits of m_ordinal into the set positions of the co muted vector.
    //
    for (size_t i = 0; i < m_coMuteSize; ++i)
    {
        result.Set(m_forwardingIndices[i], InputVector(m_ordinal).Get(i));
    }

    return result;
}

void LameJuis::InputVectorIterator::Next()
{
    ++m_ordinal;
}

bool LameJuis::InputVectorIterator::Done()
{
    return (1 << m_coMuteSize) <= m_ordinal;
}

constexpr float LameJuis::Accumulator::x_voltages[];
constexpr int LameJuis::Accumulator::x_semitones[];

LameJuis::Accumulator::Interval
LameJuis::Accumulator::GetInterval()
{
    return FloatToEnum<Interval>(m_intervalKnob->getValue());
}

float
LameJuis::Accumulator::GetPitch()
{
    return x_voltages[static_cast<size_t>(GetInterval())] + m_intervalCV->getVoltage();
}

LameJuis::MatrixEvalResultWithPitch
LameJuis::Output::CacheForSingleInputVector::ComputePitch(
    LameJuis* matrix,
    LameJuis::Output* output,
    LameJuis::InputVector defaultVector,
    float percentile)
{
    if (!m_isEvaluated)
    {
        InputVectorIterator itr = output->GetInputVectorIterator(defaultVector);
        for (; !itr.Done(); itr.Next())
        {
            m_cachedResults[itr.m_ordinal] = matrix->EvalMatrix(itr.Get());
        }

        m_numResults = itr.m_ordinal;

        std::sort(m_cachedResults, m_cachedResults + m_numResults);
        
        m_isEvaluated = true;
    }

    ssize_t ix = static_cast<size_t>(percentile * m_numResults);
    ix = std::min<ssize_t>(ix, m_numResults - 1);
    ix = std::max<ssize_t>(ix, 0);
    return m_cachedResults[ix];
}


LameJuis::MatrixEvalResultWithPitch
LameJuis::Output::ComputePitch(LameJuis* matrix, LameJuis::InputVector defaultVector)
{
    float percentile = m_coMuteState.GetPercentile();
    return m_outputCaches[defaultVector.m_bits].ComputePitch(matrix, this, defaultVector, percentile);
}

LameJuis::LameJuis()
{
    using namespace LameJuisConstants;   
    
    config(GetNumParams(), GetNumInputs(), GetNumOutputs(), GetNumLights());
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        configInput(GetMainInputId(i), "Main Out " + std::to_string(i));
        
        for (size_t j = 0; j < x_numOperations; ++j)
        {
            configParam(GetMatrixSwitchId(i, j), 0.f, 2.f, 1.f, "");
            m_operations[j].m_elements[i].Init(&params[GetMatrixSwitchId(i, j)]);
        }

        for (size_t j = 0; j < x_numAccumulators; ++j)
        {
            configParam(GetPitchCoMuteSwitchId(i, j), 0.f, 1.f, 1.f, "Co-Mute Switch " + std::to_string(i) + "," + std::to_string(j));
            m_outputs[j].m_coMuteState.m_switches[i].Init(
                &params[GetPitchCoMuteSwitchId(i, j)]);
        }

        m_inputs[i].Init(
            &inputs[GetMainInputId(i)],
            &lights[GetInputLightId(i)]);
    }
    
    for (size_t i = 0; i < x_numOperations; ++i)
    {
        configParam(GetOperationSwitchId(i), 0.f, 2.f, 1.f, "");
        configParam(GetOperatorKnobId(i), 0.f, 4.f, 0.f, "");
        configOutput(GetOperationOutputId(i), "Logic Out " + std::to_string(i));

        m_operations[i].Init(
            &params[GetOperationSwitchId(i)],
            &params[GetOperatorKnobId(i)],
            &outputs[GetOperationOutputId(i)],
            &lights[GetOperationLightId(i)]);
    }
    
    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        configParam(GetAccumulatorIntervalKnobId(i), 0.f, 11.f, 0.f, "Accum Interval Knob " + std::to_string(i));
        configParam(GetPitchPercentileKnobId(i), 0.f, 1.f, 0.f, "Voice Percentile Knob " + std::to_string(i));

        configInput(GetIntervalCVInputId(i), "Interval CV In " + std::to_string(i));
        configInput(GetPitchPercentileCVInputId(i), "Pitch Percentile CV in " + std::to_string(i));

        configOutput(GetMainOutputId(i), "Pitch Out " + std::to_string(i));
        configOutput(GetTriggerOutputId(i), "Trigger " + std::to_string(i));

        m_accumulators[i].Init(
            &params[GetAccumulatorIntervalKnobId(i)],
            &inputs[GetIntervalCVInputId(i)]);

        m_outputs[i].Init(
            &outputs[GetMainOutputId(i)],
            &outputs[GetTriggerOutputId(i)],
            &lights[GetTriggerLightId(i)],
            &params[GetPitchPercentileKnobId(i)],
            &inputs[GetPitchPercentileCVInputId(i)]);
    }

    rightExpander.producerMessage = m_rightMessages[0];
    rightExpander.consumerMessage = m_rightMessages[1];
}

void LameJuis::CheckMatrixChangedAndInvalidateCache()
{
    using namespace LameJuisConstants;

    // Check if the matrix has changed, and if it has, invalidate all caches.
    //
    bool anyChanged = false;
    for (size_t i = 0; i < x_numOperations; ++i)
    {
        if (m_operations[i].AnyThingChanged())
        {
            anyChanged = true;
        }
    }

    if (anyChanged)
    {
        ClearCaches();
    }

    // Check if any pitches have changed, and if so, invalidate all caches.
    // This means LameJuis will use a lot more CPU if a user is modulating the intervals.
    //
    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        if (m_accumulators[i].HasChanged())
        {
            anyChanged = true;
        }
    }

    if (anyChanged)
    {
        ClearOutputCaches();
    }

    // Finally, check if any co-mute states have changed, and invalidate the cache just of that output.
    //
    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        if (m_outputs[i].HasCoMutesChanged())
        {
            m_outputs[i].ClearAllCaches();
        }
    }
}

LameJuis::InputVector
LameJuis::ProcessInputs()
{
    using namespace LameJuisConstants;

    InputVector result;
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        m_inputs[i].SetValue(i > 0 ? &m_inputs[i - 1] : nullptr);
        result.Set(i, m_inputs[i].m_value);
    }

    return result;
}

void LameJuis::ProcessOperations(InputVector defaultVector)
{
    using namespace LameJuisConstants;
    
    for (size_t i = 0; i < x_numOperations; ++i)
    {
        m_operations[i].SetBitVectors();
        bool value = m_operations[i].GetValue(defaultVector);
        m_operations[i].SetOutput(value);
    }
}

void LameJuis::ProcessOutputs(InputVector defaultVector, float dt)
{
    using namespace LameJuisConstants;

    LatticeExpanderMessage msg;

    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        MatrixEvalResultWithPitch res = m_outputs[i].ComputePitch(this, defaultVector);
        m_outputs[i].SetPitch(res.m_pitch, dt);
        for (size_t j = 0; j < x_numAccumulators; ++j)
        {
            msg.m_position[i][j] = res.m_result.m_high[j];
        }
    }

    SendExpanderMessage(msg);
}

void LameJuis::process(const ProcessArgs& args)
{
    CheckMatrixChangedAndInvalidateCache();
    InputVector defaultVector = ProcessInputs();
    ProcessOperations(defaultVector);
    ProcessOutputs(defaultVector, args.sampleTime);
}
