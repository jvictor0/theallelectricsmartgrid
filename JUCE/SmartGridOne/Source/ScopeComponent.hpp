#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>

struct ScopeComponent : public juce::Component
{
    ScopeReaderFactory m_scopeReaderFactory;
    static constexpr size_t x_numXSamples = 256;
    size_t m_voiceIx;
    size_t m_scopeIx;
    ScopeWriter* m_scopeWriter;

    ScopeComponent(size_t voiceIx, size_t scopeIx, ScopeWriter* scopeWriter)
        : m_scopeReaderFactory(scopeWriter, voiceIx, scopeIx, x_numXSamples, 1)
        , m_voiceIx(voiceIx)
        , m_scopeIx(scopeIx)
        , m_scopeWriter(scopeWriter)
    {
        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);
        
        // Get bounds for drawing
        //
        auto bounds = getLocalBounds().toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();
        
        // Create scope reader
        //
        ScopeReader scopeReader = m_scopeReaderFactory.Create();
        
        // Create path for oscilloscope curve
        //
        juce::Path scopePath;
        bool firstPoint = true;
        
        // Draw the oscilloscope curve
        //
        for (int x = 0; x < x_numXSamples; ++x)
        {
            float y = scopeReader.Get(x);
            
            // Convert to screen coordinates
            // y is typically in range [-1, 1], convert to [0, height]
            //
            float screenX = bounds.getX() + (static_cast<float>(x) / static_cast<float>(x_numXSamples - 1)) * width;
            float screenY = bounds.getY() + height * 0.5f - (y * height * 0.4f);
            
            if (firstPoint)
            {
                scopePath.startNewSubPath(screenX, screenY);
                firstPoint = false;
            }
            else
            {
                scopePath.lineTo(screenX, screenY);
            }
        }
        
        // Draw the curve
        //
        g.setColour(juce::Colours::green);
        g.strokePath(scopePath, juce::PathStrokeType(1.0f));
        
        // Draw center line
        //
        g.setColour(juce::Colours::darkgrey);
        g.drawLine(bounds.getX(), bounds.getY() + height * 0.5f, 
                   bounds.getX() + width, bounds.getY() + height * 0.5f, 1.0f);
    }
};
