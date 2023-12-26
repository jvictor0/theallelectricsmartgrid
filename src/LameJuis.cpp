#include "LameJuis.hpp"

template<typename Enum>
Enum FloatToEnum(float in)
{
    return static_cast<Enum>(static_cast<int>(in + 0.5));
}

void LameJuis::Input::Preprocess(
    size_t inputId,
    LameJuis::Input* prev,
    LameJuis::PreprocessState& preprocessState)
{
    using namespace LameJuisConstants;
    bool oldValue = m_value;
    m_changed = false;
        
    // If a cable is connected, use that value.
    //
    if (m_port->isConnected())
    {
        float cableValue = m_port->getVoltage();
        m_schmittTrigger.process(cableValue);
        m_value = m_schmittTrigger.isHigh();
        if (m_value && !oldValue)
        {
            if (m_value && m_reset)
            {
                m_resetAcknowledged = true;
                m_counter = 0;
            }
            
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
        if (m_reset)
        {
            m_resetAcknowledged = true;
        }

        m_value = prev->m_counter % 2 != 0;
        m_counter = prev->m_counter / 2;
    }

    if (oldValue != m_value)
    {
        m_changed = true;
        m_light->setBrightness(m_value ? 1.f : 0.f);
        preprocessState.m_inputChanged[inputId] = true;
    }
}

LameJuis::MatrixElement::SwitchVal
LameJuis::MatrixElement::GetSwitchVal()
{
    return FloatToEnum<SwitchVal>(m_switch->getValue());
}

void LameJuis::MatrixElement::Randomize(int level)
{
    using namespace LameJuisConstants;   
    float rnd = static_cast<float>(rand()) / RAND_MAX;

    // The level determines the expected number of non-muted switches per row.
    // 
    float nonMuteProb = static_cast<float>(level + 1) / x_numInputs;
    if (rnd < 0.5 * nonMuteProb)
    {
        m_switch->setValue(static_cast<int>(SwitchVal::Normal));
        assert(GetSwitchVal() == SwitchVal::Normal);
    }
    else if (rnd < nonMuteProb)
    {
        m_switch->setValue(static_cast<int>(SwitchVal::Inverted));
        assert(GetSwitchVal() == SwitchVal::Inverted);
    }
    else
    {
        m_switch->setValue(static_cast<int>(SwitchVal::Muted));
        assert(GetSwitchVal() == SwitchVal::Muted);
    }
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
        case Operator::Off: ret = false; break;
        case Operator::NumOperations: ret = false; break;
    }

    return ret;
}

void LameJuis::LogicOperation::Randomize(int level)
{
    using namespace LameJuisConstants;

    size_t switchesOn = 0;

    // Randomize the matrix elements first.
    //
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        m_elements[i].Randomize(level);
        if (m_elements[i].GetSwitchVal() != MatrixElement::SwitchVal::Muted)
        {
            ++switchesOn;
        }
    }

    // Randomly pick which interval.
    //
    {
        float rnd = static_cast<float>(rand()) / RAND_MAX;
        m_switch->setValue(static_cast<int>(rnd * 3));
    }

    // Finally, pick an operator.
    // Or and And are the most common, followed by majority and at least 2, then xor.
    // If there are fewer than three switches, don't pick majority or AtLeastTwo.
    //
    if (level == 0 || level == 1)
    {
        // Pick a tier, then pick an operator.
        //
        float rnd = static_cast<float>(rand()) / RAND_MAX;
        if (rnd < 0.5 || level == 0 || (rnd < 0.8 && switchesOn < 3))
        {
            rnd = static_cast<float>(rand()) / RAND_MAX;
            if (rnd < 0.5)
            {
                m_operatorKnob->setValue(static_cast<int>(Operator::Or));
            }
            else
            {
                m_operatorKnob->setValue(static_cast<int>(Operator::And));
            }     
        }
        else if (rnd < 0.8)
        {
            rnd = static_cast<float>(rand()) / RAND_MAX;
            if (rnd < 0.5)
            {
                m_operatorKnob->setValue(static_cast<int>(Operator::AtLeastTwo));
            }
            else
            {
                m_operatorKnob->setValue(static_cast<int>(Operator::Majority));
            }     
        }
        else
        {
            m_operatorKnob->setValue(static_cast<int>(Operator::Xor));
        }
    }
    else
    {
        // Level 3: Uniform
        //
        float rnd = static_cast<float>(rand()) / RAND_MAX;
        m_operatorKnob->setValue(static_cast<int>(rnd * static_cast<int>(Operator::NumOperations)));
    }
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

void
LameJuis::Accumulator::Randomize(int level)
{
    // Sometimes off or octave are the only two options.
    //
    if (level < 0)
    {
        if (static_cast<float>(rand()) / RAND_MAX < 0.5)
        {
            m_intervalKnob->setValue(static_cast<int>(Interval::Off));
        }
        else
        {
            m_intervalKnob->setValue(static_cast<int>(Interval::Octave));
        }
    }
    else
    {        
        float non12EDOLikeProb;
        if (level == 0)
        {
            non12EDOLikeProb = 0.0;
        }
        else if (level == 1)
        {
            non12EDOLikeProb = 0.25;
        }
        else
        {
            non12EDOLikeProb = 0.5;
        }
        
        float rnd = static_cast<float>(rand()) / RAND_MAX;
        if (rnd < non12EDOLikeProb)
        {
            rnd = static_cast<float>(rand()) / RAND_MAX;
            int numXenIntervals = static_cast<int>(Interval::NumIntervals) - x_end12EDOLikeIx;
            m_intervalKnob->setValue(static_cast<int>(x_end12EDOLikeIx + rnd * numXenIntervals));
        }
        else
        {
            rnd = static_cast<float>(rand()) / RAND_MAX;
            m_intervalKnob->setValue(static_cast<int>(rnd * x_end12EDOLikeIx));
        }
    }
}

void
LameJuis::CoMuteState::RandomizeCoMutes(int level)
{
    using namespace LameJuisConstants;

    bool setOff = false;
    
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        if (setOff)
        {
            // Setting to one sets the comute off.  Once the 'setOff' var is set, everything to the left
            // is set off.
            //
            m_switches[i].m_switch->setValue(1);
        }
        else
        {
            float rnd = static_cast<float>(rand()) / RAND_MAX;
            if (rnd < 0.5)
            {
                // Setting to zero sets the comute on.
                //
                m_switches[i].m_switch->setValue(0);                
            }
            else 
            {
                // For level zero, only set a gemetrically sized prefix of switches.
                // For level one, occasionally skip a comute.
                // Level two is uniform.
                //
                m_switches[i].m_switch->setValue(1);
                if (level == 0 ||
                     (level == 1 && rnd < 0.75))
                {
                    setOff = true;
                }
            }
                
        }
    }
}

void
LameJuis::CoMuteState::RandomizePercentiles()
{
    m_percentileKnob->setValue(static_cast<float>(rand()) / RAND_MAX);
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
LameJuis::Output::ComputePitch(LameJuis* matrix, LameJuis::InputVector defaultVector, size_t chan)
{
    float percentile = m_coMuteState.m_percentileCVValue[chan];
    return m_outputCaches[defaultVector.m_bits].ComputePitch(matrix, this, defaultVector, percentile);
}

LameJuis::LameJuis()
{
    srand(time(nullptr));
    
    using namespace LameJuisConstants;   
    
    config(GetNumParams(), GetNumInputs(), GetNumOutputs(), GetNumLights());
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        std::string inputName = "";
        inputName += ('A' + i);
        configInput(GetMainInputId(i), "Gate " + inputName);
        
        for (size_t j = 0; j < x_numOperations; ++j)
        {
            std::string switchName = "Step " + std::to_string(j+1) + " gate " + inputName;
            configSwitch(GetMatrixSwitchId(i, j), 0.f, 2.f, 1.f, switchName, {"Inverted","Off","On"});
            m_operations[j].m_elements[i].Init(&params[GetMatrixSwitchId(i, j)]);
        }

        for (size_t j = 0; j < x_numAccumulators; ++j)
        {
            configSwitch(GetPitchCoMuteSwitchId(i, j), 0.f, 1.f, 1.f, "Voice " + std::to_string(j+1) + " co-mute " + inputName, {"Co-Muted", "On"});
            m_outputs[j].m_coMuteState.m_switches[i].Init(
                &params[GetPitchCoMuteSwitchId(i, j)],
                &lights[GetCoMuteLightId(i, j)]);
        }

        m_inputs[i].Init(
            &inputs[GetMainInputId(i)],
            &lights[GetInputLightId(i)]);
    }
    
    for (size_t i = 0; i < x_numOperations; ++i)
    {
        configSwitch(GetOperationSwitchId(i), 0.f, 2.f, 1.f, "Step " + std::to_string(i+1) + " interval", {"Bottom", "Middle", "Top"});
        configSwitch(GetOperatorKnobId(i), 0.f, 5.f, 0.f, "Step " + std::to_string(i+1) + " operator", LogicOperation::GetLogicNames());
        configOutput(GetOperationOutputId(i), "Step " + std::to_string(i+1) + " gate");

        m_operations[i].Init(
            &params[GetOperationSwitchId(i)],
            &params[GetOperatorKnobId(i)],
            &outputs[GetOperationOutputId(i)],
            &lights[GetOperationLightId(i)]);
    }
    
    configSwitch(GetAccumulatorIntervalKnobId(0), 0.f, 11.f, 0.f, "Top interval", Accumulator::GetIntervalNames());
    configInput(GetIntervalCVInputId(0), "Top interval CV");

    configSwitch(GetAccumulatorIntervalKnobId(1), 0.f, 11.f, 0.f, "Middle interval", Accumulator::GetIntervalNames());
    configInput(GetIntervalCVInputId(1), "Middle interval CV");

    configSwitch(GetAccumulatorIntervalKnobId(2), 0.f, 11.f, 0.f, "Bottom interval", Accumulator::GetIntervalNames());
    configInput(GetIntervalCVInputId(2), "Bottom interval CV");

    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        std::string name = "Voice " + std::to_string(i+1);
        configParam(GetPitchPercentileKnobId(i), 0.f, 1.f, 0.f, name + " pitch select");
        configInput(GetPitchPercentileCVInputId(i), name + " pitch select CV");

        configOutput(GetMainOutputId(i), name + " V/Oct");
        configOutput(GetTriggerOutputId(i), name + " trigger");

        m_accumulators[i].Init(
            &params[GetAccumulatorIntervalKnobId(i)],
            &inputs[GetIntervalCVInputId(i)],
            &m_12EDOMode);

        m_outputs[i].Init(
            &outputs[GetMainOutputId(i)],
            &outputs[GetTriggerOutputId(i)],
            &lights[GetTriggerLightId(i)],
            &params[GetPitchPercentileKnobId(i)],
            &inputs[GetPitchPercentileCVInputId(i)]);
    }

    configInput(GetResetInputId(), "Reset");
    configInput(GetClockInputId(), "Clock");
    m_resetPort = &inputs[GetResetInputId()];
    m_reset = false;

    m_clockPort = &inputs[GetClockInputId()];
    
    rightExpander.producerMessage = m_rightMessages[0];
    rightExpander.consumerMessage = m_rightMessages[1];

    m_12EDOMode = false;
    m_timeQuantizeMode = true;
}

void LameJuis::ProcessReset()
{
    using namespace LameJuisConstants;
    
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        if (m_inputs[i].m_resetAcknowledged)
        {
            m_inputs[i].m_reset = false;
            m_inputs[i].m_resetAcknowledged = false;
        }
    }

    float resetCableValue = m_resetPort->getVoltage();
    m_resetSchmittTrigger.process(resetCableValue);
    if (!m_reset && m_resetSchmittTrigger.isHigh())
    {
        for (size_t i = 0; i < x_numInputs; ++i)
        {
            m_inputs[i].m_reset = true;
        }
    }
    
    m_reset = m_resetSchmittTrigger.isHigh();
}    

void LameJuis::ProcessOperations(
    PreprocessState& preprocessState,
    InputVector defaultVector)
{
    using namespace LameJuisConstants;
    
    for (size_t i = 0; i < x_numOperations; ++i)
    {
        m_operations[i].SetBitVectors();
        if (preprocessState.m_recomputeLogic[i])
        {
            bool value = m_operations[i].GetValue(defaultVector);
            m_operations[i].SetOutput(value);
        }
    }
}

void LameJuis::ProcessOutputs(
    PreprocessState& preprocessState,
    InputVector defaultVector,
    float dt)
{
    using namespace LameJuisConstants;

    LatticeExpanderMessage msg;

    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        size_t chans = m_outputs[i].SetPolyChans();
        msg.m_polyChans[i] = m_outputs[i].IsPlugged() ? chans : 0;
        for (size_t j = 0; j < chans; ++j)
        {
            if (preprocessState.m_recomputeVoice[i][j])
            {
                MatrixEvalResultWithPitch res = m_outputs[i].ComputePitch(this, defaultVector, j);
                m_outputs[i].SetPitch(res, dt, j);
            }

            for (size_t k = 0; k < x_numAccumulators; ++k)
            {
                msg.m_position[i][j][k] = m_outputs[i].m_pitch[j].m_result.m_high[k];
            }
        }
    }

    ProcessTriggers(dt);
    SendExpanderMessage(msg);
}

void LameJuis::RandomizeMatrix(int level)
{
    using namespace LameJuisConstants;

    for (size_t i = 0; i < x_numOperations; ++i)
    {
        m_operations[i].Randomize(level);
    }
}

void LameJuis::RandomizeIntervals(int level)
{
    using namespace LameJuisConstants;

    for (ssize_t i = x_numAccumulators - 1; 0 <= i; --i)
    {
        while (true)
        {
            // Set the last accumulator at a lower level than the others (this effectively sets it to off
            // or octaves for level 0).
            //
            m_accumulators[i].Randomize(i == x_numAccumulators - 1 ? level - 1 : level);

            // Check for double-intervals, and retry if found.
            //
            bool unique = true;
            for (ssize_t j = x_numAccumulators - 1; i < j; --j)
            {
                if (m_accumulators[i].GetInterval() == m_accumulators[j].GetInterval())
                {
                    unique = false;
                }
            }

            if (unique)
            {
                break;
            }
        }
    }
}

void LameJuis::RandomizeCoMutes(int level)
{
    using namespace LameJuisConstants;

    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        m_outputs[i].RandomizeCoMutes(level);
    }
}

void LameJuis::RandomizePercentiles()
{
    using namespace LameJuisConstants;

    for (size_t i = 0; i < x_numAccumulators; ++i)
    {
        m_outputs[i].RandomizePercentiles();
    }
}

void LameJuis::process(const ProcessArgs& args)
{
    using namespace LameJuisConstants;

    if (!ShouldDoStep())
    {
        ProcessReset();
        ProcessTriggers(args.sampleTime);
        return;
    }
    
    PreprocessState preprocessState(m_timeQuantizeMode);
    Preprocess(preprocessState);
    
    InputVector defaultVector;
    for (size_t i = 0; i < x_numInputs; ++i)
    {
        defaultVector.Set(i, m_inputs[i].m_value);
    }  
    
    ProcessOperations(preprocessState, defaultVector);
    ProcessOutputs(preprocessState, defaultVector, args.sampleTime);

    m_lastInput = defaultVector;
}

json_t* LameJuis::dataToJson()
{
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "12EDOMode", json_boolean(m_12EDOMode));
    json_object_set_new(rootJ, "TimeQuantizeMode", json_boolean(m_timeQuantizeMode));
    return rootJ;
}

void LameJuis::dataFromJson(json_t* rootJ)
{
    json_t* twelveEDOModeJ = json_object_get(rootJ, "12EDOMode");
    if (twelveEDOModeJ)
    {
        m_12EDOMode = json_boolean_value(twelveEDOModeJ);
	}

    json_t* timeQuantizeModeJ = json_object_get(rootJ, "TimeQuantizeMode");
    if (timeQuantizeModeJ)
    {
        m_timeQuantizeMode = json_boolean_value(timeQuantizeModeJ);
	}
};
