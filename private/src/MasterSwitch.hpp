#pragma once
#include "PeriodChecker.hpp"
#include "StateSaver.hpp"
#include "plugin.hpp"

#ifndef IOS_BUILD
struct MasterSwitch : Module
{
    static constexpr size_t x_maxSteps = 64;
    static constexpr size_t x_maxChans = 64;
    static constexpr size_t x_maxPoly = 16;
    static constexpr size_t x_numJacks = 4;
    static constexpr size_t x_numLanes = 8;
    
    enum class InputMode : int
    {
        Clock,
        Select,
        OneHot,
        Parallel
    };

    enum class SampleMode : int
    {
        SH,
        Track,
        TH
    };

    InputMode m_inputMode;
    SampleMode m_sampleMode;
    size_t m_steps;
    size_t m_curStep[x_maxChans];
    size_t m_numChans;
    size_t m_numLanes;
    rack::dsp::TSchmittTrigger<float> m_clockTrig[x_maxChans];
    rack::dsp::TSchmittTrigger<float> m_resetTrig;
    PeriodChecker m_periodChecker;

    MasterSwitch()
        : m_inputMode(InputMode::Clock)
        , m_sampleMode(SampleMode::SH)
        , m_steps(0)
        , m_numChans(0)
        , m_numLanes(0)
    {
        config(x_numParams, x_numIns, x_numOuts, 0);
        for (size_t i = 0; i < x_maxChans; ++i)
        {
            m_curStep[i] = 0;
        }

        for (size_t i = 0; i < x_numJacks; ++i)
        {
            configInput(x_stepIn + i, "Step" + std::to_string(i));
            configOutput(x_stepOut + i, "Step" + std::to_string(i));
            configOutput(x_oneHotOut + i, "OneHot" + std::to_string(i));
            for (size_t j = 0; j < x_numLanes; ++j)
            {
                configInput(x_valueIn + x_numJacks * j + i, "Value" + std::to_string(j) + "_" + std::to_string(i));
                configOutput(x_laneOut + x_numJacks * j + i, "Lane" + std::to_string(j) + "_" + std::to_string(i));
            }
        }

        configInput(x_resetIn, "Reset");
        configSwitch(x_inputModeParam, 0, 3, 0, "Input Mode", {"Clock", "Select", "OneHot", "Parallel"});
        configSwitch(x_sampleParam, 0, 2, 0, "Sample Mode", {"SH", "Track", "TH"});
        configSwitch(x_numStepsParam, 1, x_maxSteps, x_maxSteps, "Steps");
    }

    size_t Chan2Jack(size_t chan)
    {
        return chan / x_maxPoly;
    }

    size_t Chan2Poly(size_t chan)
    {
        return chan % x_maxPoly;
    }

    size_t JackPoly2Chan(size_t jack, size_t poly)
    {
        return x_numJacks * jack + poly;
    }

    static constexpr size_t x_inputModeParam = 0;
    static constexpr size_t x_sampleParam = x_inputModeParam + 1;
    static constexpr size_t x_numStepsParam = x_sampleParam + 1;
    static constexpr size_t x_numParams = x_numStepsParam + 1;
    
    static constexpr size_t x_stepIn = 0;
    static constexpr size_t x_valueIn = x_stepIn + x_numJacks;
    static constexpr size_t x_resetIn = x_valueIn + x_numJacks * x_numLanes;
    static constexpr size_t x_numIns = x_resetIn + 1;

    static constexpr size_t x_stepOut = 0;
    static constexpr size_t x_oneHotOut = x_stepOut + x_numJacks;
    static constexpr size_t x_laneOut = x_oneHotOut + x_numJacks;
    static constexpr size_t x_numOuts = x_laneOut + x_numJacks * x_numLanes;    
    
    void SetNumChans()
    {
        size_t numLanes = x_numLanes;
        for (ssize_t i = x_numLanes - 1; i >= 0; --i)
        {
            bool connected = false;
            for (size_t j = 0; j < x_numJacks; ++j)
            {
                if (inputs[x_valueIn + x_numJacks * i + j].isConnected())
                {
                    connected = true;
                    break;
                }
            }

            if (connected)
            {
                break;
            }

            --numLanes;
        }

        m_numLanes = numLanes;
        
        size_t inputPoly = 0;
        for (size_t i = 0; i < x_numJacks; ++i)
        {
            if (i + 1 < x_numJacks && inputs[x_stepIn + i + 1].isConnected())
            {
                inputPoly = (i + 1) * x_maxPoly;
            }
            else if (inputs[x_stepIn + i].isConnected())
            {
                inputPoly += inputs[x_stepIn + i].getChannels();
            }
        }

        size_t numChans = inputPoly;
        size_t numSteps = static_cast<size_t>(params[x_numStepsParam].getValue());

        m_inputMode = static_cast<InputMode>(static_cast<int>(params[x_inputModeParam].getValue()));
        m_sampleMode = static_cast<SampleMode>(static_cast<int>(params[x_sampleParam].getValue()));
        if (m_inputMode == InputMode::OneHot)
        {
            numChans = 1;
            numSteps = inputPoly;            
        }
        else if (m_inputMode == InputMode::Parallel)
        {
            for (size_t i = 0; i < numChans; ++i)
            {
                m_curStep[i] = i;
            }
        }

        if (m_numChans != numChans ||
            m_steps != numSteps)
        {
            m_numChans = numChans;
            m_steps = numSteps;
            for (size_t i = 0; i < x_numJacks; ++i)
            {
                outputs[x_stepOut +  i].setChannels(JackPolyChans(i, m_numChans));
                if (m_numChans == 1)
                {
                    outputs[x_oneHotOut +  i].setChannels(JackPolyChans(i, m_steps));
                }
                else
                {
                    outputs[x_oneHotOut +  i].setChannels(0);
                }

                for (size_t j = 0; j < m_numLanes; ++j)
                {
                    outputs[x_laneOut + j * x_numJacks + i].setChannels(JackPolyChans(i, m_numChans));
                }
            }
        }
    }

    size_t JackPolyChans(size_t jack, size_t chans)
    {
        if (chans <= jack * x_maxPoly)
        {
            return 0;
        }
        else
        {
            return std::min<size_t>(16, chans - jack * x_maxPoly);
        }
    }
    
    float GetStepIn(size_t chan)
    {
        return inputs[x_stepIn + Chan2Jack(chan)].getVoltage(Chan2Poly(chan));
    }

    float GetValue(size_t lane, size_t step)
    {
        return inputs[x_valueIn + x_numJacks * lane + Chan2Jack(step)].getVoltage(Chan2Poly(step));
    }

    void SetValue(size_t lane, size_t chan)
    {
        float value = GetValue(lane, m_curStep[chan]);
        outputs[x_laneOut + x_numJacks * lane + Chan2Jack(chan)].setVoltage(value, Chan2Poly(chan));
    }

    void SetChan(size_t chan)
    {
        for (size_t i = 0; i < m_numLanes; ++i)
        {
            SetValue(i, chan);
        }
    }
    
    void StepSelect()
    {
        if (m_inputMode == InputMode::OneHot)
        {
            bool wasHigh = m_clockTrig[m_curStep[0]].isHigh();
            m_clockTrig[m_curStep[0]].process(GetStepIn(m_curStep[0]));
            if (!m_clockTrig[m_curStep[0]].isHigh())
            {
                for (size_t i = 0; i < m_steps; ++i)
                {
                    m_clockTrig[i].process(GetStepIn(i));
                    if (m_clockTrig[i].isHigh())
                    {
                        m_curStep[0] = i;
                    }
                }
            }

           if (m_sampleMode == SampleMode::SH &&
               !wasHigh &&
               m_clockTrig[m_curStep[0]].isHigh())
            {
                SetChan(0);
            }
            else if (m_sampleMode == SampleMode::TH &&
                     m_clockTrig[m_curStep[0]].isHigh())
            {
                SetChan(0);
            }
        }
        else
        {
            for (size_t i = 0; i < m_numChans; ++i)
            {
                if (m_inputMode == InputMode::Clock || m_inputMode == InputMode::Parallel)
                {
                    bool wasHigh = m_clockTrig[i].isHigh();
                    m_clockTrig[i].process(GetStepIn(i));
                    if (!wasHigh && m_clockTrig[i].isHigh())
                    {
                        if (m_inputMode == InputMode::Clock)
                        {
                            m_curStep[i] = (m_curStep[i] + 1) % m_steps;
                        }
                        
                        if (m_sampleMode != SampleMode::Track)
                        {
                            SetChan(i);
                        }
                    }
                    else if (m_sampleMode == SampleMode::TH &&
                             m_clockTrig[i].isHigh())
                    {
                        SetChan(i);
                    }
                }
                else
                {
                    int step = static_cast<int>(GetStepIn(i) * 8);
                    step = std::min<int>(m_steps - 1, std::max<int>(0, step));
                    if (static_cast<int>(m_curStep[i]) != step)
                    {
                        if (m_sampleMode != SampleMode::Track)
                        {
                            SetChan(i);
                        }

                        m_curStep[i] = step;
                    }
                }
            }
        }

        if (m_sampleMode == SampleMode::Track)
        {
            for (size_t i = 0; i < m_numChans; ++i)
            {
                SetChan(i);
            }
        }        
    }

    void SetOutputs()
    {
        for (size_t i = 0; i < m_numChans; ++i)
        {
            outputs[x_stepOut + Chan2Jack(i)].setVoltage(static_cast<float>(m_curStep[i]) / 8, Chan2Poly(i));
        }

        for (size_t i = 0; i < m_steps; ++i)
        {
            float voltage = i == m_curStep[0] ? 10 : 0;
            outputs[x_oneHotOut + Chan2Jack(i)].setVoltage(voltage, Chan2Poly(i));
        }
    }

    void HandleReset()
    {
        bool wasHigh = m_resetTrig.isHigh();
        m_resetTrig.process(inputs[x_resetIn].getVoltage());
        if (!wasHigh && m_resetTrig.isHigh())
        {
            for (size_t i = 0; i < m_numChans; ++i)
            {
                m_curStep[i] = 0;
                SetChan(i);
            }
        }
    }

    void process(const ProcessArgs &args) override
    {
        if (m_periodChecker.Process(args.sampleTime))
        {
            SetNumChans();
        }
        
        HandleReset();
        StepSelect();
        SetOutputs();
    }
};

struct MasterSwitchWidget : ModuleWidget
{
    MasterSwitchWidget(MasterSwitch *module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MasterSwitch.svg")));

        addParam(createParam<Trimpot>(Vec(10, 10), module, MasterSwitch::x_inputModeParam));
        addParam(createParam<Trimpot>(Vec(10, 40), module, MasterSwitch::x_sampleParam));
        addParam(createParam<Trimpot>(Vec(10, 70), module, MasterSwitch::x_numStepsParam));

        for (size_t i = 0; i < MasterSwitch::x_numJacks; ++i)
        {
            addInput(createInput<PJ301MPort>(Vec(30 + 25 * i, 100), module, MasterSwitch::x_stepIn + i));
            addOutput(createOutput<PJ301MPort>(Vec(30 + 25 * i, 125), module, MasterSwitch::x_stepOut + i));
            addOutput(createOutput<PJ301MPort>(Vec(150 + 25 * i, 125), module, MasterSwitch::x_oneHotOut + i));
            for (size_t j = 0; j < MasterSwitch::x_numLanes; ++j)
            {
                addInput(createInput<PJ301MPort>(Vec(30 + 25 * i, 150 + j * 25), module, MasterSwitch::x_valueIn + MasterSwitch::x_numJacks * j + i));
                addOutput(createOutput<PJ301MPort>(Vec(150 + 25 * i, 150 + j * 25), module, MasterSwitch::x_laneOut + MasterSwitch::x_numJacks * j + i));
            }
        }

        addInput(createInput<PJ301MPort>(Vec(50, 70), module, MasterSwitch::x_resetIn));
    }
};
#endif
