#include "IOUtils.hpp"
#include "MainComponent.h"

//==============================================================================
void FileManager::PersistConfig(json_t* config)
{
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SmartGridOne");
    configDir.createDirectory(); 

    juce::File configFile = configDir.getChildFile("config.json");

    configFile.create();

    juce::Logger::writeToLog("Saving Config: " + configFile.getFullPathName());

    char* configStr = json_dumps(config, JSON_ENCODE_ANY);

    configFile.replaceWithText(configStr);

    free(static_cast<void*>(configStr));
}    

json_t* FileManager::LoadConfig()
{
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SmartGridOne");
    configDir.createDirectory(); 

    juce::File configFile = configDir.getChildFile("config.json");

    if (!configFile.exists())
    {
        return nullptr;
    }

    juce::Logger::writeToLog("Loading Config: " + configFile.getFullPathName());

    juce::String configString = configFile.loadFileAsString();
    json_t* config = json_loads(configString.toUTF8().getAddress(), 0, nullptr);

    return config;
}

void FileManager::PersistJSON(json_t* json, juce::String filename)
{
    juce::File file(filename);
    char* jsonStr = json_dumps(json, JSON_ENCODE_ANY);
    file.replaceWithText(jsonStr);
    free(static_cast<void*>(jsonStr));
}

json_t* FileManager::LoadJSON(juce::String filename)
{
    juce::File file(filename);
    juce::String jsonString = file.loadFileAsString();
    return json_loads(jsonString.toUTF8().getAddress(), 0, nullptr);
}

void FileManager::SavePatch(json_t* json)
{
    if (m_fileChooser.get())
    {
        return;
    }

    if (m_currentPatchFilename.isEmpty())
    {
        juce::File smartGridDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SmartGridOne");
        smartGridDir.createDirectory();
        
        m_fileChooser = std::make_unique<juce::FileChooser>("Save Patch", smartGridDir, "*.json");
        int flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
        
        json_incref(json);
        m_fileChooser->launchAsync(flags, [this, json](const juce::FileChooser& chooser)
        {
            m_currentPatchFilename = chooser.getResult().getFullPathName();
            if (!m_currentPatchFilename.isEmpty())
            {
                juce::Logger::writeToLog("Save Patch As: " + m_currentPatchFilename);
                PersistJSON(json, m_currentPatchFilename);
            }
            
            json_decref(json);
            m_fileChooser.reset(); // Release the FileChooser
            m_mainComponent->SaveConfig();
        });
    }
    else
    {
        juce::Logger::writeToLog("Save Patch: " + m_currentPatchFilename);
        PersistJSON(json, m_currentPatchFilename);
    }
}

void FileManager::SavePatchAs(json_t* json)
{
    m_currentPatchFilename = "";
    SavePatch(json);
}

json_t* FileManager::LoadPatch(juce::String filename)
{
    juce::Logger::writeToLog("Load Patch: " + filename);
    json_t* json = LoadJSON(filename);
    m_currentPatchFilename = filename;
    return json;
}

json_t* FileManager::LoadCurrentPatch()
{
    if (m_currentPatchFilename.isEmpty())
    {
        return nullptr;
    }
    
    juce::Logger::writeToLog("Load Current Patch: " + m_currentPatchFilename);
    return LoadPatch(m_currentPatchFilename);
}

json_t* FileManager::ToJSON()
{
    json_t* json = json_object();
    json_object_set_new(json, "filename", json_string(m_currentPatchFilename.toUTF8().getAddress()));
    return json;
}

void FileManager::FromJSON(json_t* json)
{
    json_t* filenameJ = json_object_get(json, "filename");
    if (filenameJ)
    {
        m_currentPatchFilename = json_string_value(filenameJ);
    }
    else
    {
        m_currentPatchFilename = "";
    }
}
