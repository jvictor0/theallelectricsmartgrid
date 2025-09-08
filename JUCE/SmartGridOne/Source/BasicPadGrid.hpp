#pragma once

#include <JuceHeader.h>
#include "PadComponent.hpp"
#include "SmartGridInclude.hpp"
#include <memory>

struct BasicPadGrid : public juce::Component
{
    static constexpr int x_padSpacing = 2; // Reduced spacing for tighter grid
    
    std::vector<std::unique_ptr<PadComponent>> m_pads;
    PadUIGrid m_padUIGrid;
    int m_cellWidth;
    int m_cellHeight;
    int m_startX;
    int m_startY;
    int m_endX;
    int m_endY;
    int m_gridWidth;
    int m_gridHeight;

    BasicPadGrid(PadUIGrid padUIGrid, int startX, int startY, int endX, int endY)
        : m_padUIGrid(padUIGrid)
        , m_cellWidth(50) // Default cell size
        , m_cellHeight(50) // Default cell size
        , m_startX(startX)
        , m_startY(startY)
        , m_endX(endX)
        , m_endY(endY)
        , m_gridWidth(endX - startX)
        , m_gridHeight(endY - startY)
    {
        // Create pads only within the specified range
        //
        for (int y = startY; y < endY; ++y)
        {
            for (int x = startX; x < endX; ++x)
            {
                // Get the atomic color pointer from SmartBus
                //
                PadUI padUI = m_padUIGrid.MkPadUI(x, y);
                
                // Create the pad component
                //
                m_pads.push_back(std::make_unique<PadComponent>(padUI));
                
                // Add the pad as a child component
                //
                addAndMakeVisible(m_pads.back().get());
            }
        }
        
        UpdatePadPositions();
    }
    
    void SetCellSize(int cellWidth, int cellHeight)
    {
        m_cellWidth = cellWidth;
        m_cellHeight = cellHeight;
        UpdatePadPositions();
    }
    
    void SetSkin(PadComponent::Skin skin)
    {
        for (int i = 0; i < m_pads.size(); ++i)
        {
            m_pads[i]->m_skin = skin;
        }
    }
    
    void UpdatePadPositions()
    {
        // Calculate pad size to fit within cell with spacing
        //
        int padWidth = m_cellWidth - x_padSpacing;
        int padHeight = m_cellHeight - x_padSpacing;
        
        // Position and size each pad
        //
        int padIndex = 0;
        for (int y = m_startY; y < m_endY; ++y)
        {
            for (int x = m_startX; x < m_endX; ++x)
            {
                if (padIndex < m_pads.size() && m_pads[padIndex])
                {
                    // Calculate local position within the subgrid
                    //
                    int localX = x - m_startX;
                    int localY = y - m_startY;
                    
                    // Center the pad within its grid cell
                    //
                    int padX = localX * m_cellWidth + x_padSpacing / 2;
                    int padY = localY * m_cellHeight + x_padSpacing / 2;
                    
                    m_pads[padIndex]->setTopLeftPosition(padX, padY);
                    m_pads[padIndex]->SetSize(padWidth, padHeight);
                }
                padIndex++;
            }
        }
        
        // Update component size
        //
        int totalWidth = m_cellWidth * m_gridWidth;
        int totalHeight = m_cellHeight * m_gridHeight;
        setSize(totalWidth, totalHeight);
    }
    
    void resized() override
    {
        // Update pad positions when the component is resized
        //
        UpdatePadPositions();
    }
    
    // Get a specific pad by grid coordinates (in global grid space)
    //
    PadComponent* GetPad(int x, int y)
    {
        if (x >= m_startX && x < m_endX && y >= m_startY && y < m_endY)
        {
            // Convert global coordinates to local index
            //
            int localX = x - m_startX;
            int localY = y - m_startY;
            int index = localY * m_gridWidth + localX;
            
            if (index >= 0 && index < m_pads.size())
            {
                return m_pads[index].get();
            }
        }

        return nullptr;
    }
    
    // Get pad by local index (0 to gridWidth*gridHeight-1)
    //
    PadComponent* GetPad(int index)
    {
        if (index >= 0 && index < m_pads.size())
        {
            return m_pads[index].get();
        }

        return nullptr;
    }
};

