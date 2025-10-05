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

    static void PersistConfig(JSON config);
    static JSON LoadConfig();
    static void PersistJSON(JSON json, juce::String filename);
    static JSON LoadJSON(juce::String filename);

    void ChooseSaveFile(bool saveAs);
    void PickRecordingDirectory();
    void ChooseLoadFile();
    void SetDefaultRecordingDirectory();
    void SavePatch(JSON json);
    void LoadPatch(juce::String filename);

    void LoadCurrentPatch()
    {
        LoadPatch(m_currentPatchFilename);
    }

    JSON ToJSON();
    void FromJSON(JSON json);

    MainComponent* m_mainComponent;
    juce::String m_currentPatchFilename;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
};