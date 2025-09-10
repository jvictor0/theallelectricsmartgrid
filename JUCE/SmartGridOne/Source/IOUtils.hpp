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

    void SavePatch(JSON json);
    void SavePatchAs(JSON json);
    JSON LoadPatch(juce::String filename);
    JSON LoadCurrentPatch();
    JSON ToJSON();
    void FromJSON(JSON json);

    MainComponent* m_mainComponent;
    juce::String m_currentPatchFilename;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
};