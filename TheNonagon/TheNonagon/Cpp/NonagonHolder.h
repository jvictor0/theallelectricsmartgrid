#pragma once

#define IOS_BUILD
#include "TheNonagon.hpp"

struct NonagonGridRouter
{
    TheNonagonSmartGrid* m_nonagon;
    SmartGrid::Grid* m_leftGrid;
    SmartGrid::Grid* m_rightGrid;

    NonagonGridRouter(TheNonagonSmartGrid* nonagon)
    {  
        m_nonagon = nonagon;
        m_leftGrid = m_nonagon->m_lameJuisCoMuteGrid;
        m_rightGrid = m_nonagon->m_theoryOfTimeTopologyGrid;
    }
    
    ~NonagonGridRouter()
    {
    }

    void HandlePress(int x, int y)
    {
        if (x < 8)
        {
            m_leftGrid->OnPress(x, y, 128);
        }
        else
        {
            m_rightGrid->OnPress(x - 8, y, 128);
        }
    }

    void HandleRelease(int x, int y)
    {
        if (x < 8)
        {
            m_leftGrid->OnRelease(x, y);
        }
        else
        {
            m_rightGrid->OnRelease(x - 8, y);
        }
    }

    RGBColor GetColor(int x, int y)
    {
        SmartGrid::Color color;
        if (x < 8)
        {
            color = m_leftGrid->GetColor(x, y);
        }
        else
        {
            color = m_rightGrid->GetColor(x - 8, y);
        }

        return RGBColor(color.m_red, color.m_green, color.m_blue);
    }
};

struct NonagonHolder
{
    TheNonagonSmartGrid m_nonagon;
    NonagonGridRouter m_gridRouter;
    NonagonHolder() 
        : m_gridRouter(&m_nonagon)
    {
    }
    
    ~NonagonHolder() 
    { 
    }

    void HandlePress(int x, int y)
    {
        m_gridRouter.HandlePress(x, y);
    }
    
    void HandleRelease(int x, int y)
    {
        m_gridRouter.HandleRelease(x, y);
    }

    RGBColor GetColor(int x, int y)
    {
        return m_gridRouter.GetColor(x, y);
    }
};  