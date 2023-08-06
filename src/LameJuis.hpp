#pragma once
#include "plugin.hpp"
#include <cstddef>
#include "LameJuisConstants.hpp"
#include "LatticeExpander.hpp"

struct LameJuis : Module
{
    LatticeExpanderMessage m_rightMessages[2][1];
    
    struct Input
    {
        rack::engine::Input* m_port = nullptr;
        rack::dsp::TSchmittTrigger<float> m_schmittTrigger;
        rack::engine::Light* m_light = nullptr;
        bool m_value = false;
        uint8_t m_counter = 0;

        void Init(
            rack::engine::Input* port,
            rack::engine::Light* light)
        {
            m_port = port;
            m_light = light;
            m_counter = 0;
        }
        
        void SetValue(Input* prev);
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
        }
        
        SwitchVal GetSwitchVal();

        SwitchVal m_value;

        bool HasChanged()
        {
            SwitchVal newVal = GetSwitchVal();
            if (m_value != newVal)
            {
                m_value = newVal;
                return true;
            }

            return false;
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
            NumOperations = 5,
        };

        static std::vector<std::string> GetLogicNames()
        {
            return std::vector<std::string>({
                    "Or",
                    "And",
                    "Xor",
                    "At Least Two",
                    "Majority"
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
                MatrixElement::SwitchVal switchVal = m_elements[i].GetSwitchVal();
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
            return x_numAccumulators - static_cast<size_t>(GetSwitchVal()) - 1;
        }

        bool AnySwitchChanged()
        {
            using namespace LameJuisConstants;
            bool result = false;
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                if (m_elements[i].HasChanged())
                {
                    result = true;

                    // Don't break, need to set 'm_value' on the entire matrix.
                    //
                }              
            }

            return result;
        }

        bool AnyThingChanged()
        {
            bool anyChanged = false;
            if (AnySwitchChanged())
            {
                anyChanged = true;
            }

            Operator op = GetOperator();
            if (op != m_operatorValue)
            {
                m_operatorValue = op;
                anyChanged = true;
            }

            SwitchVal sv = GetSwitchVal();
            if (sv != m_switchValue)
            {
                m_switchValue = sv;
                anyChanged = true;
            }

            return anyChanged;
        }

        void Randomize(int level);

        rack::engine::Param* m_switch = nullptr;
        rack::engine::Param* m_operatorKnob = nullptr;
        rack::engine::Light* m_light = nullptr;
        rack::engine::Output* m_output = nullptr;
        InputVector m_active;
        InputVector m_inverted;

        Operator m_operatorValue;
        SwitchVal m_switchValue;

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

        float m_value;

        bool* m_12EDOMode;

        Interval GetInterval();

        int GetSemitones()
        {
            return x_semitones[static_cast<int>(GetInterval())];
        }

        bool Is12EDOLike()
        {
            // More sophisticated analysis could work here, but honestly if the user provides an analog
            // interval just have the expander display in cents (that is, say its not 12-EDO-like).
            //
            return m_intervalCV->getVoltage() == 0 &&
                static_cast<size_t>(GetInterval()) < x_end12EDOLikeIx;
        }

        float GetPitch();

        bool HasChanged()
        {
            float newValue = GetPitch();
            if (newValue != m_value)
            {
                m_value = newValue;
                return true;
            }

            return false;
        }

        void Init(
            rack::engine::Param* intervalKnob,
            rack::engine::Input* intervalCV,
            bool* twelveEDOMode)
        {
            m_intervalKnob = intervalKnob;
            m_intervalCV = intervalCV;
            m_12EDOMode = twelveEDOMode;
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
                result += accumulators[i].GetPitch() * m_high[i];
            }

            return result;
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
        void Init(rack::engine::Param* swtch)
        {
            m_switch = swtch;
        }
        
        bool IsCoMuted()
        {
            return m_switch->getValue() < 0.5;
        }

        bool HasChanged()
        {
            bool newValue = IsCoMuted();
            if (m_value != newValue)
            {
                m_value = newValue;
                return true;
            }

            return false;
        }

        bool m_value;
        
        rack::engine::Param* m_switch = nullptr;
    };

    struct CoMuteState
    {
        InputVector GetCoMuteVector()
        {
            using namespace LameJuisConstants;
            
            InputVector result;
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                result.Set(i, m_switches[i].IsCoMuted());
            }

            return result;
        }

        void Init(
            rack::engine::Param* percentileKnob,
            rack::engine::Input* percentileCV)
        {
            m_percentileKnob = percentileKnob;
            m_percentileCV = percentileCV;
        }

        float GetPercentile()
        {
            float result = m_percentileKnob->getValue() + m_percentileCV->getVoltage() / 5.0;
            result = std::min(result, 1.f);
            result = std::max(result, 0.f);
            return result;
        }

        bool HasChanged()
        {
            using namespace LameJuisConstants;
            
            bool anyChanged = false;
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                if (m_switches[i].HasChanged())
                {
                    anyChanged = true;
                }
            }

            return anyChanged;
        }

        void RandomizeCoMutes(int level);
        void RandomizePercentiles();
        
        CoMuteSwitch m_switches[LameJuisConstants::x_numInputs];
        rack::engine::Param* m_percentileKnob = nullptr;
        rack::engine::Input* m_percentileCV = nullptr;
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
        rack::dsp::PulseGenerator m_pulseGen;
        float m_pitch = 0.0;
        CoMuteState m_coMuteState;

        CacheForSingleInputVector m_outputCaches[1 << LameJuisConstants::x_numInputs];

        void ClearAllCaches()
        {
            using namespace LameJuisConstants;
            for (size_t i = 0; i < (1 << LameJuisConstants::x_numInputs); ++i)
            {
                m_outputCaches[i].ClearCache();
            }
        }

        bool HasCoMutesChanged()
        {
            return m_coMuteState.HasChanged();
        }

        MatrixEvalResultWithPitch ComputePitch(LameJuis* matrix, InputVector defaultVector);
       
        void SetPitch(float pitch, float dt)
        {
            bool changedThisFrame = (pitch != m_pitch);
            m_pitch = pitch;
            m_mainOut->setVoltage(pitch);

            if (changedThisFrame)
            {
                m_pulseGen.trigger(0.01);
            }

            bool trig = m_pulseGen.process(dt);
            m_triggerOut->setVoltage(trig ? 5.f : 0.f);
            m_triggerLight->setBrightness(trig ? 1.f : 0.f);
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
    };

    void CheckMatrixChangedAndInvalidateCache();
    InputVector ProcessInputs();
    void ProcessOperations(InputVector defaultVector);
    void ProcessOutputs(InputVector defaultVector, float dt);

    void SendExpanderMessage(LatticeExpanderMessage msg)
    {
        using namespace LameJuisConstants;           

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            msg.m_intervalSemitones[i] = m_accumulators[i].GetSemitones();
            msg.m_intervalVoltages[i] = m_accumulators[i].GetPitch();
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

    void process(const ProcessArgs& args) override;

	json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
    
    MatrixEvalResult m_evalResults[1 << LameJuisConstants::x_numInputs];
    bool m_isEvaluated[1 << LameJuisConstants::x_numInputs];

    bool m_12EDOMode;
    
    Input m_inputs[LameJuisConstants::x_numInputs];
    LogicOperation m_operations[LameJuisConstants::x_numOperations];
    Accumulator m_accumulators[LameJuisConstants::x_numAccumulators];
    Output m_outputs[LameJuisConstants::x_numAccumulators];
};
