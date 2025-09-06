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

    static void PersistConfig(json_t* config);
    static json_t* LoadConfig();
    static void PersistJSON(json_t* json, juce::String filename);
    static json_t* LoadJSON(juce::String filename);

    void SavePatch(json_t* json);
    void SavePatchAs(json_t* json);
    json_t* LoadPatch(juce::String filename);
    json_t* LoadCurrentPatch();
    json_t* ToJSON();
    void FromJSON(json_t* json);

    MainComponent* m_mainComponent;
    juce::String m_currentPatchFilename;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
};