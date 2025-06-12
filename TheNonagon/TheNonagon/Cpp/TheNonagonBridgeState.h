#pragma once
#include <stdint.h>
#include <os/log.h>
#include "RGBColor.h"
#include "NonagonHolder.h"

struct TheNonagonBridgeState 
{
    TheNonagonBridgeState() 
    {
    }

    ~TheNonagonBridgeState() 
    {
    }
    
    void HandlePress(int x, int y)
    {
        m_holder.HandlePress(x, y);
    }

    void HandleRelease(int x, int y) 
    {
        m_holder.HandleRelease(x, y);
    }
    
    RGBColor GetColor(int x, int y) 
    {
        return m_holder.GetColor(x, y);
    }
    
    void Process() 
    {
        m_holder.m_nonagon.Process(1.0f/60.0f, 0);
    }
    
    NonagonHolder m_holder;
}; 