#pragma once

#include "SmartGridInclude.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "AudioBuffer.hpp"
#include "VoiceMachineEnums.hpp"
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

struct SampleTrioWaveformVisualizerComponent : public SmartGridOneMainVisualizerComponent
{
    TheNonagonSquiggleBoyInternal::UIState* m_uiState;

    SampleTrioWaveformVisualizerComponent(TheNonagonSquiggleBoyInternal::UIState* uiState)
        : SmartGridOneMainVisualizerComponent()
        , m_uiState(uiState)
    {
    }

    static bool StripUsesVoiceColour(float t, float start, float length)
    {
        float sum = start + length;
        if (1.0f < sum)
        {
            return (t <= length + start - 1) || (start <= t);
        }

        if (sum < 1.0f)
        {
            float end = start + length;
            return start <= t && t <= end;
        }

        float end = start + length;
        return start <= t && t <= end;
    }

    void Draw(juce::Graphics& g, juce::Rectangle<int> boundsRect) override
    {
        g.fillAll(juce::Colours::black);

        if (!m_uiState)
        {
            return;
        }

        auto bounds = boundsRect.toFloat();
        float width = bounds.getWidth();
        float height = bounds.getHeight();
        float sliceHeight = height / static_cast<float>(TheNonagonInternal::x_voicesPerTrio);

        auto& squiggleUi = m_uiState->m_squiggleBoyUIState;

        size_t trackIx = squiggleUi.m_activeTrack.load(std::memory_order_acquire);
        if (TheNonagonInternal::x_numTrios <= trackIx)
        {
            trackIx = 0;
        }

        TheNonagonSmartGrid::Trio trio = static_cast<TheNonagonSmartGrid::Trio>(trackIx);

        constexpr size_t x_numSections = AudioBuffer::x_numSections;
        float segmentWidth = width / static_cast<float>(x_numSections);

        for (size_t voiceCol = 0; voiceCol < TheNonagonInternal::x_voicesPerTrio; ++voiceCol)
        {
            size_t voiceIx = trackIx * TheNonagonInternal::x_voicesPerTrio + voiceCol;
            float sliceY = bounds.getY() + static_cast<float>(voiceCol) * sliceHeight;

            juce::Colour voiceColour =
                J(TheNonagonSmartGrid::VoiceColor(trio, voiceCol)).withAlpha(0.85f);

            auto& bankUi = squiggleUi.m_voiceSourceUIState[voiceIx].m_audioBufferBankUIState;
            size_t which = bankUi.Which();

            auto machine = squiggleUi.m_voiceSourceUIState[voiceIx].m_sourceMachine.load(std::memory_order_acquire);

            float start = squiggleUi.m_voiceSourceUIState[voiceIx].m_sampleSourceUIState.m_start.load(
                std::memory_order_acquire);
            float length = squiggleUi.m_voiceSourceUIState[voiceIx].m_sampleSourceUIState.m_length.load(
                std::memory_order_acquire);

            if (machine != VoiceMachine::SourceMachine::Sample)
            {
                start = 0.0f;
                length = 1.0f;
            }

            for (size_t bufIdx = 0; bufIdx < x_numSections; ++bufIdx)
            {
                float x = bounds.getX() + static_cast<float>(bufIdx) * segmentWidth;
                float minS = bankUi.m_sectionMinimums[which][bufIdx];
                float maxS = bankUi.m_sectionMaximums[which][bufIdx];

                float minClamped = std::clamp(minS, -1.0f, 1.0f);
                float maxClamped = std::clamp(maxS, -1.0f, 1.0f);

                float yTop = sliceY + sliceHeight * (1.0f - (maxClamped * 0.5f + 0.5f));
                float yBot = sliceY + sliceHeight * (1.0f - (minClamped * 0.5f + 0.5f));
                if (yTop > yBot)
                {
                    std::swap(yTop, yBot);
                }

                float tMid =
                    (static_cast<float>(bufIdx) + 0.5f) / static_cast<float>(x_numSections);
                bool useVoice = StripUsesVoiceColour(tMid, start, length);
                g.setColour(useVoice ? voiceColour : juce::Colours::white);
                g.fillRect(x, yTop, segmentWidth + 0.5f, yBot - yTop);
            }

            if (machine == VoiceMachine::SourceMachine::Sample)
            {
                double pos =
                    squiggleUi.m_voiceSourceUIState[voiceIx].m_sampleSourceUIState.m_samplePosition.load(
                        std::memory_order_acquire);
                float px = static_cast<float>(pos);
                px = px - std::floor(px);
                float lineX = bounds.getX() + px * width;
                g.setColour(juce::Colours::white);
                g.drawLine(lineX, sliceY, lineX, sliceY + sliceHeight, 1.5f);
            }
        }
    }
};
