#pragma once
#include <stdint.h>
#include <os/log.h>
#include "RGBColor.h"
#include "NonagonHolder.h"

struct TheNonagonBridgeState 
{
    TheNonagonBridgeState() 
      : m_counter(0) 
    {}
    
    ~TheNonagonBridgeState() {}
    
    void HandlePress(int x, int y) {}
    void HandleRelease(int x, int y) {}
    
    RGBColor GetColor(int x, int y) 
    {
        // Simple proof of concept: animate red and green channels based on x, y, and m_counter
        uint8_t r = (uint8_t)((x * 16 + m_counter) % 256);
        uint8_t g = (uint8_t)((y * 32 + m_counter) % 256);
        uint8_t b = (uint8_t)((m_counter + x + y) % 256);
        return RGBColor(r, g, b);
    }
    
    void Process() 
    {
        m_counter++;
    }
    
    uint32_t m_counter;
}; 