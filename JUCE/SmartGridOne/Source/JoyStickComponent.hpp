#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include <atomic>

struct JoyStickComponent : public juce::Component
{
    static constexpr int x_defaultJoyStickSize = 80;

    AnalogUI m_xPositive;
    AnalogUI m_xNegative;
    AnalogUI m_yPositive;
    AnalogUI m_yNegative;
    bool m_return;

    JoyStickComponent(AnalogUI xPositive, AnalogUI xNegative, AnalogUI yPositive, AnalogUI yNegative, bool ret)
        : m_xPositive(xPositive)
        , m_xNegative(xNegative)
        , m_yPositive(yPositive)
        , m_yNegative(yNegative)
        , m_return(ret)
    {
        setSize(x_defaultJoyStickSize, x_defaultJoyStickSize);
    }

    void SetSize(int size)
    {
        setSize(size, size);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

        // Draw the outer circle (joystick base)
        //
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
        
        // Draw the outer circle border
        //
        g.setColour(juce::Colours::lightgrey);
        g.drawEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2, 2.0f);

        // Calculate joystick position based on analog values
        //
        float xValue = m_xPositive.GetValue() - m_xNegative.GetValue();
        float yValue = m_yPositive.GetValue() - m_yNegative.GetValue();
        
        // Clamp values to [-1, 1] range
        //
        xValue = juce::jlimit(-1.0f, 1.0f, xValue);
        yValue = juce::jlimit(-1.0f, 1.0f, yValue);

        // Calculate knob position
        //
        float knobRadius = radius * 0.3f;
        float knobX = centerX + (xValue * (radius - knobRadius));
        float knobY = centerY - (yValue * (radius - knobRadius)); // Invert Y for screen coordinates

        // Draw the joystick knob
        //
        g.setColour(juce::Colours::white);
        g.fillEllipse(knobX - knobRadius, knobY - knobRadius, knobRadius * 2, knobRadius * 2);
        
        // Draw knob border
        //
        g.setColour(juce::Colours::black);
        g.drawEllipse(knobX - knobRadius, knobY - knobRadius, knobRadius * 2, knobRadius * 2, 1.0f);

        // Draw center crosshairs for reference
        //
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.drawLine(centerX - radius * 0.3f, centerY, centerX + radius * 0.3f, centerY, 1.0f);
        g.drawLine(centerX, centerY - radius * 0.3f, centerX, centerY + radius * 0.3f, 1.0f);
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
        if (m_return)
        {
            size_t timestamp = event.eventTime.getMilliseconds() * 1000;
            m_xPositive.SendValue(timestamp, 0.0f);
            m_xNegative.SendValue(timestamp, 0.0f);
            m_yPositive.SendValue(timestamp, 0.0f);
            m_yNegative.SendValue(timestamp, 0.0f);
        }
        else
        {
            UpdateValueFromMousePosition(event.eventTime.getMilliseconds() * 1000, event.position);
        }
    }

    void UpdateValueFromMousePosition(size_t timestamp, const juce::Point<float>& position)
    {
        auto bounds = getLocalBounds().toFloat();
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

        // Calculate relative position from center
        //
        float relativeX = (position.x - centerX) / radius;
        float relativeY = (centerY - position.y) / radius; // Invert Y for screen coordinates

        // Clamp to [-1, 1] range
        //
        float xValue = juce::jlimit(-1.0f, 1.0f, relativeX);
        float yValue = juce::jlimit(-1.0f, 1.0f, relativeY);

        // Convert to positive/negative values for each axis
        //
        float xPositive = xValue > 0.0f ? xValue : 0.0f;
        float xNegative = xValue < 0.0f ? -xValue : 0.0f;
        float yPositive = yValue > 0.0f ? yValue : 0.0f;
        float yNegative = yValue < 0.0f ? -yValue : 0.0f;

        // Send values to analog UIs
        //
        m_xPositive.SendValue(timestamp, xPositive);
        m_xNegative.SendValue(timestamp, xNegative);
        m_yPositive.SendValue(timestamp, yPositive);
        m_yNegative.SendValue(timestamp, yNegative);

        repaint();
    }
};
