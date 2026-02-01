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

    // Get the base SmartGridOne directory (~/Documents/SmartGridOne on Mac, app Documents on iOS)
    //
    static juce::File GetSmartGridOneDirectory();
    
    // Get the patches directory (SmartGridOne/patches/)
    //
    static juce::File GetPatchesDirectory();
    
    // Get the recordings directory (SmartGridOne/recordings/)
    //
    static juce::File GetRecordingsDirectory();
    
    // Get a specific patch directory by name (patches/<patchName>/)
    //
    static juce::File GetPatchDirectory(const juce::String& patchName);
    
    // Get the latest (alphanumerically highest) JSON file from a patch directory
    //
    static juce::File GetLatestPatchFile(const juce::File& patchDir);
    
    // Generate a timestamp filename for saving
    //
    static juce::String GenerateTimestampFilename();
    
    static void PersistConfig(JSON config);
    static JSON LoadConfig();

    void ChooseSaveFile(bool saveAs, std::function<void(juce::String)> onFileSelected = nullptr);
    void ChooseLoadFile(std::function<void(juce::String)> onFileSelected = nullptr);
    void SetDefaultRecordingDirectory();
    void SavePatch(JSON json);
    void LoadPatch(juce::String patchName);
    void LoadPatchVersion(juce::String versionFilePath);

    void LoadCurrentPatch()
    {
        LoadPatch(m_currentPatchName);
    }

    JSON ToJSON();
    void FromJSON(JSON json);
    
    void SetCurrentPatchName(juce::String patchName) { m_currentPatchName = patchName; }
    juce::String GetCurrentPatchName() const { return m_currentPatchName; }

    MainComponent* m_mainComponent;
    juce::String m_currentPatchName;
};
