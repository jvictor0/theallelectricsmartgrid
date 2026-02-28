#pragma once

#include <JuceHeader.h>
#include "NonagonWrapper.hpp"
#include "VoiceMachineEnums.hpp"

struct VoiceConfigComponent : public juce::Component
{
    NonagonWrapper* m_nonagon;

    VoiceConfigComponent(NonagonWrapper* nonagon)
        : m_nonagon(nonagon)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.fillAll(juce::Colours::black);

        int columns = 3;
        int rows = 3;
        int columnWidth = bounds.getWidth() / columns;
        int rowHeight = bounds.getHeight() / rows;

        g.setColour(juce::Colours::white);
        g.drawRect(bounds);

        // Draw vertical grid lines
        //
        for (int i = 1; i < columns; ++i)
        {
            float x = static_cast<float>(i * columnWidth);
            g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()));
        }

        // Draw horizontal grid lines
        //
        for (int i = 1; i < rows; ++i)
        {
            float y = static_cast<float>(i * rowHeight);
            g.drawLine(0.0f, y, static_cast<float>(bounds.getWidth()), y);
        }

        auto* uiState = m_nonagon ? m_nonagon->GetSquiggleBoyUIState() : nullptr;
        if (!uiState)
        {
            return;
        }

        // Set a reasonable font size
        float fontSize = std::min(24.0f, static_cast<float>(rowHeight) * 0.4f);
        g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));

        for (int col = 0; col < columns; ++col)
        {
            size_t trioIndex = static_cast<size_t>(col);
            size_t voiceIndex = trioIndex * TheNonagonInternal::x_voicesPerTrio;
            auto sourceMachine = uiState->m_voiceSourceUIState[voiceIndex].m_sourceMachine.load();
            auto filterMachine = uiState->m_voiceFilterUIState[voiceIndex].m_filterMachine.load();

            juce::String sourceText = VoiceMachine::ToString(sourceMachine);
            juce::String filterText = VoiceMachine::ToString(filterMachine);

            // Calculate cell bounds using explicit coordinates
            //
            int cellX = col * columnWidth;

            auto headerBounds = juce::Rectangle<int>(cellX, 0, columnWidth, rowHeight);
            auto sourceBounds = juce::Rectangle<int>(cellX, rowHeight, columnWidth, rowHeight);
            auto filterBounds = juce::Rectangle<int>(cellX, rowHeight * 2, columnWidth, rowHeight);

            // Draw header (trio name)
            //
            g.setColour(GetTrioJuceColor(trioIndex));
            g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));
            g.drawFittedText(GetTrioName(trioIndex), headerBounds, juce::Justification::centred, 1);

            // Draw source machine
            //
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));
            g.drawFittedText(sourceText, sourceBounds, juce::Justification::centred, 1);

            // Draw filter machine
            //
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));
            g.drawFittedText(filterText, filterBounds, juce::Justification::centred, 1);
        }
    }

    juce::Colour GetTrioJuceColor(size_t trioIndex)
    {
        TheNonagonSmartGrid::Trio trio = static_cast<TheNonagonSmartGrid::Trio>(trioIndex);
        return J(TheNonagonSmartGrid::TrioColor(trio));
    }

    const char* GetTrioName(size_t trioIndex)
    {
        switch (static_cast<TheNonagonSmartGrid::Trio>(trioIndex))
        {
            case TheNonagonSmartGrid::Trio::Water:
                return "Water";
            case TheNonagonSmartGrid::Trio::Earth:
                return "Earth";
            case TheNonagonSmartGrid::Trio::Fire:
                return "Fire";
            case TheNonagonSmartGrid::Trio::NumTrios:
                break;
        }

        return "Unknown";
    }
};
