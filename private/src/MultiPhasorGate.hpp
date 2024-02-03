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
        float m_trigs[x_maxPoly];
        float m_phasors[x_maxPoly];
        size_t m_numTrigs;
        size_t m_numPhasors;
        float m_phasorSelector[x_maxPoly];
        float m_gateFrac[x_maxPoly];
    };
    
    MultiPhasorGateInternal()
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_gate[i] = false;
        }
    }
    
    struct PhasorBounds
    {
        PhasorBounds()
            : m_low(0)
            , m_high(0)
        {
        }
        
        float m_low;
        float m_high;

        void Set(float phase, float gateFrac)
        {
            if (phase >= 5)
            {
                m_low = 5 + (phase - 5) * (1 - gateFrac);
                m_high = 10 - (10 - phase) * (1 - gateFrac);
            }
            else
            {
                m_low = phase * (1 - gateFrac);
                m_high = 5 - (5 - phase) * (1 - gateFrac);
            }
        }

        bool Check(float phase)
        {
            return m_low < phase && phase < m_high;
        }
    };

    bool m_gate[x_maxPoly];
    Trig m_trigs[x_maxPoly];
    PhasorBounds m_bounds[x_maxPoly][x_maxPoly];

    void Process(Input& input)
    {
        for (size_t i = 0; i < input.m_numTrigs; ++i)
        {
            if (m_gate[i])
            {
                for (size_t j = 0; j < input.m_numPhasors; ++j)
                {
                    if (input.m_phasorSelector[j])
                    {
                        if (!m_bounds[i][j].Check(input.m_phasors[j]))
                        {
                            m_gate[i] = false;
                            break;
                        }
                    }
                }
            }
            else
            {
                if (m_trigs[i].Process(input.m_trigs[i]))
                {
                    m_gate[i] = true;
                    for (size_t j = 0; j < input.m_numPhasors; ++j)
                    {
                        m_bounds[i][j].Set(input.m_phasors[j], input.m_gateFrac[i] / 10);
                    }
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
        return inputs[PhasorInId()].getVoltage(i);
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
        return inputs[GateFracInId()].getVoltage(i);
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
            m_state.m_phasorSelector[i] = GetPhasorSelector(i);
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
