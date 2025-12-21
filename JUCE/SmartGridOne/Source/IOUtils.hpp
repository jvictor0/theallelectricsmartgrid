#pragma once

#include <JuceHeader.h>
#include "NonagonWrapper.hpp"

class MainComponent;

struct FileManager
{    
    FileManager(MainComponent* mainComponent)
        : m_mainComponent(mainComponent)
    {
    }

    // Get the directory for SAVING patches (always app's Documents folder)
    static juce::File GetSmartGridOneDirectory();
    
    // Get the iCloud Drive directory for READING patches (if available)
    // On iOS: May not be accessible without iCloud capability, returns empty if not available
    static juce::File GetiCloudSmartGridOneDirectory();
    
    static void PersistConfig(JSON config);
    static JSON LoadConfig();
    static void PersistJSON(JSON json, juce::String filename);
    static JSON LoadJSON(juce::String filename);

    void ChooseSaveFile(bool saveAs, std::function<void(juce::String)> onFileSelected = nullptr);
    void PickRecordingDirectory();
    void ChooseLoadFile(std::function<void(juce::String)> onFileSelected = nullptr);
    void SetDefaultRecordingDirectory();
    void SavePatch(JSON json);
    void LoadPatch(juce::String filename);

    void LoadCurrentPatch()
    {
        LoadPatch(m_currentPatchFilename);
    }

    JSON ToJSON();
    void FromJSON(JSON json);
    
    void SetCurrentPatchFilename(juce::String filename) { m_currentPatchFilename = filename; }
    juce::String GetCurrentPatchFilename() const { return m_currentPatchFilename; }

    MainComponent* m_mainComponent;
    juce::String m_currentPatchFilename;
};