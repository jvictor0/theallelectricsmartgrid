#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"
#include "SmartGrid.hpp"

struct PercentileSequencerInternal
{
    static constexpr size_t x_numSteps = 8;
    static constexpr size_t x_numClocks = 7;
    static constexpr size_t x_numTrios = 3;

    struct StepSelector
    {
        size_t m_curStep;
        bool m_resetRequested;

        StepSelector()
            : m_curStep(0)
            , m_resetRequested(false)
        {
        }            

        struct Input
        {
            size_t m_seqLen;
            bool m_clock;
            bool m_reset;

            Input()
                : m_seqLen(x_numSteps)
                , m_clock(false)
                , m_reset(false)
            {
            }
        };

        void Process(Input& input)
        {
            if (input.m_reset)
            {
                m_resetRequested = true;
            }

            if (input.m_clock)
            {
                if (m_resetRequested)
                {
                    m_curStep = 0;
                    m_resetRequested = false;
                }
                else
                {
                    m_curStep = (m_curStep + 1) % (input.m_seqLen + 1);
                }
            }
        }
    };

    struct TrioSequencer
    {
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

            Input()
                : m_sequenceScale(1)
                , m_tenorDistance(1)
            {
                for (size_t i = 0; i < x_numSteps; ++i)
                {
                    m_sequence[i] = 0;
                }
            }

            void SetReset(bool reset)
            {
                m_selectorIn[0].m_reset = reset;
                m_selectorIn[1].m_reset = reset;
            }

            void SetClock(bool clock)
            {
                m_selectorIn[0].m_clock = clock;
                m_selectorIn[1].m_clock = clock;
            }
        };

        void Process(Input& input)
        {
            for (size_t i = 0; i < 2; ++i)
            {                
                m_stepSelector[i].Process(input.m_selectorIn[i]);
                m_advanced[i] = input.m_selectorIn[i].m_clock;                
            }

            if (m_advanced[0] || m_advanced[1])
            {
                float tenorMax = input.m_tenorDistance;
                float tenorMin = tenorMax * 2.0 / 3.0;
                float bassMax = tenorMax / 3.0;

                if (m_advanced[0])
                {
                    float bassVal = input.m_sequence[m_stepSelector[0].m_curStep] * input.m_sequenceScale;
                    m_bass = bassVal * bassMax;
                }
                
                if (m_advanced[1])
                {
                    float tenorVal = input.m_sequence[m_stepSelector[1].m_curStep] - 1;
                    m_tenor = tenorMax + (tenorMax - tenorMin) * tenorVal * input.m_sequenceScale;
                }

                float altoSeq = (input.m_sequence[m_stepSelector[0].m_curStep] + input.m_sequence[m_stepSelector[1].m_curStep]) / 2 - 0.5;                    
                m_alto = (bassMax + tenorMin) / 2 + (tenorMin - bassMax) * altoSeq * input.m_sequenceScale;
            }
        }

        void ProcessNoClock(Input& input)
        {
            m_bass = 0;                
            m_tenor = input.m_tenorDistance;
            m_alto = input.m_tenorDistance / 2;
        }

        float& GetVal(size_t voice)
        {
            switch (voice)
            {
                case 0: return m_bass;
                case 1: return m_alto;
                case 2: return m_tenor;
                default: return m_bass;
            }
        }
    };

    struct Input
    {
        TrioSequencer::Input m_input[x_numTrios];
        bool m_clocks[x_numClocks];
        int m_clockSelect[x_numTrios];
        int m_resetSelect[x_numTrios];
        bool m_externalReset[x_numTrios];

        Input()
        {
            for (size_t i = 0; i < x_numClocks; ++i)
            {
                m_clocks[i] = false;
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_clockSelect[i] = 0;
                m_resetSelect[i] = -1;
            }
        }
    };

    TrioSequencer m_sequencer[x_numTrios];

    void Process(Input& input)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            if (input.m_externalReset[i])
            {
                input.m_input[i].SetReset(true);
                input.m_externalReset[i] = false;
            }
            else if (input.m_resetSelect[i] >= 0)
            {
                input.m_input[i].SetReset(input.m_clocks[input.m_resetSelect[i]]);
            }
            else
            {
                input.m_input[i].SetReset(false);
            }

            if (input.m_clockSelect[i] >= 0)
            {
                input.m_input[i].SetClock(input.m_clocks[input.m_clockSelect[i]]);
            }
            else
            {
                input.m_input[i].SetClock(false);
                if (input.m_clocks[0])
                {
                    m_sequencer[i].ProcessNoClock(input.m_input[i]);
                }
            }
            
            m_sequencer[i].Process(input.m_input[i]);
        }
    }

    float& GetVal(size_t seq, size_t voice)
    {
        return m_sequencer[seq].GetVal(voice);
    }
};


// struct PercentileSequencer : Module
// {
//     static constexpr size_t x_maxPoly = 16;
//     static constexpr size_t x_numSteps = 8;
//     static constexpr size_t x_numSequences = 3;
//     static constexpr size_t x_numScales = 2;
//     static constexpr size_t x_numClocks = 1;
//     static constexpr size_t x_numResets = 1;

//     static constexpr float x_timeToCheckButtons = 0.05;    

//     PercentileSequencerInternal m_sequencer;
//     PercentileSequencerInternal::Input m_state;
//     float m_timeToCheck;

//     void process(const ProcessArgs &args) override
//     {
//         if (m_timeToCheck < 0)
//         {
//             m_timeToCheck = x_timeToCheckButtons;
//         }

//         m_timeToCheck -= args.sampleTime;

//     }
    
//     size_t SequencerValueId(size_t seq)
//     {
//         return seq;
//     }

//     size_t StepButtonId(size_t seq)
//     {
//         return SequencerValueId(x_numSequences) + seq;
//     }

//     size_t ScaleId(size_t which)
//     {
//         return StepButtonId(x_numSequences) + which;
//     }

//     size_t ClockId()
//     {
//         return ScaleId(2);
//     }

//     size_t ResetId()
//     {
//         return ClockId() + 1;
//     }
//     size_t NumInputs()
//     {
//         return ResetId() + 1;
//     }

//     float GetSequencerValue(size_t seq, size_t step)
//     {
//         return inputs[SequencerValueId(seq)].getVoltage(step);
//     }

//     float GetStepButton(size_t seq, size_t voice, size_t step)
//     {
//         return inputs[StepButtonId(seq)].getVoltage(8 * voice + step);
//     }

//     float GetScale(size_t seq, size_t which)
//     {
//         return inputs[ScaleId(which)].getVoltage(seq);
//     }

//     float GetClock(size_t seq)
//     {
//         return inputs[ClockId()].getVoltage(seq);
//     }

//     float GetReset(size_t seq)
//     {
//         return inputs[ResetId()].getVoltage(seq);
//     }

//     size_t OutputId(size_t seq)
//     {
//         return seq;
//     }

//     size_t LightId(size_t seq)
//     {
//         return OutputId(x_numSequences) + seq;
//     }

//     void SetOutput(size_t seq, size_t voice, float val)
//     {
//         outputs[OutputId(seq)].setVoltage(val, voice);
//     }
//     void SetLight(size_t seq, size_t voice, size_t step, float val)
//     {
//         outputs[LightId(seq)].setVoltage(val, x_numSteps * voice + step);
//     }
    
//     PercentileSequencer()
//     {
//         m_timeToCheck = -1;
//         config(0, NumInputs(), 6, 0);

//         for (size_t i = 0; i < x_numSequences; ++i)
//         {
//             configInput(SequencerValueId(i), ("Sequece In " + std::to_string(i)).c_str());
//             configInput(StepButtonId(i), ("Steps Button " + std::to_string(i)).c_str());
//             configOutput(OutputId(i),("Sequence Output " + std::to_string(i)).c_str());
//             configOutput(LightId(i),("Sequence Light " + std::to_string(i)).c_str());
//         }
        
//         configInput(ScaleId(0), "Seq Scale");
//         configInput(ScaleId(1), "Seq Distance");
//         configInput(ClockId(), "Clock in");
//         configInput(ResetId(), "Reset in");
//     }    
// };

