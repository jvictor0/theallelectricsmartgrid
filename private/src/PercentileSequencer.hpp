#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "BlinkLight.hpp"
#include "Trig.hpp"

struct PercentileSequencerInternal
{
    static constexpr size_t x_numSteps = 8;

    struct StepSelector
    {
        RadialButtonsMultiCC<x_numSteps> m_buttons;
        size_t m_curStep;
        Trig m_clock;
        Trig m_reset;
        bool m_resetRequested;

        StepSelector() :
            m_curStep(0)
        {
        }            

        struct Input
        {
            RadialButtonsMultiCC<x_numSteps>::Input m_buttonsIn;
            float m_clock;
            float m_reset;
        };

        bool Process(float dt, Input& input)
        {
            bool clock = m_clock.Process(input.m_clock);
            bool reset = m_reset.Process(input.m_reset);
            if (reset)
            {
                m_resetRequested = true;
            }

            if (m_buttons.Process(dt, input.m_buttonsIn))
            {
                m_resetRequested = true;
            }

            if (clock)
            {
                m_buttons.GetBlock(m_curStep).SetLight(false);
                if (m_resetRequested)
                {
                    m_curStep = 0;
                    m_resetRequested = false;
                }
                else
                {
                    m_curStep = (m_curStep + 1) % (m_buttons.m_upIx + 1);
                }

                m_buttons.GetBlock(m_curStep).SetLight(true);                
                
                return true;
            }

            return false;
        }

        size_t GetNumSteps()
        {
            return m_buttons.m_upIx;
        }

        void SetNumSteps(size_t steps)
        {
            m_buttons.SetUpIx(steps);
        }

        float GetLight(size_t step)
        {
            return m_buttons.GetBlock(step).GetLight(false);
        }
    };

    struct TrioSequencer
    {
        static constexpr float x_maxVal = 5;
        StepSelector m_stepSelector[2];
        float m_bass;
        float m_alto;
        float m_tenor;
        bool m_advanced[2];

        struct Input
        {
            StepSelector::Input m_selectorIn[2];
            float m_sequence[x_numSteps];
            float m_sequenceScale;
            float m_tenorDistance;
        };
        
        void Process(float dt, Input& input)
        {
            for (size_t i = 0; i < 2; ++i)
            {
                m_advanced[i] = m_stepSelector[i].Process(dt, input.m_selectorIn[i]);
            }

            if (m_advanced[0] || m_advanced[1])
            {
                float tenorMax = x_maxVal * input.m_tenorDistance / 10;
                float tenorMin = tenorMax * 2.0 / 3.0;
                float bassMax = tenorMax / 3.0;

                if (m_advanced[0])
                {
                    float bassVal = input.m_sequence[m_stepSelector[0].m_curStep] * input.m_sequenceScale / (10 * x_maxVal);
                    m_bass = bassVal * bassMax;
                }
                
                if (m_advanced[1])
                {
                    float tenorVal = input.m_sequence[m_stepSelector[1].m_curStep] / x_maxVal - 1;
                    m_tenor = tenorMax + (tenorMax - tenorMin) * tenorVal * input.m_sequenceScale / 10;
                }

                float altoSeq = (input.m_sequence[m_stepSelector[0].m_curStep] + input.m_sequence[m_stepSelector[1].m_curStep]) / (2 * x_maxVal) - 0.5;                    
                m_alto = (bassMax + tenorMin) / 2 + (tenorMin - bassMax) * altoSeq * input.m_sequenceScale / 10;
            }
        }

        float GetLight(size_t voice, size_t step)
        {
            return m_stepSelector[voice].GetLight(step);
        }

        float GetVal(size_t voice)
        {
            switch (voice)
            {
                case 0: return m_bass;
                case 1: return m_alto;
                case 2: return m_tenor;
                default: return -1;
            }
        }

        size_t GetNumSteps(size_t voice)
        {
            return m_stepSelector[voice].GetNumSteps();
        }

        void SetNumSteps(size_t steps, size_t voice)
        {
            return m_stepSelector[voice].SetNumSteps(steps);
        }
    };

    struct Input
    {
        TrioSequencer::Input m_input[3];
    };

    TrioSequencer m_sequencer[3];

    void Process(float dt, Input& input)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            m_sequencer[i].Process(dt, input.m_input[i]);
        }
    }

    float GetVal(size_t seq, size_t voice)
    {
        return m_sequencer[seq].GetVal(voice);
    }

    float GetLight(size_t seq, size_t voice, size_t step)
    {
        return m_sequencer[seq].GetLight(voice, step);
    }

    size_t GetNumSteps(size_t seq, size_t voice)
    {
        return m_sequencer[seq].GetNumSteps(voice);
    }

    void SetNumSteps(size_t seq, size_t voice, size_t steps)
    {
        m_sequencer[seq].SetNumSteps(steps, voice);
    }
};

struct PercentileSequencer : Module
{
    static constexpr size_t x_maxPoly = 16;
    static constexpr size_t x_numSteps = 8;
    static constexpr size_t x_numSequences = 3;
    static constexpr size_t x_numScales = 2;
    static constexpr size_t x_numClocks = 1;
    static constexpr size_t x_numResets = 1;

    static constexpr float x_timeToCheckButtons = 0.05;    

    PercentileSequencerInternal m_sequencer;
    PercentileSequencerInternal::Input m_state;
    float m_timeToCheck;

    void SetClocksAndResets()
    {
        for (size_t i = 0; i < 3; ++i)
        {
            for (size_t j = 0; j < 2; ++j)
            {
                m_state.m_input[i].m_selectorIn[j].m_clock = GetClock(i);
                m_state.m_input[i].m_selectorIn[j].m_reset = GetReset(i);
            }            
        }
    }

    void SetOtherVals()
    {
        for (size_t i = 0; i < 3; ++i)
        {
            m_state.m_input[i].m_sequenceScale = GetScale(i, 0);
            m_state.m_input[i].m_tenorDistance = GetScale(i, 1);
            for (size_t j = 0; j < x_numSteps; ++j)
            {
                m_state.m_input[i].m_sequence[j] = GetSequencerValue(i, j);
            }

            for (size_t j = 0; j < 2; ++j)
            {
                for (size_t k = 0; k < x_numSteps; ++k)
                {
                    m_state.m_input[i].m_selectorIn[j].m_buttonsIn.m_values[k] = GetStepButton(i, j, k);
                }
            }

            outputs[OutputId(i)].setChannels(3);
            outputs[LightId(i)].setChannels(16);
        }
    }
    
    void process(const ProcessArgs &args) override
    {
        if (m_timeToCheck < 0)
        {
            SetOtherVals();
            m_timeToCheck = x_timeToCheckButtons;
        }

        SetClocksAndResets();
        
        m_timeToCheck -= args.sampleTime;

        m_sequencer.Process(args.sampleTime, m_state);

        for (size_t i = 0; i < x_numSequences; ++i)
        {
            for (size_t j = 0; j < 3; ++j)
            {
                SetOutput(i, j, m_sequencer.GetVal(i, j));
            }

            for (size_t j = 0; j < 2; ++j)
            {
                for (size_t k = 0; k < x_numSteps; ++k)
                {
                    SetLight(i, j, k, m_sequencer.GetLight(i, j, k));
                }
            }
        }
    }
    
    size_t SequencerValueId(size_t seq)
    {
        return seq;
    }

    size_t StepButtonId(size_t seq)
    {
        return SequencerValueId(x_numSequences) + seq;
    }

    size_t ScaleId(size_t which)
    {
        return StepButtonId(x_numSequences) + which;
    }

    size_t ClockId()
    {
        return ScaleId(2);
    }

    size_t ResetId()
    {
        return ClockId() + 1;
    }
    size_t NumInputs()
    {
        return ResetId() + 1;
    }

    float GetSequencerValue(size_t seq, size_t step)
    {
        return inputs[SequencerValueId(seq)].getVoltage(step);
    }

    float GetStepButton(size_t seq, size_t voice, size_t step)
    {
        return inputs[StepButtonId(seq)].getVoltage(8 * voice + step);
    }

    float GetScale(size_t seq, size_t which)
    {
        return inputs[ScaleId(which)].getVoltage(seq);
    }

    float GetClock(size_t seq)
    {
        return inputs[ClockId()].getVoltage(seq);
    }

    float GetReset(size_t seq)
    {
        return inputs[ResetId()].getVoltage(seq);
    }

    size_t OutputId(size_t seq)
    {
        return seq;
    }

    size_t LightId(size_t seq)
    {
        return OutputId(x_numSequences) + seq;
    }

    void SetOutput(size_t seq, size_t voice, float val)
    {
        outputs[OutputId(seq)].setVoltage(val, voice);
    }
    void SetLight(size_t seq, size_t voice, size_t step, float val)
    {
        outputs[LightId(seq)].setVoltage(val, x_numSteps * voice + step);
    }
    
    PercentileSequencer()
    {
        m_timeToCheck = -1;
        config(0, NumInputs(), 6, 0);

        for (size_t i = 0; i < x_numSequences; ++i)
        {
            configInput(SequencerValueId(i), ("Sequece In " + std::to_string(i)).c_str());
            configInput(StepButtonId(i), ("Steps Button " + std::to_string(i)).c_str());
            configOutput(OutputId(i),("Sequence Output " + std::to_string(i)).c_str());
            configOutput(LightId(i),("Sequence Light " + std::to_string(i)).c_str());
        }
        
        configInput(ScaleId(0), "Seq Scale");
        configInput(ScaleId(1), "Seq Distance");
        configInput(ClockId(), "Clock in");
        configInput(ResetId(), "Reset in");
    }
    
    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();
        for (size_t i = 0; i < x_numSequences; ++i)
        {
            for (size_t j = 0; j < 2; ++j)
            {                
                json_object_set_new(rootJ, ("num_steps_" + std::to_string(i) + "_" + std::to_string(j)).c_str(), json_real(m_sequencer.GetNumSteps(i, j)));
            }
        }
            
        return rootJ;
    }
    
    void dataFromJson(json_t* rootJ) override
    {
        for (size_t i = 0; i < x_numSequences; ++i)
        {                
            for (size_t j = 0; j < 2; ++j)
            {                
                json_t* val = json_object_get(rootJ, ("num_steps_" + std::to_string(i) + "_" + std::to_string(j)).c_str());
                if (val)
                {
                    m_sequencer.SetNumSteps(i, j, json_number_value(val));
                }
            }
        }
    }
};

struct PercentileSequencerWidget : ModuleWidget
{
    PercentileSequencerWidget(PercentileSequencer* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/PercentileSequencer.svg")));
        
        float rowOff = 25;
        float rowStart = 50;
        
        for (size_t i = 0; i < module->x_numSequences; ++i)
        {
            float rowPos = 100 + i * 30;
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart, rowPos), module, module->SequencerValueId(i)));
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart + rowOff, rowPos), module, module->StepButtonId(i)));
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 2 * rowOff, rowPos), module, module->OutputId(i)));
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 3 * rowOff, rowPos), module, module->LightId(i)));
        }


        size_t lastPos = 150 + module->x_numSequences * 30;
        
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 0.5 * rowOff, lastPos), module, module->ScaleId(0)));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 1.5 * rowOff, lastPos), module, module->ScaleId(1)));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 2.5 * rowOff, lastPos), module, module->ClockId()));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 3.5 * rowOff, lastPos), module, module->ResetId()));
    }
};

