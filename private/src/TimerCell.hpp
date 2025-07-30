#pragma once

#include "SmartGrid.hpp"

struct TimerCell : SmartGrid::Cell
{
    static constexpr size_t x_numCells = 16;
    static constexpr double x_blinkTime = 2.0;
    static constexpr double x_totalTime = 20.0 * 60.0;

    double* m_time;
    size_t m_position;

    TimerCell(double* time, size_t position)
        : m_time(time)
        , m_position(position)
    {
    }

    SmartGrid::Color GetCellColor(size_t cell)
    {
        return SmartGrid::Color::Green.Interpolate(SmartGrid::Color::Red, static_cast<double>(cell) / static_cast<double>(x_numCells - 1));
    }

    virtual SmartGrid::Color GetColor() override
    {
        double timeFrac = x_numCells * (*m_time) / x_totalTime;
        double timeRem = timeFrac - std::floor(timeFrac);
        if (timeRem < x_blinkTime / x_numCells)
        {
            if (static_cast<size_t>(timeRem / (x_blinkTime / x_numCells) * 2 * std::floor(1 + timeFrac)) % 2 == 0)
            {
                return GetCellColor(std::floor(timeFrac));
            } 
            else
            {
                return SmartGrid::Color::Off;
            }
        }
        else if (m_position < timeFrac - 1)
        {
            return GetCellColor(m_position);
        }
        else if (m_position < timeFrac)
        {
            return GetCellColor(m_position).AdjustBrightness(timeRem);
        }
        else
        {
            return SmartGrid::Color::Off;
        }
    }
};