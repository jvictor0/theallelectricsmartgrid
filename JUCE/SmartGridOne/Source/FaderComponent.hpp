#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include <atomic>

struct FaderComponent : public juce::Component
{
    static constexpr int x_defaultFaderSize = 20;
    static constexpr int x_defaultFaderLength = 100;

    AnalogUI m_analogUI;
    bool m_isVertical;

    FaderComponent(AnalogUI analogUI, bool isVertical)
        : m_analogUI(analogUI)
        , m_isVertical(isVertical)
    {
        if (m_isVertical)
        {
            setSize(x_defaultFaderSize, x_defaultFaderLength);
        }
        else
        {
            setSize(x_defaultFaderLength, x_defaultFaderSize);
        }
    }

    void SetSize(int faderSize, int faderLength)
    {
        if (m_isVertical)
        {
            setSize(faderSize, faderLength);
        }
        else
        {
            setSize(faderLength, faderSize);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw the fader track background
        //
        g.setColour(juce::Colours::darkgrey);
        if (m_isVertical)
        {
            g.fillRoundedRectangle(bounds.getX() + bounds.getWidth() * 0.4f, bounds.getY(), 
                                 bounds.getWidth() * 0.2f, bounds.getHeight(), 2.0f);
        }
        else
        {
            g.fillRoundedRectangle(bounds.getX(), bounds.getY() + bounds.getHeight() * 0.4f, 
                                 bounds.getWidth(), bounds.getHeight() * 0.2f, 2.0f);
        }

        // Draw the fader knob
        //
        float value = m_analogUI.GetValue();
        float knobPosition;
        float knobSize = m_isVertical ? bounds.getWidth() * 0.8f : bounds.getHeight() * 0.8f;
        
        if (m_isVertical)
        {
            knobPosition = bounds.getY() + (1.0f - value) * (bounds.getHeight() - knobSize / 2);
            g.setColour(juce::Colours::lightgrey);
            g.fillRoundedRectangle(bounds.getX() + bounds.getWidth() * 0.1f, knobPosition, 
                                 bounds.getWidth() * 0.8f, knobSize / 2, knobSize * 0.3f);
            
            // Draw knob border
            //
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(bounds.getX() + bounds.getWidth() * 0.1f, knobPosition, 
                                 bounds.getWidth() * 0.8f, knobSize / 2, knobSize * 0.3f, 1.0f);
        }
        else
        {
            knobPosition = bounds.getX() + value * (bounds.getWidth() - knobSize / 2);
            g.setColour(juce::Colours::lightgrey);
            g.fillRoundedRectangle(knobPosition, bounds.getY() + bounds.getHeight() * 0.1f, 
                                 knobSize / 2, bounds.getHeight() * 0.8f, knobSize * 0.3f);
            
            // Draw knob border
            //
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(knobPosition, bounds.getY() + bounds.getHeight() * 0.1f, 
                                 knobSize / 2, bounds.getHeight() * 0.8f, knobSize * 0.3f, 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        UpdateValueFromMousePosition(event.eventTime.getMilliseconds() * 1000, event.position);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        UpdateValueFromMousePosition(event.eventTime.getMilliseconds() * 1000, event.position);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        UpdateValueFromMousePosition(event.eventTime.getMilliseconds() * 1000, event.position);
    }

    void UpdateValueFromMousePosition(size_t timestamp, const juce::Point<float>& position)
    {
        auto bounds = getLocalBounds().toFloat();
        float newValue;
        
        if (m_isVertical)
        {
            float knobSize = bounds.getWidth() * 0.8f;
            float trackHeight = bounds.getHeight() - knobSize;
            float relativeY = position.y - knobSize * 0.5f;
            newValue = juce::jlimit(0.0f, 1.0f, 1.0f - (relativeY / trackHeight));
        }
        else
        {
            float knobSize = bounds.getHeight() * 0.8f;
            float trackWidth = bounds.getWidth() - knobSize;
            float relativeX = position.x - knobSize * 0.5f;
            newValue = juce::jlimit(0.0f, 1.0f, relativeX / trackWidth);
        }
        
        m_analogUI.SendValue(timestamp, newValue);
        repaint();
    }
};
