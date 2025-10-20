#pragma once

#include "SmartGridInclude.hpp"
#include <JuceHeader.h>

struct MeterDrawer
{
    float m_height;
    float m_width;
    float m_xMin;
    float m_yMin;

    void DrawRadialVUMeter(juce::Graphics& g, float rmsdBFS, float peakdBFS, juce::Colour needleColor)
    {
        // Calculate the center and radius for the arc
        //
        float centerX = m_xMin + m_width / 2.0f;
        float centerY = m_yMin + m_height * 0.9f;
        float radius = std::min(m_width / std::sqrt(2.0f), m_height * 0.8f) * 0.9f;
        
        // Define the arc bounds (top 90 degrees, rotated 45 degrees clockwise)
        // Start at -45 degrees, end at +45 degrees (90 degree span)
        //
        float startAngle = juce::MathConstants<float>::pi * 0.75; 
        float endAngle = juce::MathConstants<float>::pi * 0.25f; 
        float totalAngle = endAngle - startAngle;
        
        // Draw arc segments with different colors using Path
        //
        juce::Path arcPath;
        
        // White segment: -30dB to -12dB (18dB range)
        //
        float whiteStartAngle = startAngle + juce::MathConstants<float>::pi;
        float whiteEndAngle = whiteStartAngle - (18.0f / 36.0f) * totalAngle;
        arcPath.clear();
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0, whiteEndAngle, whiteStartAngle, true);
        g.setColour(juce::Colours::white);
        g.strokePath(arcPath, juce::PathStrokeType(2.0f));
        
        // Yellow segment: -12dB to 0dB (12dB range)
        //
        float yellowStartAngle = whiteEndAngle;
        float yellowEndAngle = yellowStartAngle - (12.0f / 36.0f) * totalAngle;
        arcPath.clear();
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0, yellowEndAngle, yellowStartAngle, true);
        g.setColour(juce::Colours::yellow);
        g.strokePath(arcPath, juce::PathStrokeType(2.0f));
        
        // Red segment: 0dB to +6dB (6dB range)
        //
        float redStartAngle = yellowEndAngle;
        float redEndAngle = redStartAngle - (6.0f / 36.0f) * totalAngle;
        arcPath.clear();
        arcPath.addCentredArc(centerX, centerY, radius, radius, 0, redEndAngle, redStartAngle, true);
        g.setColour(juce::Colours::red);
        g.strokePath(arcPath, juce::PathStrokeType(2.0f));
        
        // Draw tick marks every 6dB
        //
        g.setColour(juce::Colours::white);
        for (int db = -30; db <= 6; db += 6)
        {
            if (-12 <= db && db < 0)
            {
                g.setColour(juce::Colours::yellow);
            }
            else if (0 <= db)
            {
                g.setColour(juce::Colours::red);
            }
            else
            {
                g.setColour(juce::Colours::white);
            }

            float normalizedDb = (db + 30.0f) / 36.0f; // Normalize to 0-1 range
            float angle = startAngle + normalizedDb * totalAngle;
            
            // Calculate tick mark positions
            //
            float innerRadius = radius * 0.8f;
            float outerRadius = radius * 0.9f;
            
            float x1 = centerX + std::cos(angle) * innerRadius;
            float y1 = centerY - std::sin(angle) * innerRadius;
            float x2 = centerX + std::cos(angle) * outerRadius;
            float y2 = centerY - std::sin(angle) * outerRadius;
            
            g.drawLine(x1, y1, x2, y2, 1.0f);
        }
        
        // Draw the needle
        //
        float normalizedDb = (rmsdBFS + 30.0f) / 36.0f; // Normalize to 0-1 range
        normalizedDb = juce::jlimit(0.0f, 1.0f, normalizedDb); // Clamp to valid range
        float needleAngle = startAngle + normalizedDb * totalAngle;
        
        float needleLength = radius * 0.7f;
        float needleX = centerX + std::cos(needleAngle) * needleLength;
        float needleY = centerY - std::sin(needleAngle) * needleLength;
        
        g.setColour(needleColor);
        g.drawLine(centerX, centerY, needleX, needleY, 2.0f);

        float normalizedPeakDb = (peakdBFS + 30.0f) / 36.0f;
        normalizedPeakDb = juce::jlimit(0.0f, 1.0f, normalizedPeakDb);
        float peakNeedleAngle = startAngle + normalizedPeakDb * totalAngle;
      
        float peakNeedleTip = radius * 0.9f;
        float peakNeedleTipX = centerX + std::cos(peakNeedleAngle) * peakNeedleTip;
        float peakNeedleTipY = centerY - std::sin(peakNeedleAngle) * peakNeedleTip;

        float peakNeedleTail = radius * 0.8f;
        float peakNeedleTailX = centerX + std::cos(peakNeedleAngle) * peakNeedleTail;
        float peakNeedleTailY = centerY - std::sin(peakNeedleAngle) * peakNeedleTail;

        g.setColour(needleColor);
        g.drawLine(peakNeedleTailX, peakNeedleTailY, peakNeedleTipX, peakNeedleTipY, 2.0f);
    }
};

struct VoiceMeterComponent : public juce::Component
{
    SquiggleBoyWithEncoderBank::UIState* m_uiState;
    MeterDrawer m_meterDrawer[TheNonagonInternal::x_numVoices];

    VoiceMeterComponent(SquiggleBoyWithEncoderBank::UIState* uiState)
        : m_uiState(uiState)
    {
        setSize(400, 200);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        for (size_t i = 0; i < TheNonagonInternal::x_numVoices; ++i)
        {
            float rmsdBFS = m_uiState->m_voiceMeterReader[i].GetRMSDbFS();
            float peakdBFS = m_uiState->m_voiceMeterReader[i].GetPeakDbFS();
            SmartGrid::Color color = TheNonagonSmartGrid::VoiceColor(i);
            m_meterDrawer[i].m_height = getHeight() / TheNonagonInternal::x_voicesPerTrio;
            m_meterDrawer[i].m_width = getWidth() / TheNonagonInternal::x_voicesPerTrio;
            m_meterDrawer[i].m_xMin = (i % TheNonagonInternal::x_voicesPerTrio) * m_meterDrawer[i].m_width;
            m_meterDrawer[i].m_yMin = (i / TheNonagonInternal::x_voicesPerTrio) * m_meterDrawer[i].m_height;
            m_meterDrawer[i].DrawRadialVUMeter(g, rmsdBFS, peakdBFS, juce::Colour(color.m_red, color.m_green, color.m_blue));
        }
    }
};