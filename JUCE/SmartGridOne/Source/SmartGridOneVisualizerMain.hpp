#pragma once

#include <array>
#include "SmartGridInclude.hpp"
#include "NonagonWrapper.hpp"
#include "SmartGridOneMainVisualizerComponent.hpp"
#include "ScopeComponent.hpp"
#include "MeterComponent.hpp"
#include "MasteringComponents.hpp"

struct SmartGridOneVisualizerMain : public juce::Component
{
    static constexpr int x_gridWidth = 24;
    static constexpr int x_gridHeight = 18;
    static constexpr double x_aspectRatio = static_cast<double>(x_gridWidth) / static_cast<double>(x_gridHeight);
    static constexpr int x_blockSize = 8;
    static constexpr int x_meterGridX = 8;
    static constexpr int x_meterGridY = 5;
    static constexpr int x_meterGridWidth = 16;
    static constexpr int x_meterGridHeight = 2;
    static constexpr int x_fullWidthGridX = 0;
    static constexpr int x_fullWidthGridY = 8;
    static constexpr int x_fullWidthGridWidth = 24;
    static constexpr int x_fullWidthGridHeight = 8;
    static constexpr int x_modPanelSize = 2;
    static constexpr int x_modPanelY = 16;
    static constexpr int x_numModPanels = 12;
    
    static constexpr std::array<int, 2> x_encoderGridX =
    {
        8,
        16
    };
    
    static constexpr std::array<int, 2> x_encoderGridY =
    {
        0,
        3
    };
    
    static constexpr int x_encoderGridWidth = 8;
    static constexpr int x_encoderGridHeight = 2;
    
    static constexpr std::array<std::array<int, 2>, 4> x_blockPositions =
    {{
        { 0, 0 },
        { 0, 8 },
        { 8, 8 },
        { 16, 8 }
    }};

    static constexpr size_t x_numBlocks = x_blockPositions.size();
    static constexpr size_t x_voicesPerTrack = SquiggleBoy::x_voicesPerTrack;

    NonagonWrapper* m_nonagon;
    
    int m_cellSize;
    int m_gridX;
    int m_gridY;
    int m_gridWidth;
    int m_gridHeight;
    
    int m_scopeVoiceOffset;
    
    std::unique_ptr<MeteringComponent> m_meteringComponent;
    
#define F(name, bank, block, ctor, flags) \
    std::unique_ptr<SmartGridOneMainVisualizerComponent> m_##name;
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F

#define G(name, slot, ctor, mode, color) \
    std::unique_ptr<SmartGridOneMainVisualizerComponent> m_##name;
#include "ForEachModulationVisualizer.hpp"
#undef G
    
    SmartGridOneVisualizerMain(NonagonWrapper* nonagon)
        : m_nonagon(nonagon)
        , m_cellSize(0)
        , m_gridX(0)
        , m_gridY(0)
        , m_gridWidth(0)
        , m_gridHeight(0)
        , m_scopeVoiceOffset(-1)
    {
        setSize(800, 600);
        
        TheNonagonSquiggleBoyInternal::UIState* uiState = m_nonagon->GetUIState();
        
#define F(name, bank, block, ctor, flags) \
        m_##name = ctor; \
        if (m_##name) \
        { \
            m_##name->m_sourceMachineFlags = flags; \
        }
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F

#define G(name, slot, ctor, mode, color) \
        m_##name = ctor;
#include "ForEachModulationVisualizer.hpp"
#undef G
        
        m_meteringComponent = std::make_unique<MeteringComponent>(uiState);
        
        CalculateGridLayout();
    }
    
    void resized() override
    {
        CalculateGridLayout();
    }
    
    void paint(juce::Graphics& g) override
    {
        g.saveState();
        ExcludeEncoderClip(g);
        g.fillAll(juce::Colours::black);
        DispatchPaint(g, m_nonagon->GetSelectedEncoderBank());
        DispatchModulationPaint(g, m_nonagon->GetSelectedEncoderBank());
        DrawMetering(g);
        g.restoreState();
    }
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        DispatchClick(event);
    }
    
    void CalculateGridLayout()
    {
        auto bounds = getLocalBounds();
        int availableWidth = bounds.getWidth();
        int availableHeight = bounds.getHeight();
        
        double availableAspectRatio = static_cast<double>(availableWidth) / static_cast<double>(availableHeight);
        
        if (availableAspectRatio > x_aspectRatio)
        {
            m_cellSize = availableHeight / x_gridHeight;
            m_gridHeight = m_cellSize * x_gridHeight;
            m_gridWidth = m_cellSize * x_gridWidth;
            m_gridX = (availableWidth - m_gridWidth) / 2;
            m_gridY = 0;
        }
        else
        {
            m_cellSize = availableWidth / x_gridWidth;
            m_gridWidth = m_cellSize * x_gridWidth;
            m_gridHeight = m_cellSize * x_gridHeight;
            m_gridX = 0;
            m_gridY = (availableHeight - m_gridHeight) / 2;
        }
    }
    
    juce::Rectangle<int> GetBlockBounds(int blockIndex)
    {
        if (blockIndex < 0 || blockIndex >= static_cast<int>(x_blockPositions.size()))
        {
            return juce::Rectangle<int>();
        }
        
        int gridX = x_blockPositions[blockIndex][0];
        int gridY = x_blockPositions[blockIndex][1];
        
        int x = m_gridX + (gridX * m_cellSize);
        int y = m_gridY + (gridY * m_cellSize);
        int width = x_blockSize * m_cellSize;
        int height = x_blockSize * m_cellSize;
        
        return juce::Rectangle<int>(x, y, width, height);
    }
    
    juce::Rectangle<int> GetMeterBounds()
    {
        int x = m_gridX + (x_meterGridX * m_cellSize);
        int y = m_gridY + (x_meterGridY * m_cellSize);
        int width = x_meterGridWidth * m_cellSize;
        int height = x_meterGridHeight * m_cellSize;
        return juce::Rectangle<int>(x, y, width, height);
    }
    
    juce::Rectangle<int> GetFullWidthBounds()
    {
        int x = m_gridX + (x_fullWidthGridX * m_cellSize);
        int y = m_gridY + (x_fullWidthGridY * m_cellSize);
        int width = x_fullWidthGridWidth * m_cellSize;
        int height = x_fullWidthGridHeight * m_cellSize;
        return juce::Rectangle<int>(x, y, width, height);
    }

    juce::Rectangle<int> GetModPanelBounds(int slot)
    {
        int x = m_gridX + (slot * x_modPanelSize * m_cellSize);
        int y = m_gridY + (x_modPanelY * m_cellSize);
        int width = x_modPanelSize * m_cellSize;
        int height = x_modPanelSize * m_cellSize;
        return juce::Rectangle<int>(x, y, width, height);
    }
    
    void DrawMetering(juce::Graphics& g)
    {
        if (!m_meteringComponent)
        {
            return;
        }
        
        auto bounds = GetMeterBounds();
        g.saveState();
        g.reduceClipRegion(bounds);
        g.setOrigin(bounds.getX(), bounds.getY());
        m_meteringComponent->setBounds(0, 0, bounds.getWidth(), bounds.getHeight());
        m_meteringComponent->paint(g);
        g.restoreState();
    }
    
    // Check if a visualizer should be shown based on machine flags
    // Returns true if any visible voice uses a matching source machine
    //
    bool ShouldShowVisualizer(SmartGridOneMainVisualizerComponent* viz)
    {
        if (!viz)
        {
            return false;
        }

        auto* uiState = m_nonagon->GetUIState();
        size_t baseVoiceIx = uiState->m_squiggleBoyUIState.m_activeTrack.load() * x_voicesPerTrack;

        // Check if any voice in the active track matches the machine flags
        //
        for (size_t i = 0; i < x_voicesPerTrack; ++i)
        {
            size_t voiceIx = baseVoiceIx + i;
            auto machine = uiState->m_squiggleBoyUIState.m_voiceSourceUIState[voiceIx].m_sourceMachine.load();
            if (viz->ShouldShowForMachine(machine))
            {
                return true;
            }
        }

        return false;
    }

    void DispatchPaint(juce::Graphics& g, SmartGridOneEncoders::Bank bank)
    {
        // Draw visualizers for standard blocks (0-3)
        //
#define F(name, vizBank, block, ctor, flags) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block >= 0 && block < static_cast<int>(x_numBlocks)) \
        { \
            if (m_##name && ShouldShowVisualizer(m_##name.get())) \
            { \
                auto blockBounds = GetBlockBounds(block); \
                g.saveState(); \
                g.reduceClipRegion(blockBounds); \
                g.setOrigin(blockBounds.getX(), blockBounds.getY()); \
                m_##name->Draw(g, juce::Rectangle<int>(0, 0, blockBounds.getWidth(), blockBounds.getHeight())); \
                g.restoreState(); \
            } \
        }
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
        
        // Draw special full-width visualizers (block == -1)
        //
#define F(name, vizBank, block, ctor, flags) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block == -1) \
        { \
            if (m_##name && ShouldShowVisualizer(m_##name.get())) \
            { \
                auto fullBounds = GetFullWidthBounds(); \
                g.saveState(); \
                g.reduceClipRegion(fullBounds); \
                g.setOrigin(fullBounds.getX(), fullBounds.getY()); \
                m_##name->Draw(g, juce::Rectangle<int>(0, 0, fullBounds.getWidth(), fullBounds.getHeight())); \
                g.restoreState(); \
            } \
        }
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
    }
    
    void DispatchClick(const juce::MouseEvent& event)
    {
        auto position = event.getPosition();
        
        if (IsInEncoderRegion(position))
        {
            return;
        }
        
        SmartGridOneEncoders::Bank bank = m_nonagon->GetSelectedEncoderBank();
        
#define F(name, vizBank, block, ctor, flags) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block >= 0 && block < static_cast<int>(x_numBlocks)) \
        { \
            if (m_##name) \
            { \
                auto bounds = GetBlockBounds(block); \
                if (bounds.contains(position)) \
                { \
                    auto localEvent = event.withNewPosition(position - bounds.getPosition()); \
                    m_##name->OnClick(localEvent); \
                } \
            } \
        }
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
        
#define F(name, vizBank, block, ctor, flags) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block == -1) \
        { \
            if (m_##name) \
            { \
                auto bounds = GetFullWidthBounds(); \
                if (bounds.contains(position)) \
                { \
                    auto localEvent = event.withNewPosition(position - bounds.getPosition()); \
                    m_##name->OnClick(localEvent); \
                } \
            } \
        }
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F

        DispatchModulationClick(event, bank);
    }

    bool IsVoiceModeActive(SmartGridOneEncoders::Bank bank)
    {
        switch (bank)
        {
            case SmartGridOneEncoders::Bank::Source:
            case SmartGridOneEncoders::Bank::FilterAndAmp:
            case SmartGridOneEncoders::Bank::PanningAndSequencing:
            case SmartGridOneEncoders::Bank::VoiceLFOs:
            {
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    void DispatchModulationPaint(juce::Graphics& g, SmartGridOneEncoders::Bank bank)
    {
        SmartGridOneEncoders::BankMode modeForBank = m_nonagon->GetModeForEncoderBank(bank);

#define G(name, slot, ctor, mode, color) \
        if (m_##name && modeForBank == mode) \
        { \
            auto modBounds = GetModPanelBounds(slot); \
            int inset = std::max(1, static_cast<int>(std::ceil(std::min(modBounds.getWidth(), modBounds.getHeight()) * 0.02f))); \
            auto borderRect = modBounds.reduced(inset / 2); \
            auto innerBounds = modBounds.reduced(inset); \
            float cornerRadius = std::min(modBounds.getWidth(), modBounds.getHeight()) * 0.06f; \
            SmartGrid::Color borderColor = color; \
            if (borderColor == SmartGrid::Color::Off) \
            { \
                borderColor = SmartGrid::Color::Grey; \
            } \
            g.setColour(juce::Colour(borderColor.m_red, borderColor.m_green, borderColor.m_blue)); \
            g.drawRoundedRectangle(borderRect.toFloat(), cornerRadius, static_cast<float>(inset)); \
            g.saveState(); \
            g.reduceClipRegion(innerBounds); \
            g.setOrigin(innerBounds.getX(), innerBounds.getY()); \
            m_##name->Draw(g, juce::Rectangle<int>(0, 0, innerBounds.getWidth(), innerBounds.getHeight())); \
            g.restoreState(); \
        }
#include "ForEachModulationVisualizer.hpp"
#undef G
    }

    void DispatchModulationClick(const juce::MouseEvent& event, SmartGridOneEncoders::Bank bank)
    {
        auto position = event.getPosition();
        SmartGridOneEncoders::BankMode modeForBank = m_nonagon->GetModeForEncoderBank(bank);

#define G(name, slot, ctor, mode, color) \
        if (m_##name && modeForBank == mode) \
        { \
            auto modBounds = GetModPanelBounds(slot); \
            if (modBounds.contains(position)) \
            { \
                auto localEvent = event.withNewPosition(position - modBounds.getPosition()); \
                m_##name->OnClick(localEvent); \
            } \
        }
#include "ForEachModulationVisualizer.hpp"
#undef G
    }
    
    void ExcludeEncoderClip(juce::Graphics& g)
    {
        // Exclude the entire upper-right area above the metering component
        //
        int x = m_gridX + x_meterGridX * m_cellSize;
        int y = m_gridY;
        int width = x_meterGridWidth * m_cellSize;
        int height = x_meterGridY * m_cellSize;
        g.excludeClipRegion(juce::Rectangle<int>(x, y, width, height));
    }
    
    bool IsInEncoderRegion(juce::Point<int> position)
    {
        int x = m_gridX + x_meterGridX * m_cellSize;
        int y = m_gridY;
        int width = x_meterGridWidth * m_cellSize;
        int height = x_meterGridY * m_cellSize;
        return juce::Rectangle<int>(x, y, width, height).contains(position);
    }
    
    juce::Rectangle<int> GetEncoderBounds(int gridX, int gridY)
    {
        int x = m_gridX + (gridX * m_cellSize);
        int y = m_gridY + (gridY * m_cellSize);
        int width = x_encoderGridWidth * m_cellSize;
        int height = x_encoderGridHeight * m_cellSize;
        return juce::Rectangle<int>(x, y, width, height);
    }
};
