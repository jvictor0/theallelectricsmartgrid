#pragma once
#include "Color.hpp"
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
            : m_color{}
            , m_position(0)
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
        Input()
            : m_encoderInput{}
            , m_brightnessInput{}
            , m_interpInput()
            , m_brightnessConnected{}
            , m_interpConnected{}
        {
        }

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

}
