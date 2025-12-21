#pragma once

#include <JuceHeader.h>
#include "IOUtils.hpp"

//==============================================================================
/*
    Custom patch chooser that lists files from the app's Documents directory
*/
class PatchChooser : public juce::Component, public juce::ListBoxModel
{
public:
    //==============================================================================
    PatchChooser(std::function<void(juce::String)> onFileSelected, std::function<void()> onCancel, bool isSaveMode = false)
        : m_onFileSelected(onFileSelected)
        , m_onCancel(onCancel)
        , m_isSaveMode(isSaveMode)
        , m_fileListBox("Files", this)
        , m_filenameLabel("Filename:", "Filename:")
        , m_filenameInput("")
        , m_okButton("OK")
        , m_cancelButton("Cancel")
    {
        // Set up file list box
        m_fileListBox.setRowHeight(30);
        m_fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
        addAndMakeVisible(m_fileListBox);
        
        // Set up filename input (for save mode)
        if (m_isSaveMode)
        {
            m_filenameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            m_filenameLabel.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(m_filenameLabel);
            
            m_filenameInput.setMultiLine(false);
            m_filenameInput.setReturnKeyStartsNewLine(false);
            m_filenameInput.setText("patch.json", false);
            m_filenameInput.selectAll();
            m_filenameInput.setColour(juce::TextEditor::backgroundColourId, juce::Colours::darkgrey);
            m_filenameInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            m_filenameInput.setColour(juce::TextEditor::highlightColourId, juce::Colours::blue);
            m_filenameInput.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
            m_filenameInput.setColour(juce::TextEditor::outlineColourId, juce::Colours::lightgrey);
            addAndMakeVisible(m_filenameInput);
        }
        
        // Set up buttons
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
        
        RefreshFileList();
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
            // Filename label and input at top
            auto filenameArea = bounds.removeFromTop(50);
            m_filenameLabel.setBounds(filenameArea.removeFromTop(20).reduced(10, 0));
            m_filenameInput.setBounds(filenameArea.reduced(10, 5));
            bounds.removeFromTop(10);
        }
        
        // File list in the middle
        m_fileListBox.setBounds(bounds.removeFromTop(bounds.getHeight() - 50).reduced(10, 5));
        
        // Buttons at bottom
        auto buttonArea = bounds.reduced(10, 5);
        m_cancelButton.setBounds(buttonArea.removeFromRight(80));
        buttonArea.removeFromRight(10);
        m_okButton.setBounds(buttonArea.removeFromRight(80));
    }
    
    //==============================================================================
    int getNumRows() override
    {
        return m_files.size();
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
        
        if (rowNumber >= 0 && rowNumber < m_files.size())
        {
            g.drawText(m_files[rowNumber].getFileName(), 10, 0, width - 20, height, juce::Justification::centredLeft);
        }
    }
    
    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < m_files.size())
        {
            if (m_isSaveMode)
            {
                // For save mode, populate the filename input
                m_filenameInput.setText(m_files[row].getFileName(), false);
            }
            else
            {
                // For load mode, select immediately
                OnOkClicked();
            }
        }
    }
    
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < m_files.size())
        {
            OnOkClicked();
        }
    }

private:
    //==============================================================================
    void RefreshFileList()
    {
        m_files.clear();
        
        // Check both locations: iCloud (for reading) and app Documents (for saving)
        juce::Array<juce::File> allFiles;
        
        // First, check iCloud Drive location (read-only on iOS)
        juce::File iCloudDir = FileManager::GetiCloudSmartGridOneDirectory();
        if (iCloudDir.exists())
        {
            juce::Array<juce::File> iCloudFiles;
            iCloudDir.findChildFiles(iCloudFiles, juce::File::findFiles, false, "*.json");
            allFiles.addArray(iCloudFiles);
        }
        
        // Then, check app's Documents directory (where we save)
        juce::File appDir = FileManager::GetSmartGridOneDirectory();
        if (appDir.exists())
        {
            juce::Array<juce::File> appFiles;
            appDir.findChildFiles(appFiles, juce::File::findFiles, false, "*.json");
            allFiles.addArray(appFiles);
        }
        
        // Remove duplicates (in case same file exists in both locations)
        for (int i = allFiles.size() - 1; i >= 0; --i)
        {
            for (int j = i - 1; j >= 0; --j)
            {
                if (allFiles[i].getFileName() == allFiles[j].getFileName())
                {
                    // Prefer the app Documents version (writable)
                    if (allFiles[i].getFullPathName().contains("Mobile Documents"))
                    {
                        allFiles.remove(i);
                        break;
                    }
                    else
                    {
                        allFiles.remove(j);
                    }
                }
            }
        }
        
        // Sort by filename
        allFiles.sort();
        
        m_files = allFiles;
        
        m_fileListBox.updateContent();
    }
    
    void OnOkClicked()
    {
        if (m_isSaveMode)
        {
            juce::String filename = m_filenameInput.getText();
            if (filename.isEmpty())
            {
                filename = "patch.json";
            }
            
            // Ensure .json extension
            if (!filename.endsWith(".json"))
            {
                filename += ".json";
            }
            
            juce::File smartGridDir = FileManager::GetSmartGridOneDirectory();
            juce::String fullPath = smartGridDir.getChildFile(filename).getFullPathName();
            
            if (m_onFileSelected)
            {
                m_onFileSelected(fullPath);
            }
        }
        else
        {
            // Load mode - use selected file
            int selectedRow = m_fileListBox.getSelectedRow();
            if (selectedRow >= 0 && selectedRow < m_files.size())
            {
                if (m_onFileSelected)
                {
                    m_onFileSelected(m_files[selectedRow].getFullPathName());
                }
            }
        }
    }
    
    std::function<void(juce::String)> m_onFileSelected;
    std::function<void()> m_onCancel;
    bool m_isSaveMode;
    
    juce::ListBox m_fileListBox;
    juce::Label m_filenameLabel;
    juce::TextEditor m_filenameInput;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;
    
    juce::Array<juce::File> m_files;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchChooser)
};

