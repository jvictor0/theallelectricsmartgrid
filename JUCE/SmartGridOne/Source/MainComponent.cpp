#include "MainComponent.h"
#include "MainComponentMenuBarModel.hpp"

//==============================================================================
MainComponent::MainComponent()
    : m_configPage(nullptr)
    , m_wrldBuildrGrid(nullptr)
    , m_configButton("Config")
    , m_backButton("Back")
    , m_showingConfig(false)
    , m_menuBarModel(std::make_unique<MainComponentMenuBarModel>(this))
    , m_menuBar(m_menuBarModel.get())
{
    setSize (1000, 700);
    
    // Set up menu bar
    addAndMakeVisible(m_menuBar);
    
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
    
    // Menu bar at the top
    m_menuBar.setBounds(bounds.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
    
    // Config button in top right
    m_configButton.setBounds(bounds.removeFromRight(50).removeFromTop(50));
    
    // Back button in top left (only visible when showing config)
    if (m_showingConfig)
    {
        m_backButton.setBounds(bounds.removeFromLeft(100).removeFromTop(50));
    }
    
    // Config page takes up the rest of the space
    if (m_configPage && m_showingConfig)
    {
        m_configPage->setBounds(bounds);
    }
    else if (m_wrldBuildrGrid && !m_showingConfig)
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
    m_configButton.setVisible(false);
    m_backButton.setVisible(true);
    
    if (m_configPage)
    {
        m_configPage->setVisible(true);
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
    m_configButton.setVisible(true);
    m_backButton.setVisible(false);
    
    if (m_configPage)
    {
        removeChildComponent(m_configPage.get());
        m_configPage.reset();
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(true);
    }
    
    resized();
    repaint();

    SaveConfig();
}

//==============================================================================
void MainComponent::timerCallback()
{
    // Re-renderthe component at 60 FPS
    //
    repaint();
}
