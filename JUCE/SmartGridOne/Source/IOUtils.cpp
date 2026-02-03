#include "IOUtils.hpp"
#include "MainComponent.h"

//==============================================================================
juce::File FileManager::GetSmartGridOneDirectory()
{
    // Use app's Documents directory - works on both macOS and iOS
    // On iOS this is accessible via pymobiledevice3
    //
    juce::File smartGridDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SmartGridOne");
    smartGridDir.createDirectory();
    return smartGridDir;
}

//==============================================================================
juce::File FileManager::GetPatchesDirectory()
{
    juce::File patchesDir = GetSmartGridOneDirectory().getChildFile("patches");
    patchesDir.createDirectory();
    return patchesDir;
}

//==============================================================================
juce::File FileManager::GetRecordingsDirectory()
{
    juce::File recordingsDir = GetSmartGridOneDirectory().getChildFile("recordings");
    recordingsDir.createDirectory();
    return recordingsDir;
}

//==============================================================================
juce::File FileManager::GetPatchDirectory(const juce::String& patchName)
{
    if (patchName.isEmpty())
    {
        return juce::File();
    }

    juce::File patchDir = GetPatchesDirectory().getChildFile(patchName);
    return patchDir;
}

//==============================================================================
juce::File FileManager::GetLatestPatchFile(const juce::File& patchDir)
{
    if (!patchDir.exists() || !patchDir.isDirectory())
    {
        return juce::File();
    }

    juce::Array<juce::File> jsonFiles;
    patchDir.findChildFiles(jsonFiles, juce::File::findFiles, false, "*.json");
    
    if (jsonFiles.isEmpty())
    {
        return juce::File();
    }

    // Sort alphanumerically - highest (latest timestamp) will be last
    //
    jsonFiles.sort();
    
    return jsonFiles.getLast();
}

//==============================================================================
juce::String FileManager::GenerateTimestampFilename()
{
    // Generate ISO 8601 compatible timestamp with hyphens instead of colons for filesystem safety
    // Format: YYYY-MM-DDTHH-MM-SS.json
    //
    juce::Time now = juce::Time::getCurrentTime();
    return now.formatted("%Y-%m-%dT%H-%M-%S") + ".json";
}

//==============================================================================
void FileManager::PersistConfig(JSON config)
{
    juce::File configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SmartGridOne");
    juce::Result dirResult = configDir.createDirectory();
    if (dirResult.failed() && !configDir.exists())
    {
        juce::Logger::writeToLog("ERROR: Failed to create config directory: " + configDir.getFullPathName() + " - " + dirResult.getErrorMessage());
        return;
    }

    juce::File configFile = configDir.getChildFile("config.json");

    juce::Logger::writeToLog("Saving Config: " + configFile.getFullPathName());

    char* configStr = config.Dumps(JSON_ENCODE_ANY);
    juce::String configString(configStr);
    free(configStr);

    juce::Logger::writeToLog("Saving Config: " + configFile.getFullPathName() + " (" + std::to_string(configString.length()) + " bytes)");

    bool success = configFile.replaceWithText(configString);
    if (!success)
    {
        juce::Logger::writeToLog("ERROR: Failed to save config file: " + configFile.getFullPathName());
    }
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

    juce::String configString = configFile.loadFileAsString();

    juce::Logger::writeToLog("Loading Config: " + configFile.getFullPathName() + " (" + std::to_string(configString.length()) + " bytes)");

    return JSON::Loads(configString.toUTF8().getAddress(), 0, nullptr);
}

//==============================================================================
bool FileManager::CreateNewPatch(juce::String patchName)
{
    if (patchName.isEmpty())
    {
        juce::Logger::writeToLog("WARNING: CreateNewPatch called with empty patch name");
        return false;
    }

    juce::File patchDir = GetPatchDirectory(patchName);
    if (!patchDir.exists())
    {
        juce::Result result = patchDir.createDirectory();
        if (result.failed())
        {
            juce::Logger::writeToLog("ERROR: Failed to create patch directory: " + patchDir.getFullPathName() + " - " + result.getErrorMessage());
            return false;
        }
    }

    m_currentPatchName = patchName;
    juce::Logger::writeToLog("Created new patch: " + patchName);
    return true;
}

//==============================================================================
void FileManager::SavePatch(JSON json)
{
    if (m_currentPatchName.isEmpty())
    {
        juce::Logger::writeToLog("WARNING: SavePatch called but no patch name set");
        return;
    }
    
    // Get or create the patch directory
    //
    juce::File patchDir = GetPatchDirectory(m_currentPatchName);
    if (!patchDir.exists())
    {
        juce::Result result = patchDir.createDirectory();
        if (result.failed())
        {
            juce::Logger::writeToLog("ERROR: Failed to create patch directory: " + patchDir.getFullPathName() + " - " + result.getErrorMessage());
            return;
        }
    }
    
    // Generate timestamped filename
    //
    juce::String filename = GenerateTimestampFilename();
    juce::File patchFile = patchDir.getChildFile(filename);
    
    // Serialize JSON
    //
    char* jsonStr = json.Dumps(JSON_ENCODE_ANY);
    juce::String jsonString(jsonStr);
    free(jsonStr);

    juce::Logger::writeToLog("Saving patch: " + patchFile.getFullPathName() + " (" + std::to_string(jsonString.length()) + " bytes)");
   
    bool success = patchFile.replaceWithText(jsonString);
    if (!success)
    {
        juce::Logger::writeToLog("ERROR: Failed to save patch file: " + patchFile.getFullPathName());
    }
    else
    {
        if (patchFile.exists())
        {
            int64_t fileSize = patchFile.getSize();
            juce::Logger::writeToLog("Patch saved successfully. Size: " + juce::String(fileSize) + " bytes");
        }
        else
        {
            juce::Logger::writeToLog("WARNING: Patch file does not exist after save!");
        }
    }
}

//==============================================================================
void FileManager::LoadPatch(juce::String patchName)
{
    if (patchName.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: LoadPatch called with empty patch name");
        return;
    }

    juce::File patchDir = GetPatchDirectory(patchName);
    if (!patchDir.exists())
    {
        juce::Logger::writeToLog("ERROR: Patch directory does not exist: " + patchDir.getFullPathName());
        return;
    }

    juce::File latestFile = GetLatestPatchFile(patchDir);
    if (!latestFile.exists())
    {
        juce::Logger::writeToLog("ERROR: No patch files found in: " + patchDir.getFullPathName());
        return;
    }

    juce::String jsonString = latestFile.loadFileAsString();
    if (jsonString.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: Patch file is empty: " + latestFile.getFullPathName());
        return;
    }

    juce::Logger::writeToLog("Loading patch: " + latestFile.getFullPathName() + " (" + std::to_string(jsonString.length()) + " bytes)");

    JSON patch = JSON::Loads(jsonString.toUTF8().getAddress(), 0, nullptr);
    if (!patch.IsNull())
    {   
        m_currentPatchName = patchName;
        m_mainComponent->RequestLoad(patch);
    }
    else
    {
        juce::Logger::writeToLog("ERROR: Failed to parse patch JSON: " + latestFile.getFullPathName());
    }
}

//==============================================================================
void FileManager::LoadPatchVersion(juce::String versionFilePath)
{
    if (versionFilePath.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: LoadPatchVersion called with empty path");
        return;
    }

    juce::File versionFile(versionFilePath);
    if (!versionFile.exists())
    {
        juce::Logger::writeToLog("ERROR: Version file does not exist: " + versionFilePath);
        return;
    }

    juce::String jsonString = versionFile.loadFileAsString();
    if (jsonString.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: Version file is empty: " + versionFilePath);
        return;
    }

    juce::Logger::writeToLog("Loading patch version: " + versionFilePath + " (" + std::to_string(jsonString.length()) + " bytes)");

    JSON patch = JSON::Loads(jsonString.toUTF8().getAddress(), 0, nullptr);
    if (!patch.IsNull())
    {   
        m_mainComponent->RequestLoad(patch);
    }
    else
    {
        juce::Logger::writeToLog("ERROR: Failed to parse patch JSON: " + versionFilePath);
    }
}

//==============================================================================
void FileManager::ChooseSaveFile(bool saveAs, std::function<void(juce::String)> onFileSelected)
{
    // The callback is handled in MainComponent via PatchChooser
    //
    if (!saveAs && !m_currentPatchName.isEmpty())
    {
        // Just save to existing patch
        //
        m_mainComponent->RequestSave();
    }

    // Otherwise MainComponent will show the patch chooser
    //
}

//==============================================================================
void FileManager::SetDefaultRecordingDirectory()
{
    juce::File recordingsDir = GetRecordingsDirectory();
    m_mainComponent->SetRecordingDirectory(recordingsDir.getFullPathName().toUTF8().getAddress());
}

//==============================================================================
void FileManager::ChooseLoadFile(std::function<void(juce::String)> onFileSelected)
{
    // Trigger callback to show custom patch chooser
    // This will be handled in MainComponent
    //
}

//==============================================================================
JSON FileManager::ToJSON()
{
    JSON json = JSON::Object();
    json.SetNew("patchName", JSON::String(m_currentPatchName.toUTF8().getAddress()));
    return json;
}

//==============================================================================
void FileManager::FromJSON(JSON json)
{
    JSON patchNameJ = json.Get("patchName");
    if (!patchNameJ.IsNull())
    {
        m_currentPatchName = juce::String(patchNameJ.StringValue());
    }
    else
    {
        // Try legacy "filename" field for backwards compatibility
        //
        JSON filenameJ = json.Get("filename");
        if (!filenameJ.IsNull())
        {
            juce::String legacyFilename = juce::String(filenameJ.StringValue());
            // Extract patch name from legacy filename (remove path and .json extension)
            //
            juce::File legacyFile(legacyFilename);
            m_currentPatchName = legacyFile.getFileNameWithoutExtension();
        }
        else
        {
            m_currentPatchName = "";
        }
    }
}
