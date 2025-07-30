#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"

struct MultiPhasorGateInternal
{
    static constexpr size_t x_maxPoly = 16;

    struct Input
    {
        bool m_trigs[x_maxPoly];
        float m_phasors[x_maxPoly];
        size_t m_numTrigs;
        size_t m_numPhasors;
        bool m_phasorSelector[x_maxPoly][x_maxPoly];
        float m_gateFrac[x_maxPoly];
        bool m_newTrigCanStart[x_maxPoly];
        bool m_mute[x_maxPoly];

        Input()
            : m_numTrigs(0)
            , m_numPhasors(0)
        {
            for (size_t i = 0; i < x_maxPoly; ++i)
            {
                m_newTrigCanStart[i] = false;
                m_trigs[i] = false;
                m_phasors[i] = 0;
                m_gateFrac[i] = 0.5;
                for (size_t j = 0; j < x_maxPoly; ++j)
                {
                    m_phasorSelector[i][j] = false;
                }

                m_mute[i] = false;
            }
        }
    };
    
    MultiPhasorGateInternal()
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_trig[i] = false;
            m_gate[i] = false;
            m_set[i] = false;
            m_preGate[i] = false;
            m_phasorOut[i] = 0;
            for (size_t j = 0; j < x_maxPoly; ++j)
            {
                m_phasorSelector[i][j] = false;
            }            
        }
    }

    void Reset()
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_gate[i] = false;
            m_preGate[i] = false;
            m_phasorOut[i] = 0;
        }
    }
    
    struct PhasorBounds
    {
        PhasorBounds()
            : m_low(0)
            , m_start(0)
            , m_high(0)
        {
        }
        
        float m_low;
        float m_start;
        float m_high;

        void Set(float phase)
        {
            m_start = phase;
            if (phase >= 0.5)
            {
                m_low = 0.5;
                m_high = 1;
            }
            else
            {
                m_low = 0;
                m_high = 0.5;
            }
        }

        float GetPhase(float phase)
        {
            float lowPhase = 1 - (phase - m_low) / (m_start - m_low);
            float highPhase = 1 - (m_high - phase) / (m_high - m_start);
            return std::max(lowPhase, highPhase);            
        }
    };

    bool m_anyGate;
    bool m_gate[x_maxPoly];
    bool m_preGate[x_maxPoly];
    bool m_trig[x_maxPoly];
    bool m_set[x_maxPoly];
    float m_phasorOut[x_maxPoly];
    PhasorBounds m_bounds[x_maxPoly][x_maxPoly];
    bool m_phasorSelector[x_maxPoly][x_maxPoly];

    void Process(Input& input)
    {
        m_anyGate = false;
        for (size_t i = 0; i < input.m_numTrigs; ++i)
        {
            m_trig[i] = input.m_trigs[i] && input.m_newTrigCanStart[i] && !input.m_mute[i];

            if (m_set[i])
            {
                float phasorOut = 0;
                for (size_t j = 0; j < input.m_numPhasors; ++j)
                {
                    if (m_phasorSelector[i][j])
                    {
                        float thisPhase = m_bounds[i][j].GetPhase(input.m_phasors[j]);
                        if (input.m_gateFrac[i] <= thisPhase &&
                            (input.m_gateFrac[i] < 1.0 || !input.m_newTrigCanStart[i] || input.m_mute[i]))
                        {
                            m_gate[i] = false;
                            m_preGate[i] = false;
                        }
                        
                        phasorOut = std::max(thisPhase, phasorOut);
                    }
                }
            
                if (phasorOut >= 1)
                {
                    phasorOut = 0;
                    m_set[i] = false;
                }
                
                m_phasorOut[i] = phasorOut;
            }
            else if (m_preGate[i] && 
                     (input.m_gateFrac[i] < 1.0 || !input.m_newTrigCanStart[i] || input.m_mute[i]))
            {
                m_gate[i] = false;
                m_preGate[i] = false;
            }

            if (input.m_trigs[i] && input.m_newTrigCanStart[i])
            {
                if (!input.m_mute[i])
                {
                    m_gate[i] = true;
                }

                m_preGate[i] = true;
                m_set[i] = true;
                for (size_t j = 0; j < input.m_numPhasors; ++j)
                {
                    m_bounds[i][j].Set(input.m_phasors[j]);
                    m_phasorSelector[i][j] = input.m_phasorSelector[i][j];
                }
            }

            if (m_gate[i])
            {
                m_anyGate = true;
            }
        }
    }    

    struct NonagonTrigLogic
    {
        static constexpr size_t x_numVoices = 9;
        static constexpr size_t x_numTrios = 3;
        static constexpr size_t x_voicesPerTrio = x_numVoices / x_numTrios;

        bool m_pitchChanged[x_numVoices];
        bool m_earlyMuted[x_numVoices];
        bool m_subTrigger[x_numVoices];
        bool m_mute[x_numVoices];

        bool m_trigOnSubTrigger[x_numTrios];
        bool m_trigOnPitchChanged[x_numTrios];

        bool m_interrupt[x_numTrios][x_numTrios];

        int m_unisonMaster[x_numTrios];

        bool m_running;

        NonagonTrigLogic()
            : m_running(false)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_pitchChanged[i] = false;
                m_earlyMuted[i] = false;
                m_subTrigger[i] = false;
                m_mute[i] = false;
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_trigOnSubTrigger[i] = false;
                m_trigOnPitchChanged[i] = true;
                m_unisonMaster[i] = -1;

                for (size_t j = 0; j < x_numTrios; ++j)
                {
                    m_interrupt[i][j] = false;
                }
            }
        }

        void SetInput(Input& input)
        {
            for (size_t i = 0; i < x_maxPoly; ++i)
            {
                size_t trioId = i / x_voicesPerTrio;
                size_t ixToCheck = m_unisonMaster[trioId] == -1 ? i : m_unisonMaster[trioId];
                input.m_mute[i] = m_mute[i];
                input.m_trigs[i] = (m_trigOnSubTrigger[trioId] && m_subTrigger[ixToCheck]) || 
                                   (m_trigOnPitchChanged[trioId] && m_pitchChanged[ixToCheck]);
                input.m_trigs[i] &= !m_earlyMuted[ixToCheck];
                input.m_newTrigCanStart[i] = m_running && !m_earlyMuted[ixToCheck];


                for (size_t j = 0; j < i; ++j)
                {
                    size_t jTrioId = j / x_voicesPerTrio;
                    if (jTrioId == trioId && m_unisonMaster[trioId] != -1)
                    {
                        continue;
                    }

                    if (m_interrupt[trioId][jTrioId] && input.m_trigs[j] && !input.m_mute[j])
                    {
                        input.m_trigs[i] = false;
                    }
                }
            }
        }
    };
};

#ifndef IOS_BUILD
struct MultiPhasorGate : Module
{
    static constexpr size_t x_maxPoly = 16;

    size_t PhasorInId()
    {
        return 0;
    }

    size_t TrigInId()
    {
        return PhasorInId() + 1;
    }

    size_t GateFracInId()
    {
        return TrigInId() + 1;
    }

    size_t PhasorSelectorId()
    {
        return GateFracInId() + 1;
    }

    size_t NumInputs()
    {
        return PhasorSelectorId() + 1;
    }

    size_t GetNumPhasors()
    {
        return inputs[PhasorInId()].getChannels();
    }

    float GetPhasor(size_t i)
    {
        return inputs[PhasorInId()].getVoltage(i) / 10;
    }

    size_t GetNumTrigs()
    {
        return inputs[TrigInId()].getChannels();
    }

    float GetTrig(size_t i)
    {
        return inputs[TrigInId()].getVoltage(i);
    }

    float GetGateFrac(size_t i)
    {
        return inputs[GateFracInId()].getVoltage(i) / 10;
    }

    float GetPhasorSelector(size_t phasorId)
    {
        return inputs[PhasorSelectorId()].getVoltage(phasorId);
    }

    size_t GateOutId()
    {
        return 0;
    }

    void SetGateOut(size_t trigId, float out)
    {
        outputs[GateOutId()].setVoltage(out, trigId);
    }

    MultiPhasorGateInternal m_mpg;
    float m_timeToCheck;

    static constexpr float x_timeToCheckButtons = 0.05;

    MultiPhasorGateInternal::Input m_state;

    void SetPhasorSelectors()
    {
        m_state.m_numTrigs = GetNumTrigs();
        m_state.m_numPhasors = GetNumPhasors();
        for (size_t i = 0; i < m_state.m_numPhasors; ++i)
        {
            //            m_state.m_phasorSelector[i] = GetPhasorSelector(i);
        }

        for (size_t i = 0; i < m_state.m_numTrigs; ++i)
        {
            m_state.m_gateFrac[i] = GetGateFrac(i);
        }

        outputs[GateOutId()].setChannels(m_state.m_numTrigs);
    }

    void SetTrigsAndPhasors()
    {
        for (size_t i = 0; i < m_state.m_numTrigs; ++i)
        {
            m_state.m_trigs[i] = GetTrig(i);
        }

        for (size_t i = 0; i < m_state.m_numPhasors; ++i)
        {
            m_state.m_phasors[i] = GetPhasor(i);
        }
    }
    
    void process(const ProcessArgs &args) override
    {
        if (m_timeToCheck < 0)
        {
            SetPhasorSelectors();
            m_timeToCheck = x_timeToCheckButtons;
        }

        SetTrigsAndPhasors();
        
        m_timeToCheck -= args.sampleTime;

        m_mpg.Process(m_state);

        for (size_t i = 0; i < m_state.m_numTrigs; ++i)
        {
            SetGateOut(i, m_mpg.m_gate[i] ? 10 : 0);
        }
    }

    MultiPhasorGate()
        : m_timeToCheck(-1)
    {
        config(0, NumInputs(), 1, 0);

        configInput(PhasorInId(), "Phasor In");
        configInput(TrigInId(), "Trig In");
        configInput(GateFracInId(), "Gate Fraction In");
        configInput(PhasorSelectorId(), "Phasor Selector");
        configOutput(GateOutId(), "Gate Output");
    }
};

struct MultiPhasorGateWidget : ModuleWidget
{
    MultiPhasorGateWidget(MultiPhasorGate* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MultiPhasorGate.svg")));
        
        addInput(createInputCentered<PJ301MPort>(Vec(75, 100), module, module->PhasorInId()));
        addInput(createInputCentered<PJ301MPort>(Vec(100, 100), module, module->TrigInId()));
        addInput(createInputCentered<PJ301MPort>(Vec(125, 100), module, module->GateFracInId()));
        addInput(createInputCentered<PJ301MPort>(Vec(150, 100), module, module->PhasorSelectorId()));
        addOutput(createOutputCentered<PJ301MPort>(Vec(175, 100), module, module->GateOutId()));
    }    
};
#endif
