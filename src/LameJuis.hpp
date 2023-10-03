#pragma once
#include "plugin.hpp"
#include <cstddef>
#include "LameJuisConstants.hpp"
#include "LatticeExpander.hpp"

struct LameJuis : Module
{
    LatticeExpanderMessage m_rightMessages[2][1];

    struct PreprocessState
    {
        PreprocessState(bool timeQuantizeMode)
        {
            using namespace LameJuisConstants;

            m_timeQuantizeMode = timeQuantizeMode;
            m_invalidateCache = false;
            
            for (size_t i = 0; i < LameJuisConstants::x_numInputs; ++i)
            {
                m_inputChanged[i] = false;
            }

            for (size_t i = 0; i < LameJuisConstants::x_numAccumulators; ++i)
            {
                for (size_t j = 0; j < LameJuisConstants::x_maxPoly; ++j)
                {
                    m_recomputeVoice[i][j] = false;
                }
            }

            for (size_t i = 0; i < LameJuisConstants::x_numOperations; ++i)
            {
                m_recomputeLogic[i] = false;
            }
        }

        void SetRecomputeVoice(size_t voiceId)
        {
            for (size_t i = 0; i < LameJuisConstants::x_maxPoly; ++i)
            {
                m_recomputeVoice[voiceId][i] = true;
            }
        }
        
        bool m_timeQuantizeMode;
        bool m_invalidateCache;
        bool m_inputChanged[LameJuisConstants::x_numInputs];
        bool m_recomputeVoice[LameJuisConstants::x_numAccumulators][LameJuisConstants::x_maxPoly];
        bool m_recomputeLogic[LameJuisConstants::x_numOperations];
    };
    
    struct Input
    {
        rack::engine::Input* m_port = nullptr;
        rack::dsp::TSchmittTrigger<float> m_schmittTrigger;
        rack::engine::Light* m_light = nullptr;
        bool m_value = false;
        uint8_t m_counter = 0;
        bool m_changed;
        bool m_reset;
        bool m_resetAcknowledged;

        void Init(
            rack::engine::Input* port,
            rack::engine::Light* light)
        {
            m_port = port;
            m_light = light;
            m_counter = 0;
            m_changed = false;
            m_reset = false;
            m_resetAcknowledged = false;
        }
        
        void Preprocess(size_t inputId, Input* prev, PreprocessState& preprocessState);
    };

    struct MatrixElement
    {
        enum class SwitchVal : char
        {
            Inverted = 0,
            Muted = 1,
            Normal = 2
        };
        
        void Init(rack::engine::Param* swtch)
        {
            m_switch = swtch;
            m_value = GetSwitchVal();
            m_changed = false;
            m_armed = true;
        }
        
        SwitchVal GetSwitchVal();

        SwitchVal m_value;
        bool m_changed;
        bool m_armed;

        void Preprocess(PreprocessState& preprocessState)
        {
            SwitchVal newVal = GetSwitchVal();
            if (m_value != newVal)
            {
                m_value = newVal;
                m_changed = true;
                preprocessState.m_invalidateCache = true;
                m_armed = true;
            }
            else
            {
                if (m_value != SwitchVal::Muted)
                {
                    m_armed = true;
                }

                m_changed = false;
            }
        }

        void Randomize(int level);
        
        rack::engine::Param* m_switch = nullptr;
    };

    struct InputVector
    {
        InputVector()
            : m_bits(0)
        {
        }

        InputVector(uint8_t bits)
            : m_bits(bits)
        {
        }
        
        bool Get(size_t i)
        {
            return (m_bits & (1 << i)) >> i;
        }

        void Set(size_t i, bool value)
        {
            if (value)
            {
                m_bits |= 1 << i;
            }
            else
            {
                m_bits &= ~(1 << i);
            }
        }

        size_t CountSetBits();

        uint8_t m_bits;
    };
    
    struct LogicOperation
    {
        enum class Operator : char
        {
            Or = 0,
            And = 1,
            Xor = 2,
            AtLeastTwo = 3,
            Majority = 4,
            Off = 5,
            NumOperations = 6,
        };

        static std::vector<std::string> GetLogicNames()
        {
            return std::vector<std::string>({
                    "Or",
                    "And",
                    "Xor",
                    "At Least Two",
                    "Majority",
                    "Off"
                });
        }
        
        enum class SwitchVal : char
        {
            Down = 0,
            Middle = 1,
            Up = 2
        };

        Operator GetOperator();
        SwitchVal GetSwitchVal();
        
        void SetBitVectors()
        {
            using namespace LameJuisConstants;
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                MatrixElement::SwitchVal switchVal = m_elements[i].m_value;
                m_active.Set(i, switchVal != MatrixElement::SwitchVal::Muted);
                m_inverted.Set(i, switchVal == MatrixElement::SwitchVal::Inverted);
            }
        }
        
        void Init(
            rack::engine::Param* swtch,
            rack::engine::Param* operatorKnob,
            rack::engine::Output* output,
            rack::engine::Light* light)
        {
            m_switch = swtch;
            m_operatorKnob = operatorKnob;
            m_switchChanged = false;
            m_operatorChanged = false;
            m_output = output;
            m_light = light;
        }            

        bool GetValue(InputVector inputVector);

        void SetOutput(bool value)
        {
            m_output->setVoltage(value ? 5.f : 0.f);
            m_light->setBrightness(value ? 1.f : 0.f);
        }

        // Up is output zero but input id 2, so invert.
        //
        size_t GetOutputTarget()
        {
            using namespace LameJuisConstants;
            return x_numAccumulators - static_cast<size_t>(m_switchValue) - 1;
        }

        void Preprocess(size_t operatorId, PreprocessState& preprocessState)
        {
            using namespace LameJuisConstants;

            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_elements[i].Preprocess(preprocessState);
            }
            
            Operator op = GetOperator();
            if (op != m_operatorValue)
            {
                m_operatorValue = op;
                m_operatorChanged = true;
                preprocessState.m_invalidateCache = true;
            }

            SwitchVal sv = GetSwitchVal();
            if (sv != m_switchValue)
            {
                m_switchValue = sv;
                m_switchChanged = true;
                preprocessState.m_invalidateCache = true;
            }

            for (size_t i = 0; i < x_numInputs; ++i)
            {
                if (preprocessState.m_inputChanged[i] &&
                    (!preprocessState.m_timeQuantizeMode || m_elements[i].m_armed))
                {
                    preprocessState.m_recomputeLogic[operatorId] = true;
                }
            }

            if (preprocessState.m_recomputeLogic[operatorId])
            {
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    m_elements[i].m_armed = false;
                }
            }
        }

        void Randomize(int level);

        rack::engine::Param* m_switch = nullptr;
        rack::engine::Param* m_operatorKnob = nullptr;
        rack::engine::Light* m_light = nullptr;
        rack::engine::Output* m_output = nullptr;
        InputVector m_active;
        InputVector m_inverted;

        Operator m_operatorValue;
        bool m_operatorChanged;
        SwitchVal m_switchValue;
        bool m_switchChanged;

        LameJuis::MatrixElement m_elements[LameJuisConstants::x_numInputs];
    };

    struct Accumulator
    {
        enum class Interval
        {
            Off = 0,
            Octave = 1,
            PerfectFifth = 2,
            MajorThird = 3,
            PerfectFourth = 4,
            MinorThird = 5,            
            WholeStep = 6,
            HalfStep = 7,
            SevenHarm = 8,
            ElevenHarm = 9,
            ThirteenHarm = 10,
            ThirtyOneHarm = 11,
            NumIntervals = 12,
        };

        static std::vector<std::string> GetIntervalNames()
        {
            return std::vector<std::string>({
                    "Off",
                    "Octave",
                    "Perfect Fifth",
                    "Major Third",
                    "Perfect Fourth",
                    "Minor Third",
                    "Whole Step",
                    "Half Step",
                    "Seventh Harmonic",
                    "Eleventh Harmonic",
                    "Thirteenth Harmonic",
                    "Thirty-First Harmonic"});
        }
        
        static constexpr float x_voltages[] = {
            0 /*Off*/,
            1.0 /*octave = log_2(2)*/,
            0.5849625007211562 /*pefect fifth = log_2(3/2)*/,
            0.32192809488736235 /*major third = log_2(5/4)*/,
            0.4150374992788437 /*perfect fourth = log_2(4/3)*/,
            0.2630344058337938 /*minor third = log_2(6/5)*/,
            0.16992500144231237/*whole tone = log_2(9/8)*/,
            0.09310940439 /*half step = log_2(16/15)*/,
            0.8073549220576041 /*log_2(7/4)*/,
            0.45943161863 /*log_2(11/8)*/,
            0.70043971814 /*log_2(13/8)*/,
            0.95419631038 /*log_2(31/16)*/,
        };

        // Fake semitones map for the expander.
        //
        static constexpr int x_semitones[] = {
            0 /*Off*/,
            0 /*octave*/,
            7 /*pefect fifth*/,
            4 /*major third*/,
            5 /*perfect fourth*/,
            3 /*minor third*/,
            2 /*whole tone*/,
            1 /*half step*/,
            0 /*7/4*/,
            0 /*11/8*/,
            0 /*13/8*/,
            0 /*31/16*/,
        };

        static constexpr size_t x_end12EDOLikeIx = 8;
        
        rack::engine::Param* m_intervalKnob = nullptr;
        rack::engine::Input* m_intervalCV = nullptr;

        Interval m_intervalKnobValue;
        bool m_intervalKnobChanged;

        float m_intervalCVValue;
        bool m_intervalCVChanged;
        
        float m_value;
        bool m_changed;
        
        bool* m_12EDOMode;

        Interval GetInterval();

        int GetSemitones()
        {
            return x_semitones[static_cast<int>(m_intervalKnobValue)];
        }

        bool Is12EDOLike()
        {
            // More sophisticated analysis could work here, but honestly if the user provides an analog
            // interval just have the expander display in cents (that is, say its not 12-EDO-like).
            //
            return m_intervalCVValue == 0 &&
                static_cast<size_t>(m_intervalKnobValue) < x_end12EDOLikeIx;
        }

        void Preprocess(PreprocessState& preprocessState)
        {
            m_intervalKnobChanged = false;
            m_intervalCVChanged = false;
            m_changed = false;
            
            Interval newInterval = GetInterval();
            if (newInterval != m_intervalKnobValue)
            {
                m_intervalKnobChanged = true;
                m_intervalKnobValue = newInterval;
            }

            float newIntervalCVValue = m_intervalCV->getVoltage();
            if (newIntervalCVValue != m_intervalCVValue)
            {
                m_intervalCVChanged = true;
                m_intervalCVValue = newIntervalCVValue;
            }

            float newValue = x_voltages[static_cast<size_t>(m_intervalKnobValue)] + m_intervalCVValue;
            if (*m_12EDOMode)
            {
                newValue = static_cast<float>(static_cast<int>(newValue * 12 + 0.5)) / 12.0;
            }

            if (newValue != m_value)
            {
                m_changed = true;
                m_value = newValue;
                preprocessState.m_invalidateCache = true;
            }
        }

        void Init(
            rack::engine::Param* intervalKnob,
            rack::engine::Input* intervalCV,
            bool* twelveEDOMode)
        {
            m_intervalKnob = intervalKnob;
            m_intervalCV = intervalCV;
            m_12EDOMode = twelveEDOMode;
            m_intervalKnobValue = Interval::Off;
            m_intervalKnobChanged = false;
            m_intervalCVValue = 0;
            m_intervalCVChanged = false;
            m_value = 0;
            m_changed = false;
        }

        void Randomize(int level);
    };

    struct MatrixEvalResult
    {
        MatrixEvalResult()
        {
            using namespace LameJuisConstants;
            
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                m_high[i] = 0;
            }
        }

        void Clear()
        {
            memset(m_high, 0, LameJuisConstants::x_numAccumulators);
        }

        float ComputePitch(Accumulator* accumulators) const
        {
            using namespace LameJuisConstants;
            
            float result = 0;
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                result += accumulators[i].m_value * m_high[i];
            }

            return result;
        }

        bool operator==(const MatrixEvalResult& other)
        {
            using namespace LameJuisConstants;
            
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                if (m_high[i] != other.m_high[i])
                {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const MatrixEvalResult& other)
        {
            return !(*this == other);
        }
        
        uint8_t m_high[LameJuisConstants::x_numAccumulators];
    };

    void ClearCaches()
    {
        using namespace LameJuisConstants;
        memset(m_isEvaluated, 0, sizeof(m_isEvaluated));
        ClearOutputCaches();
    }

    void ClearOutputCaches()
    {
        using namespace LameJuisConstants;
        
        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_outputs[i].ClearAllCaches();
        }
    }

    struct MatrixEvalResultWithPitch
    {
        MatrixEvalResultWithPitch()
            : m_pitch(0)
        {
        }

        MatrixEvalResultWithPitch(
            const MatrixEvalResult& result,
            Accumulator* accumulators)
            : m_result(result)
            , m_pitch(result.ComputePitch(accumulators))
        {
        }

        bool operator<(const MatrixEvalResultWithPitch& other) const
        {
            return m_pitch < other.m_pitch;
        }
        
        MatrixEvalResult m_result;
        float m_pitch;
    };

    MatrixEvalResultWithPitch EvalMatrix(InputVector inputVector);

    struct InputVectorIterator
    {
        uint8_t m_ordinal = 0;
        InputVector m_coMuteVector;
        size_t m_coMuteSize = 0;
        InputVector m_defaultVector;
        size_t m_forwardingIndices[LameJuisConstants::x_numInputs];

        InputVectorIterator(InputVector coMuteVector, InputVector defaultVector);

        InputVector Get();
        void Next();
        bool Done();
    };
    
    struct CoMuteSwitch
    {        
        void Init(
            rack::engine::Param* swtch,
            rack::engine::Light* light)
        {
            m_switch = swtch;
            m_light = light;
            m_value = false;
            m_changed = false;
            m_light->setBrightness(1.f);
        }
        
        bool IsCoMuted()
        {
            return m_switch->getValue() < 0.5;
        }

        void Preprocess(PreprocessState& preprocessState)
        {
            bool newValue = IsCoMuted();
            if (m_value != newValue)
            {
                m_light->setBrightness(newValue ? 0.f : 1.f);
                m_value = newValue;
                m_changed = true;
            }
            else
            {
                m_changed = false;
            }
        }

        bool m_value;
        bool m_changed;
        
        rack::engine::Param* m_switch = nullptr;
        rack::engine::Light* m_light = nullptr;
    };

    struct CoMuteState
    {
        InputVector GetCoMuteVector()
        {
            using namespace LameJuisConstants;
            
            InputVector result;
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                result.Set(i, m_switches[i].m_value);
            }

            return result;
        }

        void Init(
            rack::engine::Param* percentileKnob,
            rack::engine::Input* percentileCV)
        {
            m_percentileKnob = percentileKnob;
            m_percentileCV = percentileCV;
            m_polyChans = 0;
            m_chansChanged = false;
            m_comuteChanged = false;
        }

        size_t GetPolyChans()
        {
            return std::max<size_t>(m_percentileCV->getChannels(), 1);
        }

        float GetPercentile(size_t chan)
        {
            float percentile = m_percentileCV->getVoltage(chan);
            float result = m_percentileKnob->getValue() + percentile / 5.0;
            result = std::min(result, 1.f);
            result = std::max(result, 0.f);
            return result;
        }

        void Preprocess(size_t outputId, PreprocessState& preprocessState)
        {
            using namespace LameJuisConstants;

            m_comuteChanged = false;
            
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_switches[i].Preprocess(preprocessState);
                if (m_switches[i].m_changed)
                {
                    m_comuteChanged = true;
                }
            }
            
            if (GetPolyChans() != m_polyChans)
            {
                m_polyChans = GetPolyChans();
                m_chansChanged = true;
            }
            else
            {
                m_chansChanged = false;
            }
            
            for (size_t i = 0; i < GetPolyChans(); ++i)
            {
                float newPercentile = GetPercentile(i);
                if (m_percentileCVValue[i] != newPercentile)
                {
                    m_percentileCVValue[i] = newPercentile;
                    m_percentileCVChanged[i] = true;
                    preprocessState.m_recomputeVoice[outputId][i] = true;
                }
                else
                {
                    m_percentileCVChanged[i] = false;
                }
            }
        }

        void RandomizeCoMutes(int level);
        void RandomizePercentiles();
        
        CoMuteSwitch m_switches[LameJuisConstants::x_numInputs];
        rack::engine::Param* m_percentileKnob = nullptr;
        rack::engine::Input* m_percentileCV = nullptr;
        float m_percentileCVValue[LameJuisConstants::x_maxPoly];
        bool m_percentileCVChanged[LameJuisConstants::x_maxPoly];
        size_t m_polyChans;
        bool m_chansChanged;
        bool m_comuteChanged;
    };

    struct Output
    {
        struct CacheForSingleInputVector
        {
            CacheForSingleInputVector() :
                m_isEvaluated(false)
            {
            }
            
            void ClearCache()
            {
                m_isEvaluated = false;
            }
            
            MatrixEvalResultWithPitch ComputePitch(
                LameJuis* matrix,
                LameJuis::Output* output,
                LameJuis::InputVector defaultVector,
                float percentile);
            
            MatrixEvalResultWithPitch m_cachedResults[1 << LameJuisConstants::x_numInputs];
            size_t m_numResults;
            bool m_isEvaluated;
        };
        
        rack::engine::Output* m_mainOut = nullptr;
        rack::engine::Output* m_triggerOut = nullptr;
        rack::engine::Light* m_triggerLight = nullptr;
        rack::dsp::PulseGenerator m_pulseGen[LameJuisConstants::x_maxPoly];
        MatrixEvalResultWithPitch m_pitch[LameJuisConstants::x_maxPoly];
        CoMuteState m_coMuteState;

        CacheForSingleInputVector m_outputCaches[1 << LameJuisConstants::x_numInputs];

        size_t GetPolyChans()
        {
            return m_coMuteState.m_polyChans;
        }
        
        void ClearAllCaches()
        {
            using namespace LameJuisConstants;
            for (size_t i = 0; i < (1 << LameJuisConstants::x_numInputs); ++i)
            {
                m_outputCaches[i].ClearCache();
            }
        }

        void Preprocess(size_t outputId, PreprocessState& preprocessState)
        {
            m_coMuteState.Preprocess(outputId, preprocessState);
            if (m_coMuteState.m_comuteChanged)
            {
                ClearAllCaches();
            }
        }

        MatrixEvalResultWithPitch ComputePitch(LameJuis* matrix, InputVector defaultVector, size_t chan);
       
        void SetPitch(MatrixEvalResultWithPitch pitch, float dt, size_t chan)
        {
            bool changedThisFrame = pitch.m_result != m_pitch[chan].m_result &&
                pitch.m_pitch != m_pitch[chan].m_pitch;
            m_pitch[chan] = pitch;
            m_mainOut->setVoltage(pitch.m_pitch, chan);

            if (changedThisFrame)
            {
                m_pulseGen[chan].trigger(0.01);
            }
        }

        size_t SetPolyChans()
        {
            size_t chans = GetPolyChans();
            m_coMuteState.m_polyChans = chans;
            m_mainOut->setChannels(chans);
            m_triggerOut->setChannels(chans);
            return chans;
        }
        
        void ProcessTriggers(float dt)
        {
            bool anyTrig = false;
            for (size_t i = 0; i < m_coMuteState.m_polyChans; ++i)
            {
                bool trig = m_pulseGen[i].process(dt);
                m_triggerOut->setVoltage(trig ? 5.f : 0.f, i);
                anyTrig = anyTrig || trig;
            }

            m_triggerLight->setBrightness(anyTrig ? 1.f : 0.f);
        }
        
        InputVectorIterator GetInputVectorIterator(InputVector defaultVector)
        {
            return InputVectorIterator(m_coMuteState.GetCoMuteVector(), defaultVector);
        }
        
        void Init(
            rack::engine::Output* mainOut,
            rack::engine::Output* triggerOut,
            rack::engine::Light* triggerLight,
            rack::engine::Param* percentileKnob,
            rack::engine::Input* percentileCV)
        {
            m_mainOut = mainOut;
            m_triggerOut = triggerOut;
            m_triggerLight = triggerLight;
            m_coMuteState.Init(percentileKnob, percentileCV);
        }

        void RandomizeCoMutes(int level)
        {
            m_coMuteState.RandomizeCoMutes(level);
        }

        void RandomizePercentiles()
        {
            m_coMuteState.RandomizePercentiles();
        }

        bool IsPlugged()
        {
            return m_mainOut->isConnected() || m_triggerOut->isConnected();
        }
    };

    void ProcessReset();
    void ProcessOperations(PreprocessState& preprocessState, InputVector defaultVector);
    void ProcessOutputs(PreprocessState& preprocessState, InputVector defaultVector, float dt);

    void ProcessTriggers(float dt)
    {
        using namespace LameJuisConstants;
        
        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_outputs[i].ProcessTriggers(dt);
        }
    }
            

    void SendExpanderMessage(LatticeExpanderMessage msg)
    {
        using namespace LameJuisConstants;           

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            msg.m_intervalSemitones[i] = m_accumulators[i].GetSemitones();
            msg.m_intervalVoltages[i] = m_accumulators[i].m_value;
            msg.m_is12EDOLike[i] = m_accumulators[i].Is12EDOLike();
        }
        
        if (rightExpander.module && rightExpander.module->model == modelLatticeExpander)
        {
            *static_cast<LatticeExpanderMessage*>(rightExpander.module->leftExpander.producerMessage) = msg;
            rightExpander.module->leftExpander.messageFlipRequested = true;
        }
    }
    
	LameJuis();

    ~LameJuis()
    {
    }

    void RandomizeMatrix(int level);
    void RandomizeIntervals(int level);
    void RandomizeCoMutes(int level);
    void RandomizePercentiles();

    void Preprocess(PreprocessState& preprocessState)
    {
        using namespace LameJuisConstants;

        ProcessReset();
        
        for (size_t i = 0; i < x_numInputs; ++i)
        {
            m_inputs[i].Preprocess(i, i > 0 ? &m_inputs[i - 1] : nullptr, preprocessState);
        }

        for (size_t i = 0; i < x_numOperations; ++i)
        {
            m_operations[i].Preprocess(i, preprocessState);
        }

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_accumulators[i].Preprocess(preprocessState);
        }

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_outputs[i].Preprocess(i, preprocessState);
        }

        for (size_t i = 0; i < x_numInputs; ++i)
        {
            if (m_inputs[i].m_changed)
            {
                for (size_t j = 0; j < x_numAccumulators; ++j)
                {
                    if (!m_outputs[j].m_coMuteState.m_switches[i].m_value)
                    {
                        preprocessState.SetRecomputeVoice(j);
                    }
                }
            }
        }

        if (preprocessState.m_invalidateCache)
        {
            ClearCaches();
        }
    }
    
    void process(const ProcessArgs& args) override;

	json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
    
    MatrixEvalResult m_evalResults[1 << LameJuisConstants::x_numInputs];
    bool m_isEvaluated[1 << LameJuisConstants::x_numInputs];

    bool m_12EDOMode;
    bool m_timeQuantizeMode;
    bool m_firstStep;

    rack::engine::Input* m_resetPort = nullptr;
    rack::dsp::TSchmittTrigger<float> m_resetSchmittTrigger;
    bool m_reset;
    
    Input m_inputs[LameJuisConstants::x_numInputs];
    LogicOperation m_operations[LameJuisConstants::x_numOperations];
    Accumulator m_accumulators[LameJuisConstants::x_numAccumulators];
    Output m_outputs[LameJuisConstants::x_numAccumulators];
};
