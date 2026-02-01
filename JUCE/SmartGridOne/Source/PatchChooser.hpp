#pragma once

#include <JuceHeader.h>
#include "IOUtils.hpp"

//==============================================================================
// Custom patch chooser that lists patch directories from the patches folder
//
class PatchChooser : public juce::Component, public juce::ListBoxModel
{
public:
    //==============================================================================
    PatchChooser(std::function<void(juce::String)> onPatchSelected, std::function<void()> onCancel, bool isSaveMode = false)
        : m_onPatchSelected(onPatchSelected)
        , m_onCancel(onCancel)
        , m_isSaveMode(isSaveMode)
        , m_patchListBox("Patches", this)
        , m_patchNameLabel("Patch Name:", "Patch Name:")
        , m_patchNameInput("")
        , m_okButton("OK")
        , m_cancelButton("Cancel")
    {
        // Set up patch list box
        //
        m_patchListBox.setRowHeight(30);
        m_patchListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
        addAndMakeVisible(m_patchListBox);
        
        // Set up patch name input (for save mode)
        //
        if (m_isSaveMode)
        {
            m_patchNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            m_patchNameLabel.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(m_patchNameLabel);
            
            m_patchNameInput.setMultiLine(false);
            m_patchNameInput.setReturnKeyStartsNewLine(false);
            m_patchNameInput.setText("NewPatch", false);
            m_patchNameInput.selectAll();
            m_patchNameInput.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkgrey);
            m_patchNameInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            m_patchNameInput.setColour(juce::TextEditor::highlightColourId, juce::Colours::blue);
            m_patchNameInput.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
            m_patchNameInput.setColour(juce::TextEditor::outlineColourId, juce::Colours::lightgrey);
            addAndMakeVisible(m_patchNameInput);
        }
        
        // Set up buttons
        //
        m_okButton.setSize(80, 30);
        m_okButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_okButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_okButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_okButton.onClick = [this]() { OnOkClicked(); };
        addAndMakeVisible(m_okButton);
        
        m_cancelButton.setSize(80, 30);
        m_cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_cancelButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_cancelButton.onClick = [this]() { if (m_onCancel) m_onCancel(); };
        addAndMakeVisible(m_cancelButton);
        
        RefreshPatchList();
    }

    ~PatchChooser() override
    {
    }

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(18.0f));
        juce::String title = m_isSaveMode ? "Save Patch" : "Load Patch";
        g.drawText(title, getLocalBounds().removeFromTop(40), juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(50); // Space for title
        
        if (m_isSaveMode)
        {
            // Patch name label and input at top
            //
            auto nameArea = bounds.removeFromTop(50);
            m_patchNameLabel.setBounds(nameArea.removeFromTop(20).reduced(10, 0));
            m_patchNameInput.setBounds(nameArea.reduced(10, 5));
            bounds.removeFromTop(10);
        }
        
        // Patch list in the middle
        //
        m_patchListBox.setBounds(bounds.removeFromTop(bounds.getHeight() - 50).reduced(10, 5));
        
        // Buttons at bottom
        //
        auto buttonArea = bounds.reduced(10, 5);
        m_cancelButton.setBounds(buttonArea.removeFromRight(80));
        buttonArea.removeFromRight(10);
        m_okButton.setBounds(buttonArea.removeFromRight(80));
    }
    
    //==============================================================================
    int getNumRows() override
    {
        return m_patchDirs.size();
    }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.fillAll(juce::Colours::darkblue);
        }
        else
        {
            g.fillAll(juce::Colours::darkgrey);
        }
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(14.0f));
        
        if (rowNumber >= 0 && rowNumber < m_patchDirs.size())
        {
            g.drawText(m_patchDirs[rowNumber].getFileName(), 10, 0, width - 20, height, juce::Justification::centredLeft);
        }
    }
    
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < m_patchDirs.size())
        {
            if (m_isSaveMode)
            {
                // For save mode, populate the patch name input with selected patch name
                //
                m_patchNameInput.setText(m_patchDirs[row].getFileName(), false);
            }
            else
            {
                // For load mode, select immediately
                //
                OnOkClicked();
            }
        }
    }
    
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < m_patchDirs.size())
        {
            OnOkClicked();
        }
    }

private:
    //==============================================================================
    void RefreshPatchList()
    {
        m_patchDirs.clear();
        
        // Get all subdirectories from patches directory
        //
        juce::File patchesDir = FileManager::GetPatchesDirectory();
        if (patchesDir.exists())
        {
            patchesDir.findChildFiles(m_patchDirs, juce::File::findDirectories, false);
        }
        
        // Sort alphabetically by directory name
        //
        m_patchDirs.sort();
        
        m_patchListBox.updateContent();
    }
    
    void OnOkClicked()
    {
        if (m_isSaveMode)
        {
            juce::String patchName = m_patchNameInput.getText().trim();
            if (patchName.isEmpty())
            {
                patchName = "NewPatch";
            }
            
            // Remove any characters that might cause filesystem issues
            //
            patchName = patchName.replaceCharacters("/\\:*?\"<>|", "_________");
            
            if (m_onPatchSelected)
            {
                m_onPatchSelected(patchName);
            }
        }
        else
        {
            // Load mode - use selected directory
            //
            int selectedRow = m_patchListBox.getSelectedRow();
            if (selectedRow >= 0 && selectedRow < m_patchDirs.size())
            {
                if (m_onPatchSelected)
                {
                    m_onPatchSelected(m_patchDirs[selectedRow].getFileName());
                }
            }
        }
    }
    
    std::function<void(juce::String)> m_onPatchSelected;
    std::function<void()> m_onCancel;
    bool m_isSaveMode;
    
    juce::ListBox m_patchListBox;
    juce::Label m_patchNameLabel;
    juce::TextEditor m_patchNameInput;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;
    
    juce::Array<juce::File> m_patchDirs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchChooser)
};

//==============================================================================
// Version chooser that lists timestamped JSON files from a patch directory
//
class VersionChooser : public juce::Component, public juce::ListBoxModel
{
public:
    //==============================================================================
    VersionChooser(const juce::String& patchName, std::function<void(juce::String)> onVersionSelected, std::function<void()> onCancel)
        : m_patchName(patchName)
        , m_onVersionSelected(onVersionSelected)
        , m_onCancel(onCancel)
        , m_versionListBox("Versions", this)
        , m_okButton("OK")
        , m_cancelButton("Cancel")
    {
        // Set up version list box
        //
        m_versionListBox.setRowHeight(30);
        m_versionListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
        addAndMakeVisible(m_versionListBox);
        
        // Set up buttons
        //
        m_okButton.setSize(80, 30);
        m_okButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_okButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_okButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_okButton.onClick = [this]() { OnOkClicked(); };
        addAndMakeVisible(m_okButton);
        
        m_cancelButton.setSize(80, 30);
        m_cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        m_cancelButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        m_cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        m_cancelButton.onClick = [this]() { if (m_onCancel) m_onCancel(); };
        addAndMakeVisible(m_cancelButton);
        
        RefreshVersionList();
    }

    ~VersionChooser() override
    {
    }

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(18.0f));
        g.drawText("Versions: " + m_patchName, getLocalBounds().removeFromTop(40), juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(50); // Space for title
        
        // Version list in the middle
        //
        m_versionListBox.setBounds(bounds.removeFromTop(bounds.getHeight() - 50).reduced(10, 5));
        
        // Buttons at bottom
        //
        auto buttonArea = bounds.reduced(10, 5);
        m_cancelButton.setBounds(buttonArea.removeFromRight(80));
        buttonArea.removeFromRight(10);
        m_okButton.setBounds(buttonArea.removeFromRight(80));
    }
    
    //==============================================================================
    int getNumRows() override
    {
        return m_versionFiles.size();
    }
    
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.fillAll(juce::Colours::darkblue);
        }
        else
        {
            g.fillAll(juce::Colours::darkgrey);
        }
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(14.0f));
        
        if (rowNumber >= 0 && rowNumber < m_versionFiles.size())
        {
            // Display filename without .json extension, formatted nicely
            //
            juce::String displayName = m_versionFiles[rowNumber].getFileNameWithoutExtension();
            
            // Convert timestamp format YYYY-MM-DDTHH-MM-SS to more readable format
            //
            displayName = displayName.replace("T", " ").replace("-", ":", 3);
            
            g.drawText(displayName, 10, 0, width - 20, height, juce::Justification::centredLeft);
        }
    }
    
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        // Single click just selects, double click or OK button loads
        //
    }
    
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < m_versionFiles.size())
        {
            OnOkClicked();
        }
    }

private:
    //==============================================================================
    void RefreshVersionList()
    {
        m_versionFiles.clear();
        
        // Get all JSON files from the patch directory
        //
        juce::File patchDir = FileManager::GetPatchDirectory(m_patchName);
        if (patchDir.exists())
        {
            patchDir.findChildFiles(m_versionFiles, juce::File::findFiles, false, "*.json");
        }
        
        // Sort in reverse order (newest first)
        //
        m_versionFiles.sort();
        std::reverse(m_versionFiles.begin(), m_versionFiles.end());
        
        m_versionListBox.updateContent();
    }
    
    void OnOkClicked()
    {
        int selectedRow = m_versionListBox.getSelectedRow();
        if (selectedRow >= 0 && selectedRow < m_versionFiles.size())
        {
            if (m_onVersionSelected)
            {
                m_onVersionSelected(m_versionFiles[selectedRow].getFullPathName());
            }
        }
    }
    
    juce::String m_patchName;
    std::function<void(juce::String)> m_onVersionSelected;
    std::function<void()> m_onCancel;
    
    juce::ListBox m_versionListBox;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;
    
    juce::Array<juce::File> m_versionFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VersionChooser)
};
