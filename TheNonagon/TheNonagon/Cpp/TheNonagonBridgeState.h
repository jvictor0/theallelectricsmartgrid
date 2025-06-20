#pragma once
#include <stdint.h>
#include <os/log.h>
#include <CoreAudio/CoreAudioTypes.h>
#include "RGBColor.h"
#include "NonagonHolder.h"
#include "GridHandle.h"
#include "MidiUtils.h"

struct TheNonagonBridgeState 
{
    TheNonagonBridgeState() 
    {
    }

    ~TheNonagonBridgeState() 
    {
    }
    
    void HandlePress(GridHandle gridHandle, int x, int y)
    {
        m_holder.HandlePress(gridHandle, x, y);
    }

    void HandleRelease(GridHandle gridHandle, int x, int y) 
    {
        m_holder.HandleRelease(gridHandle, x, y);
    }
    
    RGBColor GetColor(GridHandle gridHandle, int x, int y) 
    {
        return m_holder.GetColor(gridHandle, x, y);
    }
    
    void HandleRightMenuPress(int index)
    {
        m_holder.HandleRightMenuPress(index);
    }
    
    RGBColor GetRightMenuColor(int index)
    {
        return m_holder.GetRightMenuColor(index);
    }
    
    void Process(float** audioBuffer, int32_t numChannels, int32_t numFrames, AudioTimeStamp timestamp) 
    {
        for (int32_t frame = 0; frame < numFrames; ++frame)
        {
            UInt64 frameTimestamp = AudioFrameToHostTime(timestamp, frame, 48000.0);
            m_holder.Process(1.0f/48000.0f, frameTimestamp);
            
            for (int32_t channel = 0; channel < numChannels; ++channel)
            {
                audioBuffer[channel][frame] = 0.0f;
            }
        }
    }
    
    void SetMidiInput(int32_t index)
    {
        m_holder.SetMidiInput(index);
    }
    
    void SetMidiOutput(int32_t index)
    {
        m_holder.SetMidiOutput(index);
    }
    
    NonagonHolder m_holder;
}; 