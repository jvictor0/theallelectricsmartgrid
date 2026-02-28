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
    
    NonagonWrapper* m_nonagon;
    
    int m_cellSize;
    int m_gridX;
    int m_gridY;
    int m_gridWidth;
    int m_gridHeight;
    
    int m_scopeVoiceOffset;
    
    std::unique_ptr<MeteringComponent> m_meteringComponent;
    
#define F(name, bank, block, ctor) \
    std::unique_ptr<SmartGridOneMainVisualizerComponent> m_##name;
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
    
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
        
#define F(name, bank, block, ctor) \
        m_##name = ctor;
#include "ForEachSmartGridOneVisualizer.hpp"
#undef F
        
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
    
    void DispatchPaint(juce::Graphics& g, SmartGridOneEncoders::Bank bank)
    {
        // Draw visualizers for standard blocks (0-3)
        //
#define F(name, vizBank, block, ctor) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block >= 0 && block < 4) \
        { \
            if (m_##name) \
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
#define F(name, vizBank, block, ctor) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block == -1) \
        { \
            if (m_##name) \
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
        
#define F(name, vizBank, block, ctor) \
        if (bank == SmartGridOneEncoders::Bank::vizBank && block >= 0 && block < 4) \
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
        
#define F(name, vizBank, block, ctor) \
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
