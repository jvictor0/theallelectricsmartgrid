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

}
