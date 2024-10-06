#pragma once
#include "SmartGrid.hpp"
#include "plugin.hpp"
#include <algorithm>

namespace SmartGrid
{

struct ColorEncode
{
    ColorEncode()
        : m_encodedColor(0)
    {
    }

    Color m_color;
    size_t m_encodedColor;

    size_t Process(Color c)
    {
        if (m_color != c)
        {
            m_color = c;
            m_encodedColor = c.ZEncode();
        }

        return m_encodedColor;
    }
};

struct ColorDecode
{
    size_t m_encoded;
    Color m_color;

    ColorDecode()
        : m_encoded(0)
    {
    }

    ColorDecode(Color c)
        : m_encoded(c.ZEncode())
        , m_color(c)
    {
    }

    Color Process(size_t encoded)
    {
        if (m_encoded != encoded)
        {
            m_encoded = encoded;
            m_color = Color::ZDecode(encoded);
        }

        return m_color;
    }

    Color ProcessFloat(float value)
    {
        return Process(Color::FloatToZ(value));
    }

    Color ProcessVoltage(float voltage)
    {
        return ProcessFloat(voltage / 10);
    }

    void Hookup(ColorEncode encode)
    {
        m_encoded = encode.m_encodedColor;
        m_color = encode.m_color;
    }
};

struct BrightnessAdjuster
{
    struct Input
    {
        Input()
            : m_encodedColor(0)
            , m_brightness(1)
        {
        }
        
        size_t m_encodedColor;
        float m_brightness;
    };

    BrightnessAdjuster()
        : m_brightness(0)
    {
    }

    ColorDecode m_preColor;
    float m_brightness;
    ColorEncode m_encodedColor;

    size_t Process(Input& input)
    {
        if (m_preColor.m_encoded != input.m_encodedColor || m_brightness != input.m_brightness)
        {
            Color c = m_preColor.Process(input.m_encodedColor);
            m_encodedColor.Process(c.AdjustBrightness(input.m_brightness));
        }

        return m_encodedColor.m_encodedColor;
    }    
};

struct ColorInterpolator
{
    struct Input
    {
        Input()
            : m_position(0)
        {
            m_color[0] = 0;
            m_color[1] = 0;
        }

        size_t m_color[2];
        float m_position;
    };

    ColorDecode m_color[2];
    ColorEncode m_encodedColor;

    void Process(Input& input)
    {
        Color c1 = m_color[0].Process(input.m_color[0]);
        Color c2 = m_color[1].Process(input.m_color[1]);
        m_encodedColor.Process(c1.Interpolate(c2, input.m_position));
    }
};

struct ColorHelperChannel
{
    ColorEncode m_encoders[2];
    BrightnessAdjuster m_brightness[2];
    ColorInterpolator m_interp;

    struct Input
    {
        Color m_encoderInput[2];
        BrightnessAdjuster::Input m_brightnessInput[2];
        ColorInterpolator::Input m_interpInput;
        bool m_brightnessConnected[2];
        bool m_interpConnected[2];
    };

    void Process(Input& input)
    {
        for (size_t i = 0; i < 2; ++i)
        {
            size_t encoded = m_encoders[i].Process(input.m_encoderInput[i]);
            if (!input.m_brightnessConnected[i])
            {
                m_brightness[i].m_preColor.Hookup(m_encoders[i]);
                input.m_brightnessInput[i].m_encodedColor = encoded;
            }

            encoded = m_brightness[i].Process(input.m_brightnessInput[i]);                

            if (!input.m_interpConnected[i])
            {
                input.m_interpInput.m_color[i] = encoded;
                m_interp.m_color[i].Hookup(m_brightness[i].m_encodedColor);
            }
        }

        m_interp.Process(input.m_interpInput);
    }
};

struct ColorHelper : Module
{
    static constexpr size_t x_maxPoly = 16;
    ColorHelperChannel::Input m_state[x_maxPoly];
    ColorHelperChannel m_channels[x_maxPoly];
    PeriodChecker m_checker;

    static constexpr size_t x_redParam = 0;
    static constexpr size_t x_greenParam = x_redParam + 2;
    static constexpr size_t x_blueParam = x_greenParam + 2;
    static constexpr size_t x_brightnessParam = x_blueParam + 2;
    static constexpr size_t x_brightnessAttn = x_brightnessParam + 2;
    static constexpr size_t x_interpParam = x_brightnessAttn + 2;
    static constexpr size_t x_numParams = x_interpParam + 1;

    static constexpr size_t x_redIn = 0;
    static constexpr size_t x_greenIn = x_redIn + 2;
    static constexpr size_t x_blueIn = x_greenIn + 2;
    static constexpr size_t x_brightnessColorIn = x_blueIn + 2;
    static constexpr size_t x_brightnessIn = x_brightnessColorIn + 2;
    static constexpr size_t x_interpColorIn = x_brightnessIn + 2;
    static constexpr size_t x_interpIn = x_interpColorIn + 2;
    static constexpr size_t x_numIns = x_interpIn + 1;

    static constexpr size_t x_colorOut = 0;
    static constexpr size_t x_brightColorOut = x_colorOut + 2;
    static constexpr size_t x_interpOut = x_brightColorOut + 2;
    static constexpr size_t x_numOuts = x_interpOut + 1;
    
    ColorHelper()
        : m_checker(0.001)
    {
        config(x_numParams, x_numIns, x_numOuts, 0);

        for (size_t i = 0; i < 2; ++i)
        {
            configParam(x_redParam, 0, 1, 1, "Red " + std::to_string(i + 1));
            configParam(x_greenParam, 0, 1, 0, "Green " + std::to_string(i + 1));
            configParam(x_blueParam, 0, 1, 0, "Blue " + std::to_string(i + 1));
            configParam(x_brightnessParam, 0, 1, 0, "Brightness " + std::to_string(i + 1));
            configParam(x_brightnessAttn, -1, 1, 0, "Brightness Attn" + std::to_string(i + 1));

            configInput(x_redIn, "Red " + std::to_string(i + 1));
            configInput(x_greenIn, "Green " + std::to_string(i + 1));
            configInput(x_blueIn, "Red " + std::to_string(i + 1));
            configInput(x_brightnessColorIn, "Brightness Adjustor Color" + std::to_string(i + 1));
            configInput(x_brightnessIn, "Brightness " + std::to_string(i + 1));
            configInput(x_interpColorIn, "Interpolator color " + std::to_string(i + 1));

            configOutput(x_colorOut, "Color " + std::to_string(i + 1));
            configOutput(x_brightColorOut, "Bright Adjusted " + std::to_string(i + 1));
        }

        configParam(x_interpParam, 0, 1, 0, "Interpolate");
        configInput(x_interpIn, "Interpolate");
        configOutput(x_interpOut, "Interpolated Color");
    }

    float GetVal(size_t inId, ssize_t attnId, size_t paramId, size_t chan)
    {
        float ret = 0;

        if (inputs[inId].isConnected())
        {
            ret += inputs[inId].getVoltage(chan % inputs[inId].getChannels()) / 10;
            if (attnId != -1)
            {
                ret *= params[attnId].getValue();
            }
        }
        
        ret += params[paramId].getValue();
        
        return ret;
    }

    uint8_t GetColorComponent(size_t inId, size_t paramId, size_t chan)
    {
        float preRet = GetVal(inId, -1, paramId, chan);
        return std::max<int>(0, std::min<int>(preRet * 256, 255));
    }
    
    void process(const ProcessArgs &args) override
    {
        if (m_checker.Process(args.sampleTime))
        {
            size_t poly = 1;
            for (size_t i = 0; i < x_numIns; ++i)
            {
                poly = std::max<size_t>(poly, inputs[i].getChannels());
            }
            
            for (size_t i = 0; i < poly; ++i)
            {
                for (size_t j = 0; j < 2; ++j)
                {
                    m_state[i].m_encoderInput[j].m_red = GetColorComponent(x_redIn + j, x_redParam + j, i);
                    m_state[i].m_encoderInput[j].m_green = GetColorComponent(x_greenIn + j, x_greenParam + j, i);
                    m_state[i].m_encoderInput[j].m_blue = GetColorComponent(x_blueIn + j, x_blueParam + j, i);
                    m_state[i].m_brightnessInput[j].m_brightness = GetVal(x_brightnessIn + j, x_brightnessAttn + j, x_brightnessParam + j, i);
                    if (inputs[x_brightnessColorIn + j].isConnected())
                    {
                        m_state[i].m_brightnessInput[j].m_encodedColor = Color::FloatToZ(inputs[x_brightnessColorIn + j].getVoltage(i) / 10);
                        m_state[i].m_brightnessConnected[j] = true;
                    }
                    else
                    {
                        m_state[i].m_brightnessConnected[j] = false;
                    }

                    if (inputs[x_interpColorIn + j].isConnected())
                    {
                        m_state[i].m_interpInput.m_color[j] = Color::FloatToZ(inputs[x_interpColorIn + j].getVoltage(i) / 10);
                        m_state[i].m_interpConnected[j] = true;
                    }
                    else
                    {
                        m_state[i].m_interpConnected[j] = false;
                    }
                }

                m_state[i].m_interpInput.m_position = GetVal(x_interpIn, -1, x_interpParam, i);
                
                m_channels[i].Process(m_state[i]);
            }

            size_t interpPoly = std::max(1, inputs[x_interpIn].getChannels());
            
            for (size_t j = 0; j < 2; ++j)
            {
                size_t colorPoly = std::max<size_t>(
                    std::max<size_t>(1, inputs[x_redIn + j].getChannels()),
                    std::max<size_t>(inputs[x_greenIn + j].getChannels(), inputs[x_blueIn + j].getChannels()));

                outputs[x_colorOut + j].setChannels(colorPoly);
                for (size_t i = 0; i < colorPoly; ++i)
                {
                    float volt = Color::ZToFloat(m_channels[i].m_encoders[j].m_encodedColor) * 10;
                    outputs[x_colorOut + j].setVoltage(volt, i);
                }

                size_t brightPoly = inputs[x_brightnessColorIn + j].isConnected()
                    ? static_cast<size_t>(inputs[x_brightnessColorIn + j].getChannels())
                    : colorPoly;
                
                outputs[x_brightColorOut + j].setChannels(brightPoly);
                for (size_t i = 0; i < brightPoly; ++i)
                {
                    float volt = Color::ZToFloat(m_channels[i].m_brightness[j].m_encodedColor.m_encodedColor) * 10;
                    outputs[x_brightColorOut + j].setVoltage(volt, i);
                }

                if (inputs[x_interpColorIn + j].isConnected())
                {
                    interpPoly = std::max<size_t>(interpPoly, brightPoly);
                }
                else
                {
                    interpPoly = std::max<size_t>(interpPoly, inputs[x_interpColorIn + j].getChannels());
                }
            }

            outputs[x_interpOut].setChannels(interpPoly);
            for (size_t i = 0; i < interpPoly; ++i)
            {
                float volt = Color::ZToFloat(m_channels[i].m_interp.m_encodedColor.m_encodedColor) * 10;
                outputs[x_interpOut].setVoltage(volt, i);
            }
        }
    }
};

struct ColorHelperWidget : ModuleWidget
{
    ColorHelperWidget(ColorHelper* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/ColorHelper.svg")));

        for (size_t i = 0; i < 2; ++i)
        {
            int yPos = 100 + 100 * i;
            int pPos = yPos + 25;
            addInput(createInputCentered<PJ301MPort>(Vec(50, yPos), module, module->x_redIn + i));
            addParam(createParamCentered<Trimpot>(Vec(50, pPos), module, module->x_redParam + i));
            addInput(createInputCentered<PJ301MPort>(Vec(75, yPos), module, module->x_greenIn + i));
            addParam(createParamCentered<Trimpot>(Vec(75, pPos), module, module->x_greenParam + i));
            addInput(createInputCentered<PJ301MPort>(Vec(100, yPos), module, module->x_blueIn + i));
            addParam(createParamCentered<Trimpot>(Vec(100, pPos), module, module->x_blueParam + i));

            addOutput(createOutputCentered<PJ301MPort>(Vec(125, yPos), module, module->x_colorOut + i));

            addInput(createInputCentered<PJ301MPort>(Vec(175, yPos), module, module->x_brightnessColorIn + i));
            addParam(createParamCentered<Trimpot>(Vec(175, pPos), module, module->x_brightnessParam + i));
            addInput(createInputCentered<PJ301MPort>(Vec(200, yPos), module, module->x_brightnessIn + i));
            addParam(createParamCentered<Trimpot>(Vec(200, pPos), module, module->x_brightnessAttn + i));

            addOutput(createOutputCentered<PJ301MPort>(Vec(225, yPos), module, module->x_brightColorOut + i));

            addInput(createInputCentered<PJ301MPort>(Vec(275, yPos), module, module->x_interpColorIn + i));            
        }

        int yPos = 300;
        addInput(createInputCentered<PJ301MPort>(Vec(200, yPos), module, module->x_interpIn));
        addParam(createParamCentered<Trimpot>(Vec(225, yPos), module, module->x_interpParam));        
        addOutput(createOutputCentered<PJ301MPort>(Vec(250, yPos), module, module->x_interpOut));
    }
};

}
