#pragma once

#include "SmartGrid.hpp"

struct InteriorCell : SmartGrid::Cell
{
    SmartGrid::Grid** m_grid;
    int m_x;
    int m_y;

    InteriorCell(SmartGrid::Grid** grid, int x, int y)
        : m_grid(grid)
        , m_x(x)
        , m_y(y)
    {
    }

    virtual SmartGrid::Color GetColor() override
    {
        return (*m_grid)->GetColor(m_x, m_y);
    }

    virtual void OnPress(uint8_t velocity) override
    {
        (*m_grid)->OnPress(m_x, m_y, velocity);
    }

    virtual void OnRelease() override
    {
        (*m_grid)->OnRelease(m_x, m_y);
    }
};

struct InteriorGridBase : SmartGrid::Grid
{
    SmartGrid::Grid* m_grid;

    InteriorGridBase()
        : m_grid(nullptr)
    {
        for (size_t i = 0; i < SmartGrid::x_baseGridSize; ++i)
        {
            for (size_t j = 0; j < SmartGrid::x_baseGridSize; ++j)
            {
                Put(i, j, new InteriorCell(&m_grid, i, j));
            }
        }
    }

    void SetGrid(SmartGrid::Grid* grid)
    {
        m_grid = grid;
    }
};