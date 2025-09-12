#pragma once

#include "SmartGrid.hpp"

struct ShiftedCell : SmartGrid::Cell
{
    std::shared_ptr<SmartGrid::Cell> m_main;
    std::shared_ptr<SmartGrid::Cell> m_shifted;
    bool* m_shift;
    
    ShiftedCell(std::shared_ptr<SmartGrid::Cell> main, std::shared_ptr<SmartGrid::Cell> shifted, bool* shift)
        : m_main(main)
        , m_shifted(shifted)
        , m_shift(shift)
    {
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (*m_shift)
        {
            return m_shifted ? m_shifted->GetColor() : SmartGrid::Color::Off;
        }
        else
        {
            return m_main ? m_main->GetColor() : SmartGrid::Color::Off;
        }
    }

    virtual void OnPress(uint8_t velocity) override
    {
        if (*m_shift)
        {
            if (m_shifted)
            {
                m_shifted->OnPress(velocity);
            }
        }
        else
        {
            if (m_main)
            {
                m_main->OnPress(velocity);
            }
        }
    }

    virtual void OnRelease() override
    {
        if (*m_shift)
        {
            if (m_shifted)
            {
                m_shifted->OnRelease();
            }
        }
        else
        {
            if (m_main)
            {
                m_main->OnRelease();
            }
        }
    }
};