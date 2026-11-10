#pragma once

#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "QuadDelay.hpp"
#include "PathDrawer.hpp"
#include <JuceHeader.h>
#include <algorithm>

struct QuadDelayEnvelopeVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr size_t x_numPoints = 1024;
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    explicit QuadDelayEnvelopeVisualizerComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : m_uiState(uiState)
    {
    }

    static double HeadPosition(const ScopeReader& reader, double warpedPosition)
    {
        double length = reader.m_transferIndex - reader.m_startIndex;
        if (reader.m_transferXSample < reader.m_numXSamples)
        {
            length += reader.m_postTransferEndIndex - reader.m_postTransferIndex;
        }

        if (length <= 0.0 || !std::isfinite(warpedPosition))
        {
            return std::numeric_limits<double>::quiet_NaN();
        }

        double phase = (warpedPosition - reader.m_startIndex) / length;
        return phase - std::floor(phase);
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);
        auto& state = m_uiState->m_squiggleBoyUIState.m_delayUIState;
        auto bounds = boundsRect.toFloat();
        float laneHeight = bounds.getHeight() / 4.0f;
        float pointWidth = bounds.getWidth() / x_numPoints;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            ScopeReader reader(&state.m_scopeWriter, channel, 0, x_numPoints, 1);
            if (reader.m_empty)
            {
                continue;
            }

            float top = bounds.getY() + channel * laneHeight;
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            for (size_t point = 0; point < x_numPoints; ++point)
            {
                float magnitude = reader.Get(static_cast<double>(point));
                float level = std::clamp(PathDrawer::AmpToDbNormalized(magnitude), 0.0f, 1.0f);
                float height = laneHeight * level;
                float center = top + laneHeight * 0.5f;
                g.fillRect(bounds.getX() + point * pointWidth, center - height * 0.5f, pointWidth + 0.5f, height);
            }

            double read = HeadPosition(reader, state.m_readHeadPhase[channel].load());
            if (std::isfinite(read))
            {
                float x = bounds.getX() + static_cast<float>(read) * bounds.getWidth();
                g.setColour(juce::Colours::cyan);
                g.drawLine(x, top, x, top + laneHeight, 1.0f);
            }

            float write = static_cast<float>(reader.m_transferXSample / reader.m_numXSamples);
            float x = bounds.getX() + write * bounds.getWidth();
            g.setColour(juce::Colours::orange);
            g.drawLine(x, top, x, top + laneHeight, 1.0f);
        }
    }
};
