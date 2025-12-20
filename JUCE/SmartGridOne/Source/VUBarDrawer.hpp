#pragma once

#include <JuceHeader.h>

struct VUBarDrawer
{
    float m_height;
    float m_width;
    float m_xMin;
    float m_yMin;

    VUBarDrawer()
        : m_height(0)
        , m_width(0)
        , m_xMin(0)
        , m_yMin(0)
    {
    }

    VUBarDrawer(float height, float width, float xMin, float yMin)
        : m_height(height)
        , m_width(width)
        , m_xMin(xMin)
        , m_yMin(yMin)
    {
    }

    void DrawBar(juce::Graphics& g, juce::Colour colour, float x0Normalized, float x1Normalized, float valueNormalized)
    {
        float yNorm = 1.0f - valueNormalized;

        float screenX0 = m_xMin + x0Normalized * m_width;
        float screenX1 = m_xMin + x1Normalized * m_width;
        float screenY0 = m_yMin + yNorm * m_height;
        float screenY1 = m_yMin + m_height;

        g.setColour(colour);
        g.fillRect(screenX0, screenY0, screenX1 - screenX0, screenY1 - screenY0);
    }

    void DrawReduction(juce::Graphics& g, juce::Colour colour, float x0Normalized, float x1Normalized, float valueNormalized)
    {
        float yNorm = 1.0f - valueNormalized;

        float screenX0 = m_xMin + x0Normalized * m_width;
        float screenX1 = m_xMin + x1Normalized * m_width;
        float screenY0 = m_yMin;
        float screenY1 = m_yMin + yNorm * m_height;

        g.setColour(colour);
        g.fillRect(screenX0, screenY0, screenX1 - screenX0, screenY1 - screenY0);
    }
};

