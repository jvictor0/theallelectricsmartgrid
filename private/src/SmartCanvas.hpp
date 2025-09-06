#pragma once
#include "SmartGrid.hpp"
#include "StateSaver.hpp"
#include "plugin.hpp"

namespace SmartGrid
{

struct SmartCanvasInternal : public AbstractGrid
{
#ifndef SMART_BOX
    static constexpr int x_gridXSize = 8;
    static constexpr int x_gridYSize = 8;
#else
    static constexpr int x_gridXSize = 20;
    static constexpr int x_gridYSize = 8;
#endif
    
    virtual ~SmartCanvasInternal()
    {
    }
    
    struct Elem
    {
        size_t m_gridId;
        int m_srcX;
        int m_srcY;
        int m_trgX;
        int m_trgY;
        int m_lenX;
        int m_lenY;

        bool m_flipX;
        bool m_flipY;
        int m_rotate;

        Elem()
            : m_gridId(x_numGridIds)
            , m_srcX(0)
            , m_srcY(0)
            , m_trgX(0)
            , m_trgY(0)
            , m_lenX(x_gridXSize)
            , m_lenY(x_gridYSize)
            , m_flipX(false)
            , m_flipY(false)
            , m_rotate(0)
        {
        }

        std::pair<int, int> ApplyFlipAndRotate(int x, int y)
        {
            if (m_flipX)
            {
                x = x_gridXSize - x - 1;
            }

            if (m_flipY)
            {
                y = x_gridYSize - y - 1;
            }

            int rotate = m_rotate;
            while (rotate > 0)
            {
                int xp = y;
                int yp = x_gridYSize - x - 1;
                x = xp;
                y = yp;
                --rotate;
            }

            return std::pair<int, int>(x, y);                
        }

        std::pair<int, int> GetSrcCoords(int x, int y, bool* outOfBounds)
        {
            x = x - m_trgX + m_srcX;
            y = y - m_trgY + m_srcY;
            if (x < 0 || x_gridXSize <= x || y < 0 || x_gridYSize <= y)
            {
                *outOfBounds = true;
                return std::pair<int, int>(0, 0);
            }

            *outOfBounds = false;
            return ApplyFlipAndRotate(x, y);
        }

        bool HasGrid()
        {
            return m_gridId != x_numGridIds;
        }
        
        bool InBounds(int x, int y)
        {
            return m_trgX <= x && x < m_trgX + m_lenX &&
                   m_trgY <= y && y < m_trgY + m_lenY &&
                   0 <= x && x < x_gridXSize &&
                   0 <= y && y < x_gridYSize;
        }
        
        struct Input
        {
            size_t m_gridId;
            int m_srcX;
            int m_srcY;
            int m_trgX;
            int m_trgY;
            int m_lenX;
            int m_lenY;
            
            bool m_flipX;
            bool m_flipY;
            int m_rotate;

            Input()
                : m_gridId(x_numGridIds)
                , m_srcX(0)
                , m_srcY(0)
                , m_trgX(0)
                , m_trgY(0)
                , m_lenX(x_gridXSize)
                , m_lenY(x_gridYSize)
                , m_flipX(false)
                , m_flipY(false)
                , m_rotate(0)
            {
            }
        };

        void Process(Input& input)
        {
            m_gridId = input.m_gridId;
            m_srcX = input.m_srcX;
            m_srcY = input.m_srcY;
            m_trgX = input.m_trgX;
            m_trgY = input.m_trgY;
            m_lenX = input.m_lenX;
            m_lenY = input.m_lenY;
            m_flipX = input.m_flipX;
            m_flipY = input.m_flipY;
            m_rotate = input.m_rotate;
        }
    };

    static constexpr size_t x_numElems = 8;
    
    Color m_onColor;
    Elem m_elems[x_numElems];

    struct Input
    {
        Elem::Input m_inputs[x_numElems];
        ColorDecode m_onColor;

        Input()
            : m_onColor(Color::White)
        {
        }
    };

    void ProcessInput(Input& input)
    {
        m_onColor = input.m_onColor.m_color;
        for (size_t i = 0; i < x_numElems; ++i)
        {
            m_elems[i].Process(input.m_inputs[i]);
        }        
    }

    virtual void Apply(Message msg)
    {
        for (size_t i = 0; i < x_numElems; ++i)
        {
            if (m_elems[i].HasGrid() &&
                m_elems[i].InBounds(msg.m_x, msg.m_y))
            {
                bool oob = false;
                std::pair<int, int> coords = m_elems[i].GetSrcCoords(msg.m_x, msg.m_y, &oob);
                if (!oob)
                {
                    SmartBusPutVelocity(m_elems[i].m_gridId, coords.first, coords.second, msg.m_velocity);
                    break;
                }
            }
        }
    }
    
    virtual Color GetColor(int x, int y)
    {
        for (size_t i = 0; i < x_numElems; ++i)
        {
            if (m_elems[i].HasGrid() &&
                m_elems[i].InBounds(x, y))
            {
                bool oob = false;
                std::pair<int, int> coords = m_elems[i].GetSrcCoords(x, y, &oob);
                if (!oob)
                {
                    return SmartBusGetColor(m_elems[i].m_gridId, coords.first, coords.second);
                }
            }
        }

        return Color::Off;
    }
    
    virtual Color GetOnColor()
    {
        return m_onColor;
    }

    virtual Color GetOffColor()
    {
        return m_onColor.Dim();
    }
};

#ifndef IOS_BUILD
struct SmartCanvas : Module
{
    SmartCanvasInternal m_canvas;
    SmartCanvasInternal::Input m_state;
    PeriodChecker m_periodChecker;

    static constexpr size_t x_srcXYParam = 0;
    static constexpr size_t x_trgXYParam = x_srcXYParam + 2 * SmartCanvasInternal::x_numElems;
    static constexpr size_t x_lenXYParam = x_trgXYParam + 2 * SmartCanvasInternal::x_numElems;
    static constexpr size_t x_flipXYParam = x_lenXYParam + 2 * SmartCanvasInternal::x_numElems;
    static constexpr size_t x_rotateParam = x_flipXYParam + 2 * SmartCanvasInternal::x_numElems;
    static constexpr size_t x_colorParam = x_rotateParam + SmartCanvasInternal::x_numElems;
    static constexpr size_t x_numParams = x_colorParam + 1;

    static constexpr size_t x_srcXYIn = 0;
    static constexpr size_t x_gridIdIn = x_srcXYIn + 2 * SmartCanvasInternal::x_numElems;
    static constexpr size_t x_colorIn = x_gridIdIn + SmartCanvasInternal::x_numElems;
    static constexpr size_t x_numIns = x_colorIn + 1;

    static constexpr size_t x_gridIdOut = 0;
    static constexpr size_t x_numOuts = x_gridIdOut + 1;

    SmartCanvas()
    {
        config(x_numParams, x_numIns, x_numOuts, 0);

        for (size_t i = 0; i < SmartCanvasInternal::x_numElems; ++i)
        {
            for (size_t j = 0; j < 2; ++j)
            {
                std::string xy = j == 0 ? "X" : "Y";
                size_t offset = 2 * i + j;
                size_t size = j == 0 ? SmartCanvasInternal::x_gridXSize : SmartCanvasInternal::x_gridYSize;
                configSwitch(x_srcXYParam + offset, 0, size - 1, 0, "Source " + xy + " Pos Element " + std::to_string(i));
                configSwitch(x_trgXYParam + offset, 0, size - 1, 0, "Target " + xy + " Pos Element " + std::to_string(i));
                configSwitch(x_lenXYParam + offset, 0, size, size, (j == 0 ? "Width Element " : "Height Element ") + std::to_string(i));
                configSwitch(x_flipXYParam + offset, 0, 1, 0, "Flip " + xy + " Element " + std::to_string(i));

                configInput(x_srcXYIn + offset, xy + " CV Element " + std::to_string(i));
            }

            configSwitch(x_rotateParam + i, 0, 3, 0, "Rotate Element " + std::to_string(i));
            configInput(x_gridIdIn + i, "Grid Id Element " + std::to_string(i));
        }

        configOutput(x_gridIdOut, "Grid Id");
    }

    void ReadInputs()
    {
        for (size_t i = 0; i < SmartCanvasInternal::x_numElems; ++i)
        {
            if (!inputs[x_gridIdIn + i].isConnected() ||
                inputs[x_gridIdIn + i].getVoltage() > x_numGridIds)
            {
                m_state.m_inputs[i].m_gridId = x_numGridIds;
                continue;
            }

            m_state.m_inputs[i].m_gridId = static_cast<size_t>(inputs[x_gridIdIn + i].getVoltage());
            
            m_state.m_inputs[i].m_srcX = static_cast<int>(params[x_srcXYParam + 2 * i].getValue());
            m_state.m_inputs[i].m_srcX += static_cast<int>(inputs[x_srcXYIn + 2 * i].getVoltage());
            m_state.m_inputs[i].m_trgX = static_cast<int>(params[x_trgXYParam + 2 * i].getValue());
            m_state.m_inputs[i].m_lenX = static_cast<int>(params[x_lenXYParam + 2 * i].getValue());
            m_state.m_inputs[i].m_flipX = static_cast<int>(params[x_flipXYParam + 2 * i].getValue());

            m_state.m_inputs[i].m_srcY = static_cast<int>(params[x_srcXYParam + 2 * i + 1].getValue());
            m_state.m_inputs[i].m_srcY += static_cast<int>(inputs[x_srcXYIn + 2 * i + 1].getVoltage());
            m_state.m_inputs[i].m_trgY = static_cast<int>(params[x_trgXYParam + 2 * i + 1].getValue());
            m_state.m_inputs[i].m_lenY = static_cast<int>(params[x_lenXYParam + 2 * i + 1].getValue());
            m_state.m_inputs[i].m_flipY = static_cast<int>(params[x_flipXYParam + 2 * i + 1].getValue());

            m_state.m_inputs[i].m_rotate = static_cast<int>(params[x_rotateParam + i].getValue());
        }

        if (inputs[x_colorIn].isConnected())
        {
            m_state.m_onColor.ProcessVoltage(inputs[x_colorIn].getVoltage());
        }
        else
        {
            m_state.m_onColor.ProcessFloat(params[x_colorParam].getValue());
        }
    }

    void SetOutputs()
    {
        outputs[x_gridIdOut].setVoltage(m_canvas.m_gridId);
    }

    void process(const ProcessArgs &args) override
    {
        m_canvas.ProcessStatic(args.sampleTime);
        if (m_periodChecker.Process(args.sampleTime))
        {
            ReadInputs();
        }

        m_canvas.ProcessInput(m_state);

        SetOutputs();
    }
};

struct SmartCanvasWidget : ModuleWidget
{
    SmartCanvasWidget(SmartCanvas* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SmartCanvas.svg")));

        for (size_t i = 0; i < SmartCanvasInternal::x_numElems; ++i)
        {
            int yPos = 100 + 25 * i;

            for (size_t j = 0; j < 2; ++j)
            {
                size_t offset = 2 * i + j;
                addParam(createParamCentered<Trimpot>(Vec(100 + 25 * j, yPos), module, module->x_srcXYParam + offset));
                addInput(createInputCentered<PJ301MPort>(Vec(150 + 25 * j, yPos), module, module->x_srcXYIn + offset));
                addParam(createParamCentered<Trimpot>(Vec(225 + 25 * j, yPos), module, module->x_trgXYParam + offset));
                addParam(createParamCentered<Trimpot>(Vec(300 + 25 * j, yPos), module, module->x_lenXYParam + offset));
                addParam(createParamCentered<Trimpot>(Vec(375 + 25 * j, yPos), module, module->x_flipXYParam + offset));
            }

            addInput(createInputCentered<PJ301MPort>(Vec(50, yPos), module, module->x_gridIdIn + i));
            addParam(createParamCentered<Trimpot>(Vec(425, yPos), module, module->x_rotateParam + i));
        }

        addOutput(createOutputCentered<PJ301MPort>(Vec(50, 300), module, module->x_gridIdOut));
    }
};
#endif

}
