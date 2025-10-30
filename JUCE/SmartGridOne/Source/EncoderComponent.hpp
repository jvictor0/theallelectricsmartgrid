#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>

struct EncoderComponent : public juce::Component
{
    static constexpr int x_defaultPadSize = 100;

    EncoderBankUI m_ui;
    int m_x;
    int m_y;
    juce::Point<float> m_lastMousePosition;

    EncoderComponent(EncoderBankUI uiState, int x, int y)
        : m_ui(uiState)
        , m_x(x)
        , m_y(y)
    {
        setSize(x_defaultPadSize, x_defaultPadSize);
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

        if (m_ui.m_uiState->GetConnected(m_x, m_y))
        {
            for (size_t i = 0; i < m_ui.m_uiState->GetNumVoices(); ++i)
            {
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * (0.45f - i * 0.05f);

                // Draw the main 270-degree arc from 7:30 to 4:30
                //
                g.setColour(juce::Colours::white);
                juce::Path arcPath;
                arcPath.addCentredArc(centerX, centerY, radius, radius, 0.0f, 
                                    juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
                g.strokePath(arcPath, juce::PathStrokeType(2.0f));
            }

            for (size_t i = 0; i < m_ui.m_uiState->GetNumVoices(); ++i)
            {
                auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * (0.45f - i * 0.05f);

                // Calculate the indicator position
                //
                size_t voice = m_ui.m_uiState->GetNumVoices() * m_ui.m_uiState->GetCurrentTrack() + i;
                float value = m_ui.m_uiState->GetValue(m_x, m_y, voice);
                float angle = value * 270.0f; // Convert value to degrees (0-270)
                float indicatorAngle = juce::MathConstants<float>::pi * 0.75f + (angle * juce::MathConstants<float>::pi / 180.0f); // Start at 7:30 and go clockwise

                // Draw the small indicator circle
                //
                auto indicatorRadius = radius * 0.15f;
                auto indicatorX = centerX + radius * std::cos(indicatorAngle);
                auto indicatorY = centerY + radius * std::sin(indicatorAngle);

                auto indicatorColor = m_ui.m_uiState->GetIndicatorColor(voice);
                g.setColour(juce::Colour(indicatorColor.m_red, indicatorColor.m_green, indicatorColor.m_blue));
                g.fillEllipse(indicatorX - indicatorRadius, indicatorY - indicatorRadius, 
                            indicatorRadius * 2, indicatorRadius * 2);
            }
        }

        // Draw the rounded rectangle in the bottom gap with a black border
        //
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        auto rectSize = radius * 0.3f;
        auto squareX = centerX - 3 * rectSize / 2;
        auto squareY = centerY + radius * 0.6f;

        auto squareColor = m_ui.m_uiState->GetBrightnessAdjustedColor(m_x, m_y);
        juce::Rectangle<float> rect(squareX, squareY, 3 * rectSize, rectSize);
        float cornerSize = rectSize * 0.7f;

        g.setColour(juce::Colour(squareColor.m_red, squareColor.m_green, squareColor.m_blue));
        g.fillRoundedRectangle(rect, cornerSize);

        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(rect, cornerSize, 1.5f);
    }

    virtual void mouseDown(const juce::MouseEvent& event) override
    {
        m_lastMousePosition = event.position;
    }

    virtual void mouseDrag(const juce::MouseEvent& event) override
    {
        size_t timestamp = event.eventTime.getMilliseconds() * 1000;
        float sensitivity = 0.2;
        
        juce::Point<float> currentPosition = event.position;
        float deltaX = currentPosition.x - m_lastMousePosition.x;
        float deltaY = currentPosition.y - m_lastMousePosition.y;
        
        int change = static_cast<int>((deltaX - deltaY) * sensitivity);
        if (change != 0)
        {
            m_ui.m_messageBus->Push(SmartGrid::MessageIn(timestamp, m_ui.m_routeId, SmartGrid::MessageIn::Mode::EncoderIncDec, m_x, m_y, change));
            m_lastMousePosition = currentPosition;
        }
    }

    virtual void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        size_t timestamp = event.eventTime.getMilliseconds() * 1000;
        m_ui.m_messageBus->Push(SmartGrid::MessageIn(timestamp, m_ui.m_routeId, SmartGrid::MessageIn::Mode::EncoderPush, m_x, m_y, 1));
    }
};
