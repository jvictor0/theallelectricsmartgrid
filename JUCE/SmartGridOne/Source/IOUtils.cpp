#include "IOUtils.hpp"
#include "MainComponent.h"

//==============================================================================
void FileManager::PersistConfig(JSON config)
{
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SmartGridOne");
    configDir.createDirectory(); 

    juce::File configFile = configDir.getChildFile("config.json");

    configFile.create();

    juce::Logger::writeToLog("Saving Config: " + configFile.getFullPathName());

    char* configStr = config.Dumps(JSON_ENCODE_ANY);
    juce::String configString(configStr);
    free(configStr);
    configFile.replaceWithText(configString);
}    

JSON FileManager::LoadConfig()
{
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SmartGridOne");
    configDir.createDirectory(); 

    juce::File configFile = configDir.getChildFile("config.json");

    if (!configFile.exists())
    {
        return JSON::Null();
    }

    juce::Logger::writeToLog("Loading Config: " + configFile.getFullPathName());

    juce::String configString = configFile.loadFileAsString();
    return JSON::Loads(configString.toUTF8().getAddress(), 0, nullptr);
}

void FileManager::PersistJSON(JSON json, juce::String filename)
{
    juce::File file(filename);
    char* jsonStr = json.Dumps(JSON_ENCODE_ANY);
    juce::String jsonString(jsonStr);
    free(jsonStr);
    file.replaceWithText(jsonString);
}

void FileManager::SavePatch(JSON json)
{
    INFO("Saving Patch: %s", m_currentPatchFilename.toUTF8().getAddress());
    PersistJSON(json, m_currentPatchFilename);
}

JSON FileManager::LoadJSON(juce::String filename)
{
    juce::File file(filename);
    juce::String jsonString = file.loadFileAsString();
    return JSON::Loads(jsonString.toUTF8().getAddress(), 0, nullptr);
}

void FileManager::ChooseSaveFile(bool saveAs)
{
    if (m_fileChooser.get())
    {
        return;
    }

    if (m_currentPatchFilename.isEmpty() || saveAs)
    {
        juce::File smartGridDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SmartGridOne");
        smartGridDir.createDirectory();
        
        m_fileChooser = std::make_unique<juce::FileChooser>("Save Patch", smartGridDir, "*.json");
        int flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
        
        m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
        {
            m_currentPatchFilename = chooser.getResult().getFullPathName();
            if (!m_currentPatchFilename.isEmpty())
            {
                juce::Logger::writeToLog("Save Patch As: " + m_currentPatchFilename);
                m_mainComponent->RequestSave();
            }
            
            m_fileChooser.reset(); // Release the FileChooser
            m_mainComponent->SaveConfig();
        });
    }
    else
    {
        m_mainComponent->RequestSave();
    }
}

void FileManager::ChooseLoadFile()
{
    if (m_fileChooser.get())
    {
        return;
    }

    juce::File smartGridDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SmartGridOne");
    smartGridDir.createDirectory();
    
    m_fileChooser = std::make_unique<juce::FileChooser>("Load Patch", smartGridDir, "*.json");
    int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    
    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        juce::String filename = chooser.getResult().getFullPathName();
        if (!filename.isEmpty())
        {
            juce::Logger::writeToLog("Load Patch: " + filename);
            LoadPatch(filename);
        }
        
        m_fileChooser.reset(); // Release the FileChooser
        m_mainComponent->SaveConfig();
    });
}

void FileManager::LoadPatch(juce::String filename)
{
    juce::Logger::writeToLog("Load Patch: " + filename);
    JSON patch = LoadJSON(filename);
    if (!patch.IsNull())
    {   
        m_currentPatchFilename = filename;
        m_mainComponent->RequestLoad(patch);
    }
}

JSON FileManager::ToJSON()
{
    JSON json = JSON::Object();
    json.SetNew("filename", JSON::String(m_currentPatchFilename.toUTF8().getAddress()));
    return json;
}

void FileManager::FromJSON(JSON json)
{
    JSON filenameJ = json.Get("filename");
    if (!filenameJ.IsNull())
    {
        m_currentPatchFilename = juce::String(filenameJ.StringValue());
    }
    else
    {
        m_currentPatchFilename = "";
    }
}
