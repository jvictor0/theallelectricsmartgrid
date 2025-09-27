#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
    : m_configPage(nullptr)
    , m_filePage(nullptr)
    , m_wrldBuildrGrid(nullptr)
    , m_configButton("Config")
    , m_backButton("Back")
    , m_fileButton("File")
    , m_showingConfig(false)
    , m_showingFile(false)
{
    setSize (1000, 700);
    
    // Set up config button
    m_configButton.setButtonText("Config");
    m_configButton.setSize(40, 40);
    m_configButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_configButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    m_configButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_configButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
    m_configButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_configButton.onClick = [this]() { OnConfigButtonClicked(); };
    addAndMakeVisible(m_configButton);
    
    // Set up back button
    m_backButton.setSize(80, 30);
    m_backButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_backButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    m_backButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_backButton.onClick = [this]() { OnBackButtonClicked(); };
    addAndMakeVisible(m_backButton);
    m_backButton.setVisible(false);
    
    // Set up file button
    m_fileButton.setSize(50, 50);
    m_fileButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_fileButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    m_fileButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_fileButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
    m_fileButton.onClick = [this]() { OnFileButtonClicked(); };
    addAndMakeVisible(m_fileButton);
    
    // Set up world builder grid
    m_wrldBuildrGrid = std::make_unique<WrldBuildrComponent>(&m_nonagon);
    addAndMakeVisible(m_wrldBuildrGrid.get());
    
    // Ensure config page starts closed
    m_showingConfig = false;

    setAudioChannels(0, 4);

    // Start the 60 FPS timer for re-rendering
    //
    startTimer(1000 / 60); // 60 FPS

    LoadConfig();
    
    // Force initial layout calculation after everything is set up
    resized();
    
    // Ensure the component is properly sized and laid out
    setSize(1000, 700);
    resized();
    
    // Force the world builder grid to update its display mode
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->SetDisplayMode();
    }
    
    // Trigger a repaint to ensure everything is drawn correctly
    repaint();
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (!m_showingConfig)
    {
        g.setFont (juce::FontOptions (24.0f));
        g.setColour (juce::Colours::white);
        g.drawText ("SmartGrid One", getLocalBounds().removeFromTop(100), juce::Justification::centred, true);
        
        g.setFont (juce::FontOptions (16.0f));
        g.drawText ("Click the gear icon to configure launchpads", getLocalBounds().removeFromTop(150), juce::Justification::centred, true);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Right side panel area
    auto rightPanel = bounds.removeFromRight(50);
    
    // Config button in top right
    m_configButton.setBounds(rightPanel.removeFromTop(50));
    
    // File button directly below config button, same size
    m_fileButton.setBounds(rightPanel.removeFromTop(50));
    
    // Back button in top left (only visible when showing config or file)
    if (m_showingConfig || m_showingFile)
    {
        m_backButton.setBounds(bounds.removeFromLeft(100).removeFromTop(50));
    }
    
    // Show appropriate page
    if (m_configPage && m_showingConfig)
    {
        m_configPage->setBounds(bounds);
    }
    else if (m_filePage && m_showingFile)
    {
        m_filePage->setBounds(bounds);
    }
    else if (m_wrldBuildrGrid && !m_showingConfig && !m_showingFile)
    {
        // Let the world builder grid handle its own sizing and positioning
        //
        m_wrldBuildrGrid->setBounds(bounds);
    }
}

//==============================================================================
void MainComponent::OnConfigButtonClicked()
{
    if (!m_configPage)
    {
        m_configPage = std::make_unique<ConfigPage>(&m_nonagon);
        addAndMakeVisible(m_configPage.get());
    }
    
    m_showingConfig = true;
    m_showingFile = false;
    m_configButton.setVisible(false);
    m_fileButton.setVisible(false);
    m_backButton.setVisible(true);
    
    if (m_configPage)
    {
        m_configPage->setVisible(true);
    }
    
    if (m_filePage)
    {
        m_filePage->setVisible(false);
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(false);
    }
    
    resized();
    repaint();
}

void MainComponent::OnBackButtonClicked()
{
    m_showingConfig = false;
    m_showingFile = false;
    m_configButton.setVisible(true);
    m_fileButton.setVisible(true);
    m_backButton.setVisible(false);
    
    if (m_configPage)
    {
        removeChildComponent(m_configPage.get());
        m_configPage.reset();
    }
    
    if (m_filePage)
    {
        removeChildComponent(m_filePage.get());
        m_filePage.reset();
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(true);
    }
    
    resized();
    repaint();

    SaveConfig();
}

void MainComponent::OnFileButtonClicked()
{
    if (!m_filePage)
    {
        m_filePage = std::make_unique<FilePage>(
            [this]() { m_fileManager.ChooseLoadFile(); },
            [this]() { m_fileManager.ChooseSaveFile(false); },
            [this]() { m_fileManager.ChooseSaveFile(true); }
        );
        
        addAndMakeVisible(m_filePage.get());
    }
    
    m_showingFile = true;
    m_showingConfig = false;
    m_configButton.setVisible(false);
    m_fileButton.setVisible(false);
    m_backButton.setVisible(true);
    
    if (m_filePage)
    {
        m_filePage->setVisible(true);
    }
    
    if (m_configPage)
    {
        m_configPage->setVisible(false);
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(false);
    }
    
    resized();
    repaint();
}

void MainComponent::OnFileBackButtonClicked()
{
    OnBackButtonClicked();
}

//==============================================================================
void MainComponent::timerCallback()
{
    // Re-renderthe component at 60 FPS
    //
    HandleStateInterchange();
    m_wrldBuildrGrid->SetDisplayMode();
    repaint();
}
