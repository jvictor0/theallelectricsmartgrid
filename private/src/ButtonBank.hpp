#pragma once
#include "SmartGrid.hpp"
#include "StateSaver.hpp"
#include "plugin.hpp"
#include "VoiceAllocator.hpp"

namespace SmartGrid
{

struct ButtonBankInternal : Grid
{
    Color m_offColor;
    Color m_onColor;
    bool m_momentary;
    size_t m_width[x_baseGridSize];
    VoiceAllocator<CellVoice> m_voiceAllocator;


#ifndef SMART_BOX
    static constexpr size_t x_maxWidth = x_baseGridSize;
#else
    static constexpr size_t x_maxWidth = 16;
#endif

    struct BankedButton : public Cell
    {
        bool m_state;
        Color m_color;
        bool m_hasDirectColor;
        
        int m_x;
        int m_y;
        ButtonBankInternal* m_owner;

        struct Input
        {
            ColorDecode m_color;
            bool m_hasDirectColor;

            Input()
                : m_hasDirectColor(false)
            {
            }
        };

        CellVoice ToCellVoice()
        {
            return CellVoice(&m_state, m_x, m_y);
        }

        void Process(Input& input)
        {
            m_hasDirectColor = input.m_hasDirectColor;
            if (m_hasDirectColor)
            {
                m_color = input.m_color.m_color;
            }
        }
        
        BankedButton(int x, int y, ButtonBankInternal* owner)
            : m_state(false)
            , m_color(Color::White)
            , m_hasDirectColor(false)
            , m_x(x)
            , m_y(y)
            , m_owner(owner)
        {
            m_pressureSensitive = true;
        }

        virtual Color GetColor() override
        {
            if (static_cast<int>(m_owner->m_width[m_y]) <= m_x)
            {
                return Color::Off;
            }            
            else if (m_hasDirectColor)
            {
                return m_color;
            }
            else
            {
                return m_state ? m_owner->m_onColor : m_owner->m_offColor;
            }
        }

        virtual void OnPress(uint8_t) override
        {
            if (m_owner->HasPolyphony())
            {
                if (m_state)
                {
                    m_owner->m_voiceAllocator.DeAllocate(ToCellVoice());
                }
                else
                {
                    m_owner->m_voiceAllocator.Allocate(ToCellVoice());
                }
            }
            else
            {
                m_state = !m_state;
            }
        }

        virtual void OnRelease() override
        {
            if (m_owner->m_momentary)
            {
                if (m_owner->HasPolyphony())
                {
                    if (m_state)
                    {
                        m_owner->m_voiceAllocator.DeAllocate(ToCellVoice());
                    }
                }
                else
                {
                    m_state = !m_state;
                }
            }
        }
    };

    ButtonBankInternal()
        : m_offColor(Color::White.Dim())
        , m_onColor(Color::White)
        , m_momentary(false)
    {
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_width[i] = x_maxWidth;
            for (size_t j = 0; j < x_maxWidth; ++j)
            {
                Put(j, i, new BankedButton(j, i, this));
            }
        }
    }

    struct Input
    {
        ColorDecode m_offColor;
        ColorDecode m_onColor;
        bool m_momentary;
        size_t m_width[x_baseGridSize];
        BankedButton::Input m_input[x_maxWidth][x_baseGridSize];
        size_t m_maxPolyphony;

        Input()
            : m_offColor(Color::White.Dim())
            , m_onColor(Color::White)
            , m_momentary(false)
            , m_maxPolyphony(0)
        {
            for (size_t i = 0; i < x_baseGridSize; ++i)
            {
                m_width[i] = x_maxWidth;
            }
        }
    };

    bool HasPolyphony()
    {
        return m_voiceAllocator.m_maxPolyphony > 0;
    }
    
    void Clear()
    {
        for (size_t i = 0; i < x_maxWidth; ++i)
        {
            for (size_t j = 0; j < x_baseGridSize; ++j)
            {
                static_cast<BankedButton*>(Get(i, j))->m_state = false;
            }
        }
    }
    
    void ProcessInput(Input& input)
    {
        m_offColor = input.m_offColor.m_color;
        m_onColor = input.m_onColor.m_color;
        m_momentary = input.m_momentary;
        if (m_voiceAllocator.m_maxPolyphony != input.m_maxPolyphony)
        {
            m_voiceAllocator.SetPolyphony(input.m_maxPolyphony);
            Clear();
        }
        
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_width[i] = input.m_width[i];
            for (size_t j = 0; j < m_width[i]; ++j)
            {
                static_cast<BankedButton*>(Get(j, i))->Process(input.m_input[j][i]);
            }
        }
    }
};

#ifndef IOS_BUILD
struct ButtonBank : Module
{
    static constexpr size_t x_maxPoly = 16;
    
    static constexpr size_t x_momentaryParam = 0;
    static constexpr size_t x_onColorParam = x_momentaryParam + 1;
    static constexpr size_t x_offColorParam = x_onColorParam + 1;
    static constexpr size_t x_widthParam = x_offColorParam + 1;
    static constexpr size_t x_polyphonyParam = x_widthParam + x_baseGridSize;
    static constexpr size_t x_numParams = x_polyphonyParam + 1;

    static constexpr size_t x_momentaryIn = 0;
    static constexpr size_t x_onColorIn = x_momentaryIn + 1;
    static constexpr size_t x_offColorIn = x_onColorIn + 1;
    static constexpr size_t x_rowColorIn = x_offColorIn + 1;
    static constexpr size_t x_numIns = x_rowColorIn + x_baseGridSize;

    static constexpr size_t x_gridIdOut = 0;
    static constexpr size_t x_gateOut = x_gridIdOut + 1;
    static constexpr size_t x_velocityOut = x_gateOut + x_baseGridSize;
    static constexpr size_t x_polyXOut = x_velocityOut + x_baseGridSize;
    static constexpr size_t x_polyYOut = x_polyXOut + 1;
    static constexpr size_t x_polyVelocityOut = x_polyYOut + 1;
    static constexpr size_t x_polyGateOut = x_polyVelocityOut + 1;
    static constexpr size_t x_numOuts = x_polyGateOut + 1;    

    bool m_overflowPorts = true;
    
    ButtonBankInternal m_bank;
    ButtonBankInternal::Input m_state;
    rack::dsp::TSchmittTrigger<float> m_momentaryTrig;
    PeriodChecker m_periodChecker;
    
    ButtonBank()
    {
        config(x_numParams, x_numIns, x_numOuts, 0);

        configSwitch(x_momentaryParam, 0, 1, 0, "Momentary");
        configParam(x_onColorParam, 0, 1, 1, "On Color");
        configParam(x_offColorParam, 0, 1, Color::White.Dim().ZEncodeFloat(), "Off Color");
        configSwitch(x_polyphonyParam, 0, 16, 0, "Polyphony");

        configInput(x_momentaryIn, "Momentary");
        configInput(x_onColorIn, "On Color");
        configInput(x_offColorIn, "Off Color");
        configOutput(x_gridIdOut, "Grid Id");
        configOutput(x_polyXOut, "Poly X");
        configOutput(x_polyYOut, "Poly Y");
        configOutput(x_polyVelocityOut, "Poly Velocity");
        configOutput(x_polyGateOut, "Poly Gate");

        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            configSwitch(x_widthParam + i, 0, ButtonBankInternal::x_maxWidth, ButtonBankInternal::x_maxWidth, "Row Width " + std::to_string(i));
            configInput(x_rowColorIn + i, "Poly Row Color " + std::to_string(i));
            configOutput(x_gateOut + i, "Poly Gates Row " + std::to_string(i));
            configOutput(x_velocityOut + i, "Poly Velocity Row " + std::to_string(i));
        }
    }

    void ReadInputs()
    {
        bool momentary = m_momentaryTrig.process(inputs[x_momentaryIn].getVoltage());
        if (params[x_momentaryParam].getValue() > 0)
        {            
            momentary = !momentary;
        }

        m_state.m_momentary = momentary;

        if (inputs[x_onColorIn].isConnected())
        {
            m_state.m_onColor.Process(Color::FloatToZ(inputs[x_onColorIn].getVoltage() / 10));            
        }
        else
        {
            m_state.m_onColor.Process(Color::FloatToZ(params[x_onColorParam].getValue()));
        }

        if (inputs[x_offColorIn].isConnected())
        {
            m_state.m_offColor.Process(Color::FloatToZ(inputs[x_offColorIn].getVoltage() / 10));            
        }
        else
        {
            m_state.m_offColor.Process(Color::FloatToZ(params[x_offColorParam].getValue()));
        }

        m_state.m_maxPolyphony = static_cast<size_t>(params[x_polyphonyParam].getValue());

        size_t row = 0;
        size_t chan = 0;
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            m_state.m_width[i] = static_cast<size_t>(params[x_widthParam + i].getValue());
            if (inputs[x_rowColorIn + i].isConnected() || !m_overflowPorts)
            {
                row = i;
                chan = 0;
            }
            
            for (size_t j = 0; j < m_state.m_width[i]; ++j)
            {
                if (inputs[x_rowColorIn + row].isConnected() &&
                    static_cast<int>(chan) < inputs[x_rowColorIn + row].getChannels())
                {
                    m_state.m_input[j][i].m_hasDirectColor = true;
                    m_state.m_input[j][i].m_color.Process(Color::FloatToZ(inputs[x_rowColorIn + row].getVoltage(chan) / 10));
                }
                else
                {
                    m_state.m_input[j][i].m_hasDirectColor = false;
                }

                ++chan;
            }
        }
    }

    void SetOutputs()
    {
        size_t row = 0;
        size_t chan = 0;
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            if (outputs[x_gateOut + i].isConnected() || outputs[x_velocityOut + i].isConnected() || !m_overflowPorts)
            {
                row = i;
                chan = 0;
                outputs[x_gateOut + row].setChannels(m_state.m_width[i]);
                outputs[x_velocityOut + row].setChannels(m_state.m_width[i]);
            }
            else
            {
                outputs[x_gateOut + row].setChannels(std::min(outputs[x_gateOut + row].getChannels() + m_state.m_width[i], x_maxPoly));
                outputs[x_velocityOut + row].setChannels(std::min(outputs[x_velocityOut + row].getChannels() + m_state.m_width[i], x_maxPoly));
            }

            
            for (size_t j = 0; j < m_state.m_width[i]; ++j)
            {
                if (chan >= x_maxPoly)
                {
                    break;
                }
                
                ButtonBankInternal::BankedButton* cell = static_cast<ButtonBankInternal::BankedButton*>(m_bank.Get(j, i));
                outputs[x_gateOut + row].setVoltage(cell->m_state ? 10 : 0, chan);
                outputs[x_velocityOut + row].setVoltage(static_cast<float>(cell->m_velocity) * 10 / 127, chan);
                ++chan;
            }
        }

        outputs[x_polyXOut].setChannels(m_bank.m_voiceAllocator.m_maxPolyphony);
        outputs[x_polyYOut].setChannels(m_bank.m_voiceAllocator.m_maxPolyphony);
        outputs[x_polyVelocityOut].setChannels(m_bank.m_voiceAllocator.m_maxPolyphony);
        outputs[x_polyGateOut].setChannels(m_bank.m_voiceAllocator.m_maxPolyphony);

        for (size_t i = 0; i < m_bank.m_voiceAllocator.m_maxPolyphony; ++i)
        {
            CellVoice voice = m_bank.m_voiceAllocator.m_voices[i];
            if (voice.m_gate && *voice.m_gate)
            {
                outputs[x_polyGateOut].setVoltage(10, i);
                outputs[x_polyXOut].setVoltage(voice.m_x, i);
                outputs[x_polyYOut].setVoltage(voice.m_y, i);
                ButtonBankInternal::BankedButton* cell = static_cast<ButtonBankInternal::BankedButton*>(m_bank.Get(voice.m_x, voice.m_y));
                outputs[x_polyVelocityOut].setVoltage(static_cast<float>(cell->m_velocity) * 10 / 127, i);
            }
            else
            {
                outputs[x_polyGateOut].setVoltage(0, i);
                outputs[x_polyXOut].setVoltage(0, i);
                outputs[x_polyYOut].setVoltage(0, i);
                outputs[x_polyVelocityOut].setVoltage(0, i);
            }
        }

        outputs[x_gridIdOut].setVoltage(m_bank.m_gridId);
    }

    void process(const ProcessArgs &args) override
    {
        m_bank.ProcessStatic(args.sampleTime);
        if (m_periodChecker.Process(args.sampleTime))
        {
            ReadInputs();
        }

        m_bank.ProcessInput(m_state);

        SetOutputs();
    }
};

struct ButtonBankWidget : public ModuleWidget
{
    ButtonBankWidget(ButtonBank* module)
    {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ButtonBank.svg")));

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        addParam(createParamCentered<Trimpot>(Vec(50, 100), module, module->x_momentaryParam));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(50, 125), module, module->x_momentaryIn));
        
        addParam(createParamCentered<Trimpot>(Vec(50, 200), module, module->x_onColorParam));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(50, 225), module, module->x_onColorIn));
        addParam(createParamCentered<Trimpot>(Vec(50, 250), module, module->x_offColorParam));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(50, 275), module, module->x_offColorIn));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(50, 350), module, module->x_gridIdOut));

        addParam(createParamCentered<Trimpot>(Vec(300, 200), module, module->x_polyphonyParam));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(300, 225), module, module->x_polyXOut));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(300, 250), module, module->x_polyYOut));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(300, 275), module, module->x_polyVelocityOut));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(300, 300), module, module->x_polyGateOut));
        
        for (size_t i = 0; i < x_baseGridSize; ++i)
        {
            float xPos = 100;
            float yPos = 100 + 25 * i;
            addParam(createParamCentered<Trimpot>(Vec(xPos, yPos), module, module->x_widthParam + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(xPos + 50, yPos), module, module->x_rowColorIn + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(xPos + 100, yPos), module, module->x_gateOut + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(xPos + 150, yPos), module, module->x_velocityOut + i));
        }
    }
};
#endif

}
