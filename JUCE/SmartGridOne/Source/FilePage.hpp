#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    File page for file operations (Open, Save, Save As)
*/
class FilePage : public juce::Component
{
public:
    //==============================================================================
    FilePage(std::function<void()> onOpen, std::function<void()> onSave, std::function<void()> onSaveAs)
        : m_onOpen(onOpen)
        , m_onSave(onSave)
        , m_onSaveAs(onSaveAs)
        , m_openButton("Open")
        , m_saveButton("Save")
        , m_saveAsButton("Save As")
    {
        // Set up buttons
        m_openButton.setSize(120, 40);
        m_openButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_openButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_openButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_openButton.onClick = [this]() { if (m_onOpen) m_onOpen(); };
        addAndMakeVisible(m_openButton);
        
        m_saveButton.setSize(120, 40);
        m_saveButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_saveButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_saveButton.onClick = [this]() { if (m_onSave) m_onSave(); };
        addAndMakeVisible(m_saveButton);
        
        m_saveAsButton.setSize(120, 40);
        m_saveAsButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_saveAsButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_saveAsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_saveAsButton.onClick = [this]() { if (m_onSaveAs) m_onSaveAs(); };
        addAndMakeVisible(m_saveAsButton);
    }

    ~FilePage() override
    {
    }

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(20.0f));
        g.drawText("File Operations", getLocalBounds().removeFromTop(50), juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(60); // Space for title
        
        // Center the file operation buttons
        auto centerArea = bounds.reduced(50);
        const int buttonHeight = 40;
        const int buttonSpacing = 20;
        const int totalButtonHeight = (buttonHeight * 3) + (buttonSpacing * 2);
        
        auto buttonArea = centerArea.removeFromTop(totalButtonHeight);
        buttonArea = buttonArea.withSizeKeepingCentre(120, totalButtonHeight);
        
        m_openButton.setBounds(buttonArea.removeFromTop(buttonHeight));
        buttonArea.removeFromTop(buttonSpacing);
        m_saveButton.setBounds(buttonArea.removeFromTop(buttonHeight));
        buttonArea.removeFromTop(buttonSpacing);
        m_saveAsButton.setBounds(buttonArea.removeFromTop(buttonHeight));
    }

private:
    //==============================================================================
    std::function<void()> m_onOpen;
    std::function<void()> m_onSave;
    std::function<void()> m_onSaveAs;
    
    juce::TextButton m_openButton;
    juce::TextButton m_saveButton;
    juce::TextButton m_saveAsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilePage)
};
