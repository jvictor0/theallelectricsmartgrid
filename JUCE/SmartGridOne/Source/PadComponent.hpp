#pragma once

#include <JuceHeader.h>
#include "SmartGridInclude.hpp"
#include <atomic>

struct PadComponent : public juce::Component
{
    static constexpr int x_defaultPadSize = 50;
    
    PadUI m_padUI;

    enum class Skin
    {
        Pad,
        AcradeButton,
    };

    Skin m_skin;

    bool m_isPressed;
    
    PadComponent(PadUI padUI)
        : m_padUI(padUI)
        , m_skin(Skin::Pad)
        , m_isPressed(false)
    {
        setSize(x_defaultPadSize, x_defaultPadSize);
    }
    
    void SetSize(int width, int height)
    {
        setSize(width, height);
    }
    
    void paint(juce::Graphics& g) override
    {
        switch (m_skin)
        {
            case Skin::Pad:
            {
                PaintPad(g);
                break;
            }
            case Skin::AcradeButton:
            {
                PaintAcradeButton(g);
                break;
            }
        }
    }

    void PaintPad(juce::Graphics& g)
    {
        // Get the current color from the atomic pointer
        //
        SmartGrid::Color currentColor = m_padUI.GetColor();
        
        // Convert SmartGrid::Color to JUCE Colour
        //
        juce::Colour juceColor = juce::Colour(
            currentColor.m_red,
            currentColor.m_green, 
            currentColor.m_blue
        );

        // Calculate brightness for LED effect - ensure minimum visibility
        //
        float brightness = static_cast<float>(std::max(currentColor.m_red, std::max(currentColor.m_green, currentColor.m_blue))) / 255.0f;
        brightness = std::max(0.2f, brightness); // Ensure minimum 20% brightness
        
        // Draw the pad background with rounded corners
        //
        g.setColour(juce::Colours::darkgrey);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        
        // Draw the LED effect - brighter color = brighter LED
        //
        if (currentColor != SmartGrid::Color::Off)
        {
            // Create a radial gradient for LED effect
            //
            juce::ColourGradient gradient(
                juceColor.withAlpha(1.0f),
                getWidth() / 2.0f,
                getHeight() / 2.0f,
                juceColor.withAlpha(1.0f),
                getWidth() / 2.0f,
                getHeight() / 2.0f,
                true
            );
            
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        }
        
        // Draw the pad border
        //
        g.setColour(juce::Colours::lightgrey);
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 8.0f, 1.0f);
        
        // Draw a subtle highlight on top
        //
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(getLocalBounds().reduced(2).toFloat(), 6.0f);
    }

    void PaintAcradeButton(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        
        // Calculate pressed offset for 3D effect
        //
        float pressOffset = m_isPressed ? 2.0f : 0.0f;
        float shadowOffset = m_isPressed ? 1.0f : 3.0f;
        
        // Draw shadow/bevel effect
        //
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(centerX - radius + shadowOffset, centerY - radius + shadowOffset, 
                     radius * 2, radius * 2);
        
        // Draw main button body (white)
        //
        g.setColour(juce::Colours::white);
        g.fillEllipse(centerX - radius + pressOffset, centerY - radius + pressOffset, 
                     radius * 2, radius * 2);
        
        // Draw button border
        //
        g.setColour(juce::Colours::black);
        g.drawEllipse(centerX - radius + pressOffset, centerY - radius + pressOffset, 
                     radius * 2, radius * 2, 2.0f);
        
        // Draw small LED in top left corner
        //
        SmartGrid::Color currentColor = m_padUI.GetColor();
        if (currentColor != SmartGrid::Color::Off)
        {
            juce::Colour ledColor = juce::Colour(
                currentColor.m_red,
                currentColor.m_green, 
                currentColor.m_blue
            );
            
            // LED position in top left corner
            //
            float ledRadius = radius * 0.15f;
            float ledX = centerX - radius * 0.6f + pressOffset;
            float ledY = centerY - radius * 0.6f + pressOffset;
            
            // Draw LED with glow effect
            //
            g.setColour(ledColor.withAlpha(0.8f));
            g.fillEllipse(ledX - ledRadius * 1.5f, ledY - ledRadius * 1.5f, 
                         ledRadius * 3, ledRadius * 3);
            
            g.setColour(ledColor);
            g.fillEllipse(ledX - ledRadius, ledY - ledRadius, 
                         ledRadius * 2, ledRadius * 2);
            
            // LED border
            //
            g.setColour(juce::Colours::black);
            g.drawEllipse(ledX - ledRadius, ledY - ledRadius, 
                         ledRadius * 2, ledRadius * 2, 0.5f);
        }
        
        // Draw button text/icon area (optional)
        //
        if (m_isPressed)
        {
            // Draw a subtle pressed indicator
            //
            g.setColour(juce::Colours::black.withAlpha(0.2f));
            g.fillEllipse(centerX - radius * 0.3f + pressOffset, centerY - radius * 0.3f + pressOffset, 
                         radius * 0.6f, radius * 0.6f);
        }
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        m_padUI.OnPress(event.eventTime.getMilliseconds() * 1000);
        m_isPressed = true;
    }
    
    void mouseUp(const juce::MouseEvent& event) override
    {
        m_padUI.OnRelease(event.eventTime.getMilliseconds() * 1000);
        m_isPressed = false;
    }
    
    void mouseEnter(const juce::MouseEvent& event) override
    {

    }
    
    void mouseExit(const juce::MouseEvent& event) override
    {

    }
};