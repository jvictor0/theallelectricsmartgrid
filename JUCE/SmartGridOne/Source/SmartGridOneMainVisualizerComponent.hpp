#pragma once

#include <JuceHeader.h>

struct SmartGridOneMainVisualizerComponent
{
    SmartGridOneMainVisualizerComponent()
    {
    }
    
    // Main drawing method - receives bounds for the block area
    //
    virtual void Draw(juce::Graphics& g, juce::Rectangle<int> bounds) = 0;
    
    // Optional click handler - event position is in block-local coordinates
    //
    virtual void OnClick(const juce::MouseEvent& event)
    {
    }
    
    virtual ~SmartGridOneMainVisualizerComponent() = default;
};
