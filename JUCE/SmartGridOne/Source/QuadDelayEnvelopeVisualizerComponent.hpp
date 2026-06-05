#pragma once

#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "QuadDelay.hpp"
#include <JuceHeader.h>
#include <algorithm>

struct QuadDelayEnvelopeVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    static constexpr float x_rangeSlewTimeSeconds = 10.0f;
    static constexpr float x_frameRate = 60.0f;
    static constexpr float x_rangeSlewAlpha = 0.00167f;

    TheNonagonSquiggleBoyInternal::UIState* m_uiState;
    float m_slewedMin{0.0f};
    float m_slewedMax{0.0f};

    QuadDelayEnvelopeVisualizerComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
    {
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        auto bounds = boundsRect.toFloat();
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        float sliceHeight = height / 4.0f;

        auto& delayUIState = m_uiState->m_squiggleBoyUIState.m_delayUIState;
        const QuadDelay::UISnapshot& snapshot = delayUIState.GetCurrentSnapshot();

        float targetMin = snapshot.m_delayEnvelopeState[0].m_minEnvelope[0];
        float targetMax = snapshot.m_delayEnvelopeState[0].m_maxEnvelope[0];
        for (int delayLineIdx = 0; delayLineIdx < 4; ++delayLineIdx)
        {
            auto& envelopeState = snapshot.m_delayEnvelopeState[delayLineIdx];
            for (size_t i = 0; i < QuadDelay::x_positionalBufferSize; ++i)
            {
                targetMin = std::min(targetMin, envelopeState.m_minEnvelope[i]);
                targetMax = std::max(targetMax, envelopeState.m_maxEnvelope[i]);
            }
        }

        float targetRange = targetMax - targetMin;
        float currentRange = m_slewedMax - m_slewedMin;
        if (targetRange > currentRange)
        {
            m_slewedMin = targetMin;
            m_slewedMax = targetMax;
        }
        else
        {
            m_slewedMin = m_slewedMin + x_rangeSlewAlpha * (targetMin - m_slewedMin);
            m_slewedMax = m_slewedMax + x_rangeSlewAlpha * (targetMax - m_slewedMax);
        }

        float range = m_slewedMax - m_slewedMin;
        if (range < 1e-6f)
        {
            range = 1.0f;
        }

        float segmentWidth = width / static_cast<float>(QuadDelay::x_positionalBufferSize);
        for (int delayLineIdx = 0; delayLineIdx < 4; ++delayLineIdx)
        {
            float sliceY = bounds.getY() + static_cast<float>(delayLineIdx) * sliceHeight;
            auto& envelopeState = snapshot.m_delayEnvelopeState[delayLineIdx];

            for (size_t bufIdx = 0; bufIdx < QuadDelay::x_positionalBufferSize; ++bufIdx)
            {
                float x = bounds.getX() + static_cast<float>(bufIdx) * segmentWidth;
                float minEnv = envelopeState.m_minEnvelope[bufIdx];
                float maxEnv = envelopeState.m_maxEnvelope[bufIdx];

                float yMin = sliceY + sliceHeight * (1.0f - (minEnv - m_slewedMin) / range);
                float yMax = sliceY + sliceHeight * (1.0f - (maxEnv - m_slewedMin) / range);
                if (yMin > yMax)
                {
                    std::swap(yMin, yMax);
                }

                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.fillRect(x, yMin, segmentWidth + 0.5f, yMax - yMin);
            }

            float relativeRead = envelopeState.m_relativeReadHeadPosition;
            if (relativeRead >= 0.0f && relativeRead <= 1.0f)
            {
                float lineX = bounds.getX() + relativeRead * width;
                g.setColour(juce::Colours::cyan);
                g.drawLine(lineX, sliceY, lineX, sliceY + sliceHeight, 1.0f);
            }

            float relativeWrite = envelopeState.m_relativeWriteHeadPosition;
            if (relativeWrite >= 0.0f && relativeWrite <= 1.0f)
            {
                float lineX = bounds.getX() + relativeWrite * width;
                g.setColour(juce::Colours::orange);
                g.drawLine(lineX, sliceY, lineX, sliceY + sliceHeight, 1.0f);
            }
        }
    }
};
