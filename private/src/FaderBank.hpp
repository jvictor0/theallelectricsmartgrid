#pragma once
#include "SmartGrid.hpp"
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
                x_gridSize,
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
    
    FaderBankSmartGrid()
        : CompositeGrid()
    {
        for (size_t i = 0; i < x_gridSize; ++i)
        {
            m_faders[i].reset(new BankedFader());
            AddGrid(i, 0, m_faders[i]);
        }
    }

    virtual void Apply(Message msg) override
    {
        if (msg.m_x >= 0 &&
            msg.m_x < x_gridSize &&
            m_faders[msg.m_x]->m_outputConnected)
        {
            CompositeGrid::Apply(msg);
        }
    }
    
    virtual Color GetColor(int i, int j) override
    {
        if (i >= 0 && i < x_gridSize && m_faders[i]->m_outputConnected)
        {
            return CompositeGrid::GetColor(i, j);
        }
        
        return Color::Off;
    }

    
    std::shared_ptr<BankedFader> m_faders[x_gridSize];

    struct Input
    {
        BankedFader::Input m_inputs[x_gridSize];
    };

    void ProcessInput(Input& input)
    {
        for (size_t i = 0; i < x_gridSize; ++i)
        {
            m_faders[i]->ProcessInput(input.m_inputs[i]);
        }
    }
};

struct FaderBank : public Module
{
    FaderBankSmartGrid m_faderBank;
    FaderBankSmartGrid::Input m_state;

    static constexpr size_t x_gridIdOutId = x_gridSize;
    
    FaderBank()
    {
        config(x_gridSize, x_gridSize, x_gridSize + 1, 0);

        for (size_t i = 0; i < x_gridSize; ++i)
        {
            configInput(i, ("Fader " + std::to_string(i)).c_str());
            configOutput(i, ("Fader " + std::to_string(i)).c_str());
            configParam(i, 0, 1, 1, ("Color " + std::to_string(i)).c_str());
        }

        configOutput(x_gridIdOutId, "Grid Id");
    }

    void ReadState()
    {
        for (size_t i = 0; i < x_gridSize; ++i)
        {
            m_state.m_inputs[i].m_outputConnected = outputs[i].isConnected();
            m_state.m_inputs[i].m_inputConnected = inputs[i].isConnected();
            m_state.m_inputs[i].m_color = Color::ZDecodeFloat(params[i].getValue());
        }
    }

    void SetOutputs()
    {
        for (size_t i = 0; i < x_gridSize; ++i)
        {
            outputs[i].setVoltage(m_faderBank.m_faders[i]->m_state);
        }

        outputs[x_gridIdOutId].setVoltage(m_faderBank.m_gridId);
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
        
        for (size_t i = 0; i < x_gridSize; ++i)
        {
            float rowPos = 100 + i * 30;
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart, rowPos), module, i));
            addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 50, rowPos), module, i));
            addParam(createParamCentered<Trimpot>(Vec(rowStart + 100, rowPos), module, i));
        }
    }
};

}
