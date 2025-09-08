#pragma once

#include <JuceHeader.h>
#include "EncoderComponent.hpp"
#include <memory>

struct EncoderGrid : public juce::Component
{
    static constexpr int x_encoderSpacing = 2; // Reduced spacing for tighter grid
    
    std::vector<std::unique_ptr<EncoderComponent>> m_encoders;
    EncoderBankUI m_ui;
    int m_cellSize;
    int m_startX;
    int m_startY;
    int m_endX;
    int m_endY;
    int m_gridWidth;
    int m_gridHeight;
    
    EncoderGrid(EncoderBankUI uiState, int startX, int startY, int endX, int endY)
        : m_ui(uiState)
        , m_cellSize(100) // Default cell size
        , m_startX(startX)
        , m_startY(startY)
        , m_endX(endX)
        , m_endY(endY)
        , m_gridWidth(endX - startX)
        , m_gridHeight(endY - startY)
    {
        // Create encoders only within the specified range
        //
        for (int y = startY; y < endY; ++y)
        {
            for (int x = startX; x < endX; ++x)
            {
                // Create the encoder component
                //
                m_encoders.push_back(std::make_unique<EncoderComponent>(m_ui, x, y));
                
                // Add the encoder as a child component
                //
                addAndMakeVisible(m_encoders.back().get());
            }
        }
        
        UpdateEncoderPositions();
    }
    
    void SetCellSize(int cellSize)
    {
        m_cellSize = cellSize;
        UpdateEncoderPositions();
    }
    
    void UpdateEncoderPositions()
    {
        // Calculate encoder size to fit within cell with spacing
        //
        int encoderSize = m_cellSize - x_encoderSpacing;
        
        // Position and size each encoder
        //
        int encoderIndex = 0;
        for (int y = m_startY; y < m_endY; ++y)
        {
            for (int x = m_startX; x < m_endX; ++x)
            {
                if (encoderIndex < m_encoders.size() && m_encoders[encoderIndex])
                {
                    // Calculate local position within the subgrid
                    //
                    int localX = x - m_startX;
                    int localY = y - m_startY;
                    
                    // Center the encoder within its grid cell
                    //
                    int encoderX = localX * m_cellSize + x_encoderSpacing / 2;
                    int encoderY = localY * m_cellSize + x_encoderSpacing / 2;
                    
                    m_encoders[encoderIndex]->setTopLeftPosition(encoderX, encoderY);
                    m_encoders[encoderIndex]->SetSize(encoderSize);
                }
                encoderIndex++;
            }
        }
        
        // Update component size
        //
        int totalWidth = m_cellSize * m_gridWidth;
        int totalHeight = m_cellSize * m_gridHeight;
        setSize(totalWidth, totalHeight);
    }
    
    void resized() override
    {
        // Update encoder positions when the component is resized
        //
        UpdateEncoderPositions();
    }
    
    // Get a specific encoder by grid coordinates (in global grid space)
    //
    EncoderComponent* GetEncoder(int x, int y)
    {
        if (x >= m_startX && x < m_endX && y >= m_startY && y < m_endY)
        {
            // Convert global coordinates to local index
            //
            int localX = x - m_startX;
            int localY = y - m_startY;
            int index = localY * m_gridWidth + localX;
            
            if (index >= 0 && index < m_encoders.size())
            {
                return m_encoders[index].get();
            }
        }

        return nullptr;
    }
    
    // Get encoder by local index (0 to gridWidth*gridHeight-1)
    //
    EncoderComponent* GetEncoder(int index)
    {
        if (index >= 0 && index < m_encoders.size())
        {
            return m_encoders[index].get();
        }

        return nullptr;
    }
};
