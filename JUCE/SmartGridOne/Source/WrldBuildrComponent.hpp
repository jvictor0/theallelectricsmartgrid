#pragma once

#include <JuceHeader.h>
#include "BasicPadGrid.hpp"
#include "EncoderGrid.hpp"
#include "FaderComponent.hpp"
#include "JoyStickComponent.hpp"
#include "ScopeComponent.hpp"
#include "SmartGridInclude.hpp"
#include "MeterComponent.hpp"
#include "MasteringComponents.hpp"

struct BasicPadGridHolder
{
    std::unique_ptr<BasicPadGrid> m_padGrid;
    int m_gridX;
    int m_gridY; 

    int m_padWidth;
    int m_padHeight;
    
    BasicPadGridHolder(std::unique_ptr<BasicPadGrid> padGrid, int gridX, int gridY)
        : m_padGrid(std::move(padGrid))
        , m_gridX(gridX)
        , m_gridY(gridY)
        , m_padWidth(1)
        , m_padHeight(1)
    {
    }
    
    void SetPosition(int gridX, int gridY)
    {
        m_gridX = gridX;
        m_gridY = gridY;
    }

    void OnPress(int x, int y, size_t timestamp)
    {
        if (m_padGrid)
        {
            m_padGrid->OnPress(x, y, timestamp);
        }
    }

    void OnRelease(int x, int y, size_t timestamp)
    {
        if (m_padGrid)
        {
            m_padGrid->OnRelease(x, y, timestamp);
        }
    }
    void SetBounds(int cellSize, int mainGridX, int mainGridY)
    {
        if (m_padGrid)
        {
            int subgridX = mainGridX + (m_gridX * cellSize);
            int subgridY = mainGridY + (m_gridY * cellSize);
            int subgridWidth = m_padGrid->m_gridWidth * cellSize * m_padWidth;
            int subgridHeight = m_padGrid->m_gridHeight * cellSize * m_padHeight;
            
            m_padGrid->setBounds(subgridX, subgridY, subgridWidth, subgridHeight);
            m_padGrid->SetCellSize(cellSize * m_padWidth, cellSize * m_padHeight);
        }
    }
};

struct EncoderGridHolder
{
    std::unique_ptr<EncoderGrid> m_encoderGrid;
    int m_gridX;
    int m_gridY;
    
    EncoderGridHolder(std::unique_ptr<EncoderGrid> encoderGrid, int gridX, int gridY)
        : m_encoderGrid(std::move(encoderGrid))
        , m_gridX(gridX)
        , m_gridY(gridY)
    {
    }
    
    void SetPosition(int gridX, int gridY)
    {
        m_gridX = gridX;
        m_gridY = gridY;
    }
    
    void SetBounds(int cellSize, int mainGridX, int mainGridY)
    {
        if (m_encoderGrid)
        {
            int subgridX = mainGridX + (m_gridX * cellSize);
            int subgridY = mainGridY + (m_gridY * cellSize);
            int subgridWidth = m_encoderGrid->m_gridWidth * cellSize;
            int subgridHeight = m_encoderGrid->m_gridHeight * cellSize;
            
            m_encoderGrid->setBounds(subgridX, subgridY, subgridWidth, subgridHeight);
            m_encoderGrid->SetCellSize(2 * cellSize);
        }
    }
};

struct FaderHolder
{
    std::unique_ptr<FaderComponent> m_fader;
    int m_gridX;
    int m_gridY;
    int m_faderSize;
    int m_faderLength;
    bool m_isVertical;
    
    FaderHolder(std::unique_ptr<FaderComponent> fader, int gridX, int gridY, int faderSize, int faderLength)
        : m_fader(std::move(fader))
        , m_gridX(gridX)
        , m_gridY(gridY)
        , m_faderSize(faderSize)
        , m_faderLength(faderLength)
    {
    }
    
    void SetPosition(int gridX, int gridY)
    {
        m_gridX = gridX;
        m_gridY = gridY;
    }
    
    void SetSize(int faderSize, int faderLength)
    {
        m_faderSize = faderSize;
        m_faderLength = faderLength;
    }
    
    void SetBounds(int cellSize, int mainGridX, int mainGridY)
    {
        if (m_fader)
        {
            int faderX = mainGridX + (m_gridX * cellSize);
            int faderY = mainGridY + (m_gridY * cellSize);
            int faderWidth = m_fader->m_isVertical ? m_faderSize * cellSize : m_faderLength * cellSize;
            int faderHeight = m_fader->m_isVertical ? m_faderLength * cellSize : m_faderSize * cellSize;
            
            m_fader->setBounds(faderX, faderY, faderWidth, faderHeight);            
            m_fader->SetSize(m_faderSize * cellSize, m_faderLength * cellSize);
        }
    }
};

struct JoyStickHolder
{
    std::unique_ptr<JoyStickComponent> m_joyStick;
    int m_gridX;
    int m_gridY;
    
    JoyStickHolder(std::unique_ptr<JoyStickComponent> joyStick, int gridX, int gridY)
        : m_joyStick(std::move(joyStick))
        , m_gridX(gridX)
        , m_gridY(gridY)
    {
    }
    
    void SetPosition(int gridX, int gridY)
    {
        m_gridX = gridX;
        m_gridY = gridY;
    }
    
    void SetBounds(int cellSize, int mainGridX, int mainGridY)
    {
        if (m_joyStick)
        {
            int joyStickX = mainGridX + (m_gridX * cellSize);
            int joyStickY = mainGridY + (m_gridY * cellSize);
            int joyStickWidth = 4 * cellSize;
            int joyStickHeight = 4 * cellSize;
            
            m_joyStick->setBounds(joyStickX, joyStickY, joyStickWidth, joyStickHeight);
            m_joyStick->SetSize(4 * cellSize);
        }
    }
};

struct ScopeComponentHolder
{
    std::unique_ptr<juce::Component> m_scopeComponent;
    int m_gridX;
    int m_gridY;
    int m_scopeWidth;
    int m_scopeHeight;
    
    ScopeComponentHolder(std::unique_ptr<juce::Component> scopeComponent, int gridX, int gridY, int scopeWidth, int scopeHeight)
        : m_scopeComponent(std::move(scopeComponent))
        , m_gridX(gridX)
        , m_gridY(gridY)
        , m_scopeWidth(scopeWidth)
        , m_scopeHeight(scopeHeight)
    {
    }
    
    void SetPosition(int gridX, int gridY)
    {
        m_gridX = gridX;
        m_gridY = gridY;
    }
    
    void SetSize(int scopeWidth, int scopeHeight)
    {
        m_scopeWidth = scopeWidth;
        m_scopeHeight = scopeHeight;
    }
    
    void SetBounds(int cellSize, int mainGridX, int mainGridY)
    {
        if (m_scopeComponent)
        {
            int scopeX = mainGridX + (m_gridX * cellSize);
            int scopeY = mainGridY + (m_gridY * cellSize);
            int scopeWidth = m_scopeWidth * cellSize;
            int scopeHeight = m_scopeHeight * cellSize;
            
            m_scopeComponent->setBounds(scopeX, scopeY, scopeWidth, scopeHeight);
        }
    }
};

struct WrldBuildrComponent : public juce::Component
{
    static constexpr int x_gridWidth = 24;
    static constexpr int x_gridHeight = 18;
    static constexpr double x_aspectRatio = static_cast<double>(x_gridWidth) / static_cast<double>(x_gridHeight); 

    NonagonWrapper* m_nonagon;
    
    int m_cellSize;
    int m_gridX;
    int m_gridY;
    int m_gridWidth;
    int m_gridHeight;

    int m_scopeVoiceOffset;
    
    std::unique_ptr<BasicPadGridHolder> m_leftPadGrid;
    std::unique_ptr<BasicPadGridHolder> m_rightPadGrid;
    std::unique_ptr<BasicPadGridHolder> m_bottomCenterGrid;
    std::unique_ptr<BasicPadGridHolder> m_belowEncodersGrid;
    std::unique_ptr<BasicPadGridHolder> m_leftTopGrid;
    std::unique_ptr<BasicPadGridHolder> m_timerGrid;
    std::unique_ptr<BasicPadGridHolder> m_leftArcadeGrid;
    std::unique_ptr<BasicPadGridHolder> m_rightArcadeGrid;

    std::unique_ptr<EncoderGridHolder> m_encoderGrids[4];

    std::unique_ptr<FaderHolder> m_faderGrids[8];

    std::unique_ptr<FaderHolder> m_xFader;

    std::unique_ptr<JoyStickHolder> m_joyStickReturn;
    std::unique_ptr<JoyStickHolder> m_joyStickFixed;

    std::unique_ptr<ScopeComponentHolder> m_vcoOneScope;
    std::unique_ptr<ScopeComponentHolder> m_vcoTwoScope;
    std::unique_ptr<ScopeComponentHolder> m_postFilterScope;
    std::unique_ptr<ScopeComponentHolder> m_postAmpScope;
    std::unique_ptr<ScopeComponentHolder> m_controlScope[4];

    std::unique_ptr<ScopeComponentHolder> m_voiceMeter;
    std::unique_ptr<ScopeComponentHolder> m_analyzer;
    std::unique_ptr<ScopeComponentHolder> m_quadAnalyzer[2];
    std::unique_ptr<ScopeComponentHolder> m_theoryOfTimeScope;

    std::unique_ptr<ScopeComponentHolder> m_soundStage;
    std::unique_ptr<ScopeComponentHolder> m_melodyRoll;

    std::unique_ptr<ScopeComponentHolder> m_delayAnalyzer[2];
    std::unique_ptr<ScopeComponentHolder> m_reverbAnalyzer[2];

    std::unique_ptr<ScopeComponentHolder> m_multibandEQ;
    std::unique_ptr<ScopeComponentHolder> m_multibandGainReduction;
    std::unique_ptr<ScopeComponentHolder> m_sourceMixerFrequency;
    std::unique_ptr<ScopeComponentHolder> m_sourceMixerReduction;

    bool m_drawGrid;

    bool m_initialized;
    
    WrldBuildrComponent(NonagonWrapper* nonagonWrapper)
    {
        m_initialized = false;
        m_nonagon = nonagonWrapper;
        setSize(800, 600); // Default size
        setWantsKeyboardFocus(true);
        
        m_drawGrid = false;
        m_scopeVoiceOffset = -1;
        
        // Create the 8x8 pad grid for the range 0,0,8,8
        //
        auto leftPadGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::LeftGrid), 0, 0, 8, 8);
        m_leftPadGrid = std::make_unique<BasicPadGridHolder>(std::move(leftPadGrid), 0, 8);
        addAndMakeVisible(m_leftPadGrid->m_padGrid.get());

        auto rightPadGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::RightGrid), 0, 0, 8, 8);
        m_rightPadGrid = std::make_unique<BasicPadGridHolder>(std::move(rightPadGrid), 16, 8);
        addAndMakeVisible(m_rightPadGrid->m_padGrid.get());

        auto bottomCenterGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid), 0, 0, 8, 2);
        m_bottomCenterGrid = std::make_unique<BasicPadGridHolder>(std::move(bottomCenterGrid), 8, 16);
        addAndMakeVisible(m_bottomCenterGrid->m_padGrid.get());

        auto belowEncodersGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid), 0, 2, 8, 4);
        m_belowEncodersGrid = std::make_unique<BasicPadGridHolder>(std::move(belowEncodersGrid), 8, 5);
        m_belowEncodersGrid->m_padWidth = 2;
        addAndMakeVisible(m_belowEncodersGrid->m_padGrid.get());

        auto leftTopGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid), 0, 4, 4, 6);
        m_leftTopGrid = std::make_unique<BasicPadGridHolder>(std::move(leftTopGrid), 0, 6);
        addAndMakeVisible(m_leftTopGrid->m_padGrid.get());

        auto timerGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid), 4, 4, 8, 6);
        m_timerGrid = std::make_unique<BasicPadGridHolder>(std::move(timerGrid), 4, 2);
        addAndMakeVisible(m_timerGrid->m_padGrid.get());

        auto leftArcadeGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid), 0, 6, 4, 7);
        m_leftArcadeGrid = std::make_unique<BasicPadGridHolder>(std::move(leftArcadeGrid), 0, 16);
        m_leftArcadeGrid->m_padGrid->SetSkin(PadComponent::Skin::AcradeButton);
        m_leftArcadeGrid->m_padWidth = 2;
        m_leftArcadeGrid->m_padHeight = 2;
        addAndMakeVisible(m_leftArcadeGrid->m_padGrid.get());

        auto rightArcadeGrid = std::make_unique<BasicPadGrid>(m_nonagon->MkLaunchPadUIGridWrldBldr(TheNonagonSquiggleBoyWrldBldr::Routes::AuxGrid), 4, 6, 8, 7);
        m_rightArcadeGrid = std::make_unique<BasicPadGridHolder>(std::move(rightArcadeGrid), 16, 16);
        m_rightArcadeGrid->m_padGrid->SetSkin(PadComponent::Skin::AcradeButton);
        m_rightArcadeGrid->m_padWidth = 2;
        m_rightArcadeGrid->m_padHeight = 2;
        addAndMakeVisible(m_rightArcadeGrid->m_padGrid.get());
        
        for (int i = 0; i < 4; ++i)
        {
            auto encoderGrid = std::make_unique<EncoderGrid>(m_nonagon->MkEncoderBankUI(), 0, i, 4, i + 1);
            int xPos = 8 + 8 * (i / 2);
            int yPos = 3 * (i % 2);
            m_encoderGrids[i] = std::make_unique<EncoderGridHolder>(std::move(encoderGrid), xPos, yPos);
            addAndMakeVisible(m_encoderGrids[i]->m_encoderGrid.get());
        }

        for (int i = 0; i < 8; ++i)
        {
            auto faderGrid = std::make_unique<FaderComponent>(m_nonagon->MkAnalogUI(i + 1), true);
            int xPos = 8 + i;
            int yPos = 8;
            m_faderGrids[i] = std::make_unique<FaderHolder>(std::move(faderGrid), xPos, yPos, 1, 4);
            addAndMakeVisible(m_faderGrids[i]->m_fader.get());
        }

        auto xFader = std::make_unique<FaderComponent>(m_nonagon->MkAnalogUI(0), false);
        m_xFader = std::make_unique<FaderHolder>(std::move(xFader), 9, 13, 2, 6);
        addAndMakeVisible(m_xFader->m_fader.get());

        auto joyStickFixed = std::make_unique<JoyStickComponent>(
            m_nonagon->MkAnalogUI(9), m_nonagon->MkAnalogUI(10), m_nonagon->MkAnalogUI(11), m_nonagon->MkAnalogUI(12), false);
        m_joyStickReturn = std::make_unique<JoyStickHolder>(std::move(joyStickFixed), 0, 2);
        addAndMakeVisible(m_joyStickReturn->m_joyStick.get());

        auto joyStickReturn = std::make_unique<JoyStickComponent>(
            m_nonagon->MkAnalogUI(13), m_nonagon->MkAnalogUI(14), m_nonagon->MkAnalogUI(15), m_nonagon->MkAnalogUI(16), true);
        m_joyStickFixed = std::make_unique<JoyStickHolder>(std::move(joyStickReturn), 4, 4);
        addAndMakeVisible(m_joyStickFixed->m_joyStick.get());

        auto theoryOfTimeScope = std::make_unique<TheoryOfTimeScopeComponent>(m_nonagon->GetUIState());
        m_theoryOfTimeScope = std::make_unique<ScopeComponentHolder>(std::move(theoryOfTimeScope), 8, 8, 8, 8);
        addAndMakeVisible(m_theoryOfTimeScope->m_scopeComponent.get());

        TheNonagonSquiggleBoyInternal::UIState* uiState = m_nonagon->GetUIState();

        for (int i = 0; i < 4; ++i)
        {
            auto audioScope = std::make_unique<ScopeComponent>(i, m_nonagon->GetAudioScopeWriter(), &m_scopeVoiceOffset, ScopeComponent::ScopeType::Audio, uiState);
            int xPos = i == 0 ? 0 : 8 * (i - 1);
            if (i == 3)
            {
                xPos = 0;
            }

            int yPos = i == 0 ? 0 : 8;
            std::unique_ptr<ScopeComponentHolder>& scope = i == 0 ? m_vcoOneScope : i == 1 ? m_vcoTwoScope : i == 2 ? m_postFilterScope : m_postAmpScope;
            scope = std::make_unique<ScopeComponentHolder>(std::move(audioScope), xPos, yPos, 8, 8);
            addAndMakeVisible(scope->m_scopeComponent.get());
        }

        auto voiceMeter = std::make_unique<VoiceMeterComponent>(uiState);
        m_voiceMeter = std::make_unique<ScopeComponentHolder>(std::move(voiceMeter), 0, 0, 8, 8);
        addAndMakeVisible(m_voiceMeter->m_scopeComponent.get());

        auto analyzer = std::make_unique<AnalyserComponent>(
            WindowedFFT(m_nonagon->GetAudioScopeWriter(), static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp)), 
            &m_scopeVoiceOffset,
            uiState);
        m_analyzer = std::make_unique<ScopeComponentHolder>(std::move(analyzer), 16, 8, 8, 8);
        addAndMakeVisible(m_analyzer->m_scopeComponent.get());

        for (int i = 0; i < 2; ++i)
        {
            auto quadAnalyzer = std::make_unique<QuadAnalyserComponent>(
                uiState, 
                i == 0 ? false : true,
                QuadAnalyserComponent::Type::Master);
            m_quadAnalyzer[i] = std::make_unique<ScopeComponentHolder>(std::move(quadAnalyzer), 0, 8 - i * 8, 8, 8);
            addAndMakeVisible(m_quadAnalyzer[i]->m_scopeComponent.get());

            auto delayAnalyzer = std::make_unique<QuadAnalyserComponent>(
                uiState, 
                i == 0 ? false : true,
                QuadAnalyserComponent::Type::Delay);
            m_delayAnalyzer[i] = std::make_unique<ScopeComponentHolder>(std::move(delayAnalyzer), 0, 8 - i * 8, 8, 8);
            addAndMakeVisible(m_delayAnalyzer[i]->m_scopeComponent.get());

            auto reverbAnalyzer = std::make_unique<QuadAnalyserComponent>(
                uiState, 
                i == 0 ? false : true,
                QuadAnalyserComponent::Type::Reverb);
            m_reverbAnalyzer[i] = std::make_unique<ScopeComponentHolder>(std::move(reverbAnalyzer), 0, 8 - i * 8, 8, 8);
            addAndMakeVisible(m_reverbAnalyzer[i]->m_scopeComponent.get());
        }

        for (int i = 0; i < 4; ++i)
        {
            auto controlScope = std::make_unique<ScopeComponent>(i, m_nonagon->GetControlScopeWriter(), &m_scopeVoiceOffset, ScopeComponent::ScopeType::Control, uiState);
            int xPos = i == 0 ? 0 : 8 * (i - 1);
            int yPos = i == 0 ? 0 : 8;
            m_controlScope[i] = std::make_unique<ScopeComponentHolder>(std::move(controlScope), xPos, yPos, 8, 8);
            addAndMakeVisible(m_controlScope[i]->m_scopeComponent.get());
        }

        auto soundStage = std::make_unique<SoundStageComponent>(uiState);
        m_soundStage = std::make_unique<ScopeComponentHolder>(std::move(soundStage), 0, 0, 8, 8);
        addAndMakeVisible(m_soundStage->m_scopeComponent.get());

        auto melodyRoll = std::make_unique<MelodyRollComponent>(m_nonagon->GetNoteWriter());
        m_melodyRoll = std::make_unique<ScopeComponentHolder>(std::move(melodyRoll), 0, 8, 24, 8);
        addAndMakeVisible(m_melodyRoll->m_scopeComponent.get());

        auto multibandEQ = std::make_unique<MultibandEQComponent>(uiState);
        m_multibandEQ = std::make_unique<ScopeComponentHolder>(std::move(multibandEQ), 8, 8, 8, 8);
        addAndMakeVisible(m_multibandEQ->m_scopeComponent.get());

        auto multibandGainReduction = std::make_unique<MultibandGainReductionComponent>(uiState);
        m_multibandGainReduction = std::make_unique<ScopeComponentHolder>(std::move(multibandGainReduction), 16, 8, 8, 8);
        addAndMakeVisible(m_multibandGainReduction->m_scopeComponent.get());

        auto sourceMixerFrequency = std::make_unique<SourceMixerFrequencyComponent>(uiState);
        m_sourceMixerFrequency = std::make_unique<ScopeComponentHolder>(std::move(sourceMixerFrequency), 0, 8, 8, 8);
        addAndMakeVisible(m_sourceMixerFrequency->m_scopeComponent.get());

        auto sourceMixerReduction = std::make_unique<SourceMixerReductionComponent>(uiState);
        m_sourceMixerReduction = std::make_unique<ScopeComponentHolder>(std::move(sourceMixerReduction), 0, 0, 8, 8);
        addAndMakeVisible(m_sourceMixerReduction->m_scopeComponent.get());

        m_initialized = true;

        // Calculate initial layout
        //
        CalculateGridLayout();
        UpdatePadGridPosition();
        
        // Force a repaint to ensure everything is drawn correctly
        //
        repaint();
    }
    
    void resized() override
    {
        CalculateGridLayout();
        UpdatePadGridPosition();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Fill background
        //
        g.fillAll(juce::Colours::black);
        
        // Set grid line color
        //
        g.setColour(juce::Colours::white);
        
        if (m_drawGrid)
        {
            // Draw vertical grid lines
            //
            for (int x = 0; x <= x_gridWidth; ++x)
            {
                int lineX = m_gridX + (x * m_cellSize);
                g.drawVerticalLine(lineX, m_gridY, m_gridY + m_gridHeight);
            }
            
            // Draw horizontal grid lines
            //
            for (int y = 0; y <= x_gridHeight; ++y)
            {
                int lineY = m_gridY + (y * m_cellSize);
                g.drawHorizontalLine(lineY, m_gridX, m_gridX + m_gridWidth);
            }
        }
    }
    
    void CalculateGridLayout()
    {
        auto bounds = getLocalBounds();
        int availableWidth = bounds.getWidth();
        int availableHeight = bounds.getHeight();
        
        // Calculate aspect ratio of available space
        //
        double availableAspectRatio = static_cast<double>(availableWidth) / static_cast<double>(availableHeight);
        
        if (availableAspectRatio > x_aspectRatio)
        {
            // Available space is wider than 3:2, so height determines cell size
            //
            m_cellSize = availableHeight / x_gridHeight;
            m_gridHeight = m_cellSize * x_gridHeight;
            m_gridWidth = m_cellSize * x_gridWidth;
            
            // Center horizontally
            //
            m_gridX = (availableWidth - m_gridWidth) / 2;
            m_gridY = 0;
        }
        else
        {
            // Available space is taller than 3:2, so width determines cell size
            //
            m_cellSize = availableWidth / x_gridWidth;
            m_gridWidth = m_cellSize * x_gridWidth;
            m_gridHeight = m_cellSize * x_gridHeight;
            
            // Center vertically
            //
            m_gridX = 0;
            m_gridY = (availableHeight - m_gridHeight) / 2;
        }
    }

    void SetDisplayMode()
    {
        switch (m_nonagon->GetDisplayMode())
        {
            case TheNonagonSquiggleBoyWrldBldr::DisplayMode::Controller:
            {
                SetDisplayModeController(true);
                RemoveVisualizers();
                break;
            }
            case TheNonagonSquiggleBoyWrldBldr::DisplayMode::Visualizer:
            {
                SetDisplayModeController(false);
                SetVisualizerVisibility(m_nonagon->GetVisualDisplayMode());
                break;
            }
        }
    }

    void RemoveVisualizers()
    {
        m_vcoOneScope->m_scopeComponent->setVisible(false);
        m_vcoTwoScope->m_scopeComponent->setVisible(false);
        m_postFilterScope->m_scopeComponent->setVisible(false);
        m_postAmpScope->m_scopeComponent->setVisible(false);
        m_voiceMeter->m_scopeComponent->setVisible(false);
        m_analyzer->m_scopeComponent->setVisible(false);
        for (int i = 0; i < 2; ++i)
        {
            m_quadAnalyzer[i]->m_scopeComponent->setVisible(false);
            m_delayAnalyzer[i]->m_scopeComponent->setVisible(false);
            m_reverbAnalyzer[i]->m_scopeComponent->setVisible(false);
        }

        m_theoryOfTimeScope->m_scopeComponent->setVisible(false);

        for (int i = 0; i < 4; ++i)
        {
            m_controlScope[i]->m_scopeComponent->setVisible(false);
        }

        m_soundStage->m_scopeComponent->setVisible(false);
        m_melodyRoll->m_scopeComponent->setVisible(false);
    
        m_multibandEQ->m_scopeComponent->setVisible(false);
        m_multibandGainReduction->m_scopeComponent->setVisible(false);
        m_sourceMixerFrequency->m_scopeComponent->setVisible(false);
        m_sourceMixerReduction->m_scopeComponent->setVisible(false);
    }

    void SetVisualizerVisibility(SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode visualDisplayMode)
    {
        m_vcoOneScope->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Voice);
        m_vcoTwoScope->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Voice);
        m_postFilterScope->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Filter || 
                                                        visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Voice);
        m_postAmpScope->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Filter);
        m_voiceMeter->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Filter);
        m_analyzer->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Voice ||
                                                visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Filter);
        
        for (int i = 0; i < 2; ++i)
        {
            m_quadAnalyzer[i]->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::QuadMaster);
            m_delayAnalyzer[i]->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Delay);
            m_reverbAnalyzer[i]->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Reverb);
        }

        m_theoryOfTimeScope->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::QuadMaster);

        for (int i = 0; i < 4; ++i)
        {
            m_controlScope[i]->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::Control);
        }

        m_soundStage->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::PanAndMelody);
        m_melodyRoll->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::PanAndMelody);
        m_multibandEQ->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::StereoMaster);
        m_multibandGainReduction->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::StereoMaster);
        m_sourceMixerFrequency->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::StereoMaster);
        m_sourceMixerReduction->m_scopeComponent->setVisible(visualDisplayMode == SquiggleBoyWithEncoderBank::UIState::VisualDisplayMode::StereoMaster);
    }

    void SetDisplayModeController(bool isVisible)
    {
        m_leftPadGrid->m_padGrid->setVisible(isVisible);
        m_rightPadGrid->m_padGrid->setVisible(isVisible);
        m_leftTopGrid->m_padGrid->setVisible(isVisible);
        m_timerGrid->m_padGrid->setVisible(isVisible);
         
        for (int i = 0; i < 8; ++i)
        {
            m_faderGrids[i]->m_fader->setVisible(isVisible);
        }

        m_xFader->m_fader->setVisible(isVisible);
        m_joyStickReturn->m_joyStick->setVisible(isVisible);
        m_joyStickFixed->m_joyStick->setVisible(isVisible);
    }
        
    void UpdatePadGridPosition()
    {
        if (!m_initialized)
        {
            return;
        }

        m_leftPadGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_rightPadGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_bottomCenterGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_belowEncodersGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_leftTopGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_timerGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_leftArcadeGrid->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_rightArcadeGrid->SetBounds(m_cellSize, m_gridX, m_gridY);

        for (int i = 0; i < 4; ++i)
        {
            m_encoderGrids[i]->SetBounds(m_cellSize, m_gridX, m_gridY);
        }

        for (int i = 0; i < 8; ++i)
        {
            m_faderGrids[i]->SetBounds(m_cellSize, m_gridX, m_gridY);
        }

        m_xFader->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_joyStickReturn->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_joyStickFixed->SetBounds(m_cellSize, m_gridX, m_gridY);

        m_vcoOneScope->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_vcoTwoScope->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_postFilterScope->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_postAmpScope->SetBounds(m_cellSize, m_gridX, m_gridY);

        m_voiceMeter->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_analyzer->SetBounds(m_cellSize, m_gridX, m_gridY);
        for (int i = 0; i < 2; ++i)
        {
            m_quadAnalyzer[i]->SetBounds(m_cellSize, m_gridX, m_gridY);
            m_delayAnalyzer[i]->SetBounds(m_cellSize, m_gridX, m_gridY);
            m_reverbAnalyzer[i]->SetBounds(m_cellSize, m_gridX, m_gridY);
        }

        m_theoryOfTimeScope->SetBounds(m_cellSize, m_gridX, m_gridY);

        for (int i = 0; i < 4; ++i)
        {
            m_controlScope[i]->SetBounds(m_cellSize, m_gridX, m_gridY);
        }

        m_soundStage->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_melodyRoll->SetBounds(m_cellSize, m_gridX, m_gridY);

        m_multibandEQ->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_multibandGainReduction->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_sourceMixerFrequency->SetBounds(m_cellSize, m_gridX, m_gridY);
        m_sourceMixerReduction->SetBounds(m_cellSize, m_gridX, m_gridY);
    }
};
