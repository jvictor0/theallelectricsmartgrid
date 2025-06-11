#pragma once
#include "SmartGrid.hpp"
#include "StateSaver.hpp"
#include "plugin.hpp"

namespace SmartGrid
{

struct FaderBankSmartGrid : public CompositeGrid
{
    struct BankedFader : public Fader
    {
        float m_state;
        bool m_inputConnected;
        bool m_outputConnected;
        
        BankedFader()
            : Fader(
                &m_state,
                x_baseGridSize,
                Color::White,
                0 /*minValue*/,
                10 /*maxValue*/)
            , m_state(0)
            , m_inputConnected(false)
            , m_outputConnected(false)
        {
        }

        struct Input
        {
            bool m_outputConnected;
            bool m_inputConnected;
            Color m_color;

            Input()
                : m_outputConnected(false)
                , m_inputConnected(false)
                , m_color(Color::White)
            {
            }
        };
        
        void ProcessInput(Input& input)
        {
            m_inputConnected = input.m_inputConnected;
            m_outputConnected = input.m_outputConnected;
            m_color = input.m_color;
        }
    };
    
    FaderBankSmartGrid(StateSaver& saver)
        : CompositeGrid()
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_faders[i].reset(new BankedFader());
            AddGrid(i, 0, m_faders[i]);
            saver.Insert("Value", i, &m_faders[i]->m_state);
        }
    }

    virtual void Apply(Message msg) override
    {
        if (msg.m_x >= 0 &&
            msg.m_x < x_baseGridSize &&
            m_faders[msg.m_x]->m_outputConnected)
        {
            CompositeGrid::Apply(msg);
        }
    }
    
    virtual Color GetColor(int i, int j) override
    {
        if (i >= 0 && i < x_baseGridSize && m_faders[i]->m_outputConnected)
        {
            return CompositeGrid::GetColor(i, j);
        }
        
        return Color::Off;
    }

    
    std::shared_ptr<BankedFader> m_faders[x_baseGridSize];

    struct Input
    {
        BankedFader::Input m_inputs[x_baseGridSize];
    };

    void ProcessInput(Input& input)
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_faders[i]->ProcessInput(input.m_inputs[i]);
        }
    }
};

#ifndef IOS_BUILD
struct FaderBank : public Module
{
    StateSaver m_stateSaver;
    FaderBankSmartGrid m_faderBank;
    FaderBankSmartGrid::Input m_state;

    static constexpr size_t x_colorIn = x_baseGridSize;

    static constexpr size_t x_gridIdOutId = x_baseGridSize;
    
    FaderBank()
        : m_faderBank(m_stateSaver)
    {
        config(x_baseGridSize, 2 * x_baseGridSize, x_baseGridSize + 1, 0);

        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            configInput(i, ("Fader " + std::to_string(i)).c_str());
            configOutput(i, ("Fader " + std::to_string(i)).c_str());
            configParam(i, 0, 1, 1, ("Color " + std::to_string(i)).c_str());
            configInput(x_colorIn + i, ("Color " + std::to_string(i)).c_str());
        }

        configOutput(x_gridIdOutId, "Grid Id");
    }

    void ReadState()
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_state.m_inputs[i].m_outputConnected = outputs[i].isConnected();
            m_state.m_inputs[i].m_inputConnected = inputs[i].isConnected();
            if (inputs[x_colorIn + i].isConnected())
            {
                m_state.m_inputs[i].m_color = Color::ZDecodeFloat(inputs[x_colorIn + i].getVoltage() / 10);
            }
            else
            {
                m_state.m_inputs[i].m_color = Color::ZDecodeFloat(params[i].getValue());
            }
        }
    }

    void SetOutputs()
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            outputs[i].setVoltage(m_faderBank.m_faders[i]->m_state);
        }

        outputs[x_gridIdOutId].setVoltage(m_faderBank.m_gridId);
    }

    json_t* dataToJson() override
    {
        return m_stateSaver.ToJSON();
    }

    void dataFromJson(json_t* rootJ) override
    {
        m_stateSaver.SetFromJSON(rootJ);
    }

    void process(const ProcessArgs &args) override
    {
        m_faderBank.ProcessStatic(args.sampleTime);
        ReadState();
        m_faderBank.ProcessInput(m_state);
        SetOutputs();
    }    
};

struct FaderBankWidget : public ModuleWidget
{
    FaderBankWidget(FaderBank* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/FaderBank.svg")));
        
        addOutput(createOutputCentered<PJ301MPort>(Vec(300, 100), module, module->x_gridIdOutId));

        float rowStart = 50;
        
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            float rowPos = 100 + i * 30;
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart, rowPos), module, i));
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 50, rowPos), module, i));
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 75, rowPos), module, x_baseGridSize + i));
            addParam(createParamCentered<Trimpot>(Vec(rowStart + 100, rowPos), module, i));
        }
    }
};
#endif

}
