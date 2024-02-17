#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "BlinkLight.hpp"
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

        Input()
            : m_numTrigs(0)
            , m_numPhasors(0)
        {
            for (size_t i = 0; i < x_maxPoly; ++i)
            {
                m_trigs[i] = false;
                m_phasors[i] = 0;
                m_gateFrac[i] = 0.5;
                for (size_t j = 0; j < x_maxPoly; ++j)
                {
                    m_phasorSelector[i][j] = false;
                }

            }
        }
    };
    
    MultiPhasorGateInternal()
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_gate[i] = false;
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

        bool Check(float phase, float* phasorOut, float gateFrac)
        {
            float lowPhase = 1 - (phase - m_low) / (m_start - m_low);
            float highPhase = 1 - (m_high - phase) / (m_high - m_start);
            *phasorOut = std::max(lowPhase, highPhase);            
            return *phasorOut < gateFrac;
        }
    };

    bool m_gate[x_maxPoly];
    bool m_set[x_maxPoly];
    float m_phasorOut[x_maxPoly];
    PhasorBounds m_bounds[x_maxPoly][x_maxPoly];

    void Process(Input& input)
    {
        for (size_t i = 0; i < input.m_numTrigs; ++i)
        {
            if (m_set[i])
            {
                float phasorOut = 0;
                for (size_t j = 0; j < input.m_numPhasors; ++j)
                {
                    if (input.m_phasorSelector[i][j])
                    {
                        float thisPhase = 0;
                        if (!m_bounds[i][j].Check(input.m_phasors[j], &thisPhase, input.m_gateFrac[i]))
                        {
                            m_gate[i] = false;
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

            if (!m_gate[i] && input.m_trigs[i])
            {
                m_gate[i] = true;
                m_set[i] = true;
                for (size_t j = 0; j < input.m_numPhasors; ++j)
                {
                    m_bounds[i][j].Set(input.m_phasors[j]);
                }
            }
        }
    }    
};

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
