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
        }
        
        SwitchVal GetSwitchVal();
        
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
            Majority = 4
        };
        
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

        rack::engine::Param* m_switch = nullptr;
        rack::engine::Param* m_operatorKnob = nullptr;
        rack::engine::Light* m_light = nullptr;
        rack::engine::Output* m_output = nullptr;
        InputVector m_active;
        InputVector m_inverted;

        LameJuis::MatrixElement m_elements[LameJuisConstants::x_numInputs];
    };

    struct Accumulator
    {
        enum class Interval
        {
            Off = 0,
            HalfStep = 1,
            WholeStep = 2,
            MinorThird = 3,
            MajorThird = 4,
            PerfectFourth = 5,
            PerfectFifth = 6,
            MinorSeventh = 7,
            Octave = 8
        };

        static constexpr float x_voltages[] = {
            0 /*Off*/,
            0.09310940439 /*half step = log_2(16/15)*/,
            0.16992500144231237/*whole tone = log_2(9/8)*/,
            0.2630344058337938 /*minor third = log_2(6/5)*/,
            0.32192809488736235 /*major third = log_2(5/4)*/,
            0.4150374992788437 /*perfect fourth = log_2(4/3)*/,
            0.5849625007211562 /*pefect fifth = log_2(3/2)*/,
            0.8073549220576041 /*minor seventh = log_2(7/4)*/,
            1.0 /*octave = log_2(2)*/
        };

        // Fake semitones map for the expander.
        //
        static constexpr int x_semitones[] = {
            0 /*Off*/,
            1 /*half step*/,
            2 /*whole tone*/,
            3 /*minor third*/,
            4 /*major third = log_2(5/4)*/,
            5 /*perfect fourth*/,
            7 /*pefect fifth*/,
            10 /*minor seventh*/,
            0 /*octave*/
        };
        
        rack::engine::Param* m_intervalKnob = nullptr;
        rack::engine::Input* m_intervalCV = nullptr;

        Interval GetInterval();

        int GetSemitones()
        {
            return x_semitones[static_cast<int>(GetInterval())];
        }

        float GetPitch();

        void Init(
            rack::engine::Param* intervalKnob,
            rack::engine::Input* intervalCV)
        {
            m_intervalKnob = intervalKnob;
            m_intervalCV = intervalCV;
        }
    };

    struct MatrixEvalResult
    {
        MatrixEvalResult()
        {
            using namespace LameJuisConstants;
            
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                m_high[i] = 0;
                m_total[i] = 0;
            }
        }

        void SetPitch(Accumulator* accumulators)
        {
            using namespace LameJuisConstants;
            
            float result = 0;
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                result += accumulators[i].GetPitch() * m_high[i];
            }

            m_pitch = result;
        }

        bool operator<(const MatrixEvalResult& other) const
        {
            return m_pitch < other.m_pitch;
        }
        
        uint8_t m_high[LameJuisConstants::x_numAccumulators];
        uint8_t m_total[LameJuisConstants::x_numAccumulators];
        float m_pitch;
    };

    MatrixEvalResult EvalMatrix(InputVector inputVector);

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
        
        CoMuteSwitch m_switches[LameJuisConstants::x_numInputs];
        rack::engine::Param* m_percentileKnob = nullptr;
        rack::engine::Input* m_percentileCV = nullptr;
    };

    struct Output
    {
        rack::engine::Output* m_mainOut = nullptr;
        rack::engine::Output* m_triggerOut = nullptr;
        rack::engine::Light* m_triggerLight = nullptr;
        rack::dsp::PulseGenerator m_pulseGen;
        float m_pitch = 0.0;
        CoMuteState m_coMuteState;

        MatrixEvalResult ComputePitch(LameJuis* matrix, InputVector defaultVector);
       
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
    };

    InputVector ProcessInputs();
    void ProcessOperations(InputVector defaultVector);
    void ProcessOutputs(InputVector defaultVector, float dt);

    void SendExpanderMessage(LatticeExpanderMessage msg)
    {
        using namespace LameJuisConstants;           

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            msg.m_intervalSemitones[i] = m_accumulators[i].GetSemitones();
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

    void process(const ProcessArgs& args) override;

    Input m_inputs[LameJuisConstants::x_numInputs];
    LogicOperation m_operations[LameJuisConstants::x_numOperations];
    Accumulator m_accumulators[LameJuisConstants::x_numAccumulators];
    Output m_outputs[LameJuisConstants::x_numAccumulators];
};
