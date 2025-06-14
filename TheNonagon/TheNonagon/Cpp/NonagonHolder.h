#pragma once

#define IOS_BUILD
#include "TheNonagon.hpp"
#include "CircularQueue.h"
#include <thread>

struct NonagonGridRouter
{
    TheNonagonSmartGrid* m_nonagon;
    SmartGrid::Grid* m_leftGrid;
    SmartGrid::Grid* m_rightGrid;
    size_t m_gridIndex;

    NonagonGridRouter(TheNonagonSmartGrid* nonagon)
    {  
        m_nonagon = nonagon;
        m_leftGrid = m_nonagon->m_lameJuisCoMuteGrid;
        m_rightGrid = m_nonagon->m_theoryOfTimeTopologyGrid;
        m_gridIndex = 0;
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

    void HandleRightMenuPress(int index)
    {
        if (index == 0)
        {
            m_leftGrid = m_nonagon->m_lameJuisCoMuteGrid;
            m_rightGrid = m_nonagon->m_theoryOfTimeTopologyGrid;
        }
        else if (index == 1)
        {
            m_leftGrid = m_nonagon->m_theoryOfTimeSwingGrid;
            m_rightGrid = m_nonagon->m_lameJuisIntervalGrid;
        }
        else if (index == 2)
        {
            m_leftGrid = m_nonagon->m_timbreAndMuteFireGrid;
            m_rightGrid = m_nonagon->m_indexArpFireGrid;
        }
        else if (index == 3)
        {
            m_leftGrid = m_nonagon->m_timbreAndMuteEarthGrid;
            m_rightGrid = m_nonagon->m_indexArpEarthGrid;
        }
        else if (index == 4)
        {
            m_leftGrid = m_nonagon->m_timbreAndMuteWaterGrid;
            m_rightGrid = m_nonagon->m_indexArpWaterGrid;
        }

        m_gridIndex = index;
    }

    RGBColor GetRightMenuColor(int index)
    {
        if (index == 0)
        {
            return RGBColor(1.0f, 1.0f, 1.0f);
        }
        else if (index == 1)
        {
            return RGBColor(1.0f, 1.0f, 1.0f);
        }
        else if (index == 2)
        {
            return RGBColor(1.0f, 1.0f, 1.0f);
        }
        else if (index == 3)
        {
            return RGBColor(1.0f, 1.0f, 1.0f);
        }
        else if (index == 4)
        {
            return RGBColor(1.0f, 1.0f, 1.0f);
        }

        return RGBColor(0.0f, 0.0f, 0.0f);
    }
};

struct NonagonHolder
{
    TheNonagonSmartGrid m_nonagon;
    NonagonGridRouter m_gridRouter;
    CircularQueue<SmartGrid::Message, 1024> m_ioQueue;
    std::thread m_ioThread;

    NonagonHolder() 
        : m_gridRouter(&m_nonagon)
    {
    }
    
    ~NonagonHolder() 
    { 
    }

    void ProcessMessages()
    {
        SmartGrid::Message message;
        while (m_ioQueue.Pop(message))
        {
            ProcessMessage(message);
        }
    }

    void ProcessMessage(SmartGrid::Message& message)
    {
        if (message.m_velocity > 0)
        {
            m_gridRouter.HandlePress(message.m_x, message.m_y);
        }
        else
        {
            m_gridRouter.HandleRelease(message.m_x, message.m_y);
        }
    }

    void Process(float dt)
    {
        m_nonagon.Process(dt);
        ProcessMessages();
    }

    void HandlePress(int x, int y)
    {
        m_ioQueue.Push(SmartGrid::Message(x, y, 128));
    }
    
    void HandleRelease(int x, int y)
    {
        m_ioQueue.Push(SmartGrid::Message(x, y, 0));
    }

    RGBColor GetColor(int x, int y)
    {
        return m_gridRouter.GetColor(x, y);
    }

    void HandleRightMenuPress(int index)
    {
        m_gridRouter.HandleRightMenuPress(index);
    }

    RGBColor GetRightMenuColor(int index)
    {
        return m_gridRouter.GetRightMenuColor(index);
    }
};  