#pragma once
#include "plugin.hpp"

namespace SmartGrid
{

struct GridWidget : LightWidget
{
    int m_x;
    int m_y;
    size_t* m_gridId;

    GridWidget()
        : m_x(0), m_y(0), m_gridId(nullptr)
    {
    }
    
    GridWidget(int x, int y, size_t* gridId)
        : m_x(x), m_y(y), m_gridId(gridId)
    {
    }

    void draw(const DrawArgs& args) override
    {
        Color color = Color::Off;
        if (m_gridId)
        {
            size_t gridId = *m_gridId;
            if (gridId < x_numGridIds)
            {
                color = g_smartBus.GetColor(gridId, m_x, m_y);
            }
        }

        NVGcolor nvgc = nvgRGBA(color.m_red, color.m_green, color.m_blue, 255);
        NVGcolor offColor = nvgRGBA(77, 77, 77, 100);
        if (color == Color::Off)
        {
            nvgc = offColor;
        }

        auto gradient = nvgRadialGradient(
            args.vg,
            box.size.x / 2,
            box.size.y / 2,
            box.size.y / 10,
            box.size.x * 0.75f,
            nvgc,
            offColor);

        nvgBeginPath(args.vg);
        nvgFillPaint(args.vg, gradient);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, box.size.x / 10.0f);
        nvgFill(args.vg);
    }
};

inline void AddSmartGridWidget(
    ModuleWidget* widget,
    float xOff,
    float yOff,
    size_t* gridId)
{
    for (size_t i = 0; i < x_gridSize; ++i)
    {
        for (size_t j = 0; j < x_gridSize; ++j)
        {
            Vec pos = Vec(xOff + i * 25 - 12.5, yOff - j * 25 + 12.5);
            auto* gridWidget = createWidget<GridWidget>(pos);
            gridWidget->box.size = Vec(25, 25);
            gridWidget->m_x = i;
            gridWidget->m_y = j;
            gridWidget->m_gridId = gridId;
            widget->addChild(gridWidget);
        }
    }
}

}
