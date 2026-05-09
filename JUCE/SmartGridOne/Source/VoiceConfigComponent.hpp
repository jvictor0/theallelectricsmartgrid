#pragma once

#include <JuceHeader.h>
#include "NonagonWrapper.hpp"
#include "VoiceMachineEnums.hpp"

struct VoiceConfigComponent : public juce::Component
{
    struct DirectoryExplorerComponent : public juce::Component
    {
        NonagonWrapper* m_nonagon;

        DirectoryExplorerComponent(NonagonWrapper* nonagon)
            : m_nonagon(nonagon)
        {
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            g.fillAll(juce::Colours::black);

            if (!m_nonagon)
            {
                return;
            }

            auto* squiggleUi = m_nonagon->GetSquiggleBoyUIState();

            if (!squiggleUi)
            {
                return;
            }

            const auto& dirUi = squiggleUi->m_directoryExplorerUIState;
            size_t which = dirUi.Which();

            if (!dirUi.GetUiExplorerOpen(which))
            {
                return;
            }

            static constexpr int x_numFileRows = 7;
            int totalRows = 1 + x_numFileRows;
            int rowHeight = bounds.getHeight() / totalRows;
            float fontSize = static_cast<float>(rowHeight) * 0.45f;
            fontSize = juce::jmin(22.0f, fontSize);

            g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));

            juce::Rectangle<int> headerBounds(0, 0, bounds.getWidth(), rowHeight);
            g.setColour(juce::Colours::lightgrey);
            g.drawFittedText(
                juce::String(dirUi.GetRelativePath(which)),
                headerBounds,
                juce::Justification::centredLeft,
                1);

            size_t selectedRow = dirUi.GetSelectedListRow(which);

            for (int i = 0; i < x_numFileRows; ++i)
            {
                int y = rowHeight * (1 + i);
                juce::Rectangle<int> rowBounds(0, y, bounds.getWidth(), rowHeight);

                if (static_cast<size_t>(i) == selectedRow)
                {
                    g.setColour(juce::Colours::darkgrey);
                    g.fillRect(rowBounds);

                    g.setColour(juce::Colours::yellow);
                }
                else
                {
                    g.setColour(juce::Colours::white);
                }

                g.drawFittedText(
                    juce::String(dirUi.GetListLine(which, static_cast<size_t>(i))),
                    rowBounds.reduced(4, 0),
                    juce::Justification::centredLeft,
                    1);
            }
        }
    };

    NonagonWrapper* m_nonagon;

    DirectoryExplorerComponent m_directoryExplorerComponent;

    VoiceConfigComponent(NonagonWrapper* nonagon)
        : m_nonagon(nonagon)
        , m_directoryExplorerComponent(nonagon)
    {
        addAndMakeVisible(m_directoryExplorerComponent);
    }

    void resized() override
    {
        m_directoryExplorerComponent.setBounds(getLocalBounds());
    }

    void paint(juce::Graphics& g) override
    {
        auto* squiggleUi = m_nonagon ? m_nonagon->GetSquiggleBoyUIState() : nullptr;
        bool explorerActive = false;

        if (squiggleUi)
        {
            const auto& dirUi = squiggleUi->m_directoryExplorerUIState;
            size_t which = dirUi.Which();
            explorerActive = dirUi.GetUiExplorerOpen(which);
        }

        m_directoryExplorerComponent.setVisible(explorerActive);

        if (explorerActive)
        {
            return;
        }

        auto bounds = getLocalBounds();
        g.fillAll(juce::Colours::black);

        auto* uiState = squiggleUi;

        if (!uiState)
        {
            return;
        }

        g.setColour(juce::Colours::white);
        g.drawRect(bounds);

        size_t trackIx = uiState->m_activeTrack.load();

        if (TheNonagonInternal::x_numTrios <= trackIx)
        {
            trackIx = 0;
        }

        size_t voiceIndex = trackIx * TheNonagonInternal::x_voicesPerTrio;
        auto sourceMachine = uiState->m_voiceSourceUIState[voiceIndex].m_sourceMachine.load();
        auto filterMachine = uiState->m_voiceFilterUIState[voiceIndex].m_filterMachine.load();

        juce::String sourceText = VoiceMachine::ToString(sourceMachine);
        juce::String filterText = VoiceMachine::ToString(filterMachine);

        auto remaining = bounds;
        auto topHalf = remaining.removeFromTop(bounds.getHeight() / 2);

        const int topRowH = topHalf.getHeight() / 3;
        auto headerBounds = topHalf.removeFromTop(topRowH);
        auto sourceBounds = topHalf.removeFromTop(topRowH);
        auto filterBounds = topHalf;

        const int cellW = remaining.getWidth() / static_cast<int>(TheNonagonInternal::x_voicesPerTrio);
        const int cellH = remaining.getHeight();

        g.setColour(juce::Colours::grey);
        g.drawLine(
            0.0f,
            static_cast<float>(headerBounds.getBottom()),
            static_cast<float>(bounds.getWidth()),
            static_cast<float>(headerBounds.getBottom()));
        g.drawLine(
            0.0f,
            static_cast<float>(sourceBounds.getBottom()),
            static_cast<float>(bounds.getWidth()),
            static_cast<float>(sourceBounds.getBottom()));
        g.drawLine(
            0.0f,
            static_cast<float>(remaining.getY()),
            static_cast<float>(bounds.getWidth()),
            static_cast<float>(remaining.getY()));

        for (int i = 1; i < 3; ++i)
        {
            float x = static_cast<float>(remaining.getX() + i * cellW);
            g.drawLine(x, static_cast<float>(remaining.getY()), x, static_cast<float>(remaining.getBottom()));
        }

        float mainFontSize = std::min(28.0f, static_cast<float>(topRowH) * 0.42f);

        g.setColour(GetTrioJuceColor(trackIx));
        g.setFont(juce::Font(juce::FontOptions(mainFontSize, juce::Font::bold)));
        g.drawFittedText(GetTrioName(trackIx), headerBounds, juce::Justification::centred, 1);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(mainFontSize, juce::Font::bold)));
        g.drawFittedText(sourceText, sourceBounds.reduced(6, 0), juce::Justification::centred, 1);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(mainFontSize, juce::Font::bold)));
        g.drawFittedText(filterText, filterBounds.reduced(6, 0), juce::Justification::centred, 1);

        float pathFontSize = std::min(16.0f, static_cast<float>(cellH) * 0.28f);

        g.setFont(juce::Font(juce::FontOptions(pathFontSize, juce::Font::plain)));
        g.setColour(juce::Colours::white);

        for (size_t voiceCol = 0; voiceCol < TheNonagonInternal::x_voicesPerTrio; ++voiceCol)
        {
            const int x0 = remaining.getX() + static_cast<int>(voiceCol) * cellW;
            juce::Rectangle<int> pathCell(x0, remaining.getY(), cellW, cellH);

            const size_t voiceIndex = trackIx * TheNonagonInternal::x_voicesPerTrio + voiceCol;
            const auto& sampleDirUi = uiState->m_voiceSourceUIState[voiceIndex].m_sampleDirectoryUIState;
            const size_t whichPaths = sampleDirUi.Which();
            juce::String path = juce::String(sampleDirUi.GetPath(whichPaths));

            g.drawFittedText(path, pathCell.reduced(4, 2), juce::Justification::centred, 4);
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
