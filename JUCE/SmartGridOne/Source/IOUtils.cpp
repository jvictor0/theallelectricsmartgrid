#include "IOUtils.hpp"
#include "MainComponent.h"

#if JUCE_IOS || JUCE_MAC
// Forward declaration of Objective-C++ helper function
extern juce::String GetiCloudDocumentsPath();
#endif

//==============================================================================
juce::File FileManager::GetSmartGridOneDirectory()
{
    // Use iCloud container for saving (same location on both macOS and iOS)
    // This ensures files sync between devices
#if JUCE_IOS || JUCE_MAC
    juce::String iCloudPath = GetiCloudDocumentsPath();
    
    if (iCloudPath.isNotEmpty())
    {
        juce::File documentsDir = juce::File(iCloudPath);
        if (documentsDir.exists())
        {
            juce::File smartGridDir = documentsDir.getChildFile("SmartGridOne");
            smartGridDir.createDirectory();
            juce::Logger::writeToLog("GetSmartGridOneDirectory (save): " + smartGridDir.getFullPathName());
            return smartGridDir;
        }
    }
    
    // Fallback to app's Documents directory if iCloud not available
    juce::Logger::writeToLog("GetSmartGridOneDirectory: iCloud not available, using fallback");
#endif
    
    juce::File smartGridDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SmartGridOne");
    smartGridDir.createDirectory();
    juce::Logger::writeToLog("GetSmartGridOneDirectory (save, fallback): " + smartGridDir.getFullPathName());
    
    return smartGridDir;
}

//==============================================================================
juce::File FileManager::GetiCloudSmartGridOneDirectory()
{
    // Try to get iCloud Drive Documents folder for READING
    // On iOS: May not be accessible without iCloud capability
    juce::File iCloudDir;
    
#if JUCE_IOS || JUCE_MAC
    juce::String iCloudPath = GetiCloudDocumentsPath();
    
    if (iCloudPath.isNotEmpty())
    {
        juce::File documentsDir = juce::File(iCloudPath);
        // Check if the Documents directory exists (parent of SmartGridOne)
        if (documentsDir.exists())
        {
            iCloudDir = documentsDir.getChildFile("SmartGridOne");
            // Create the SmartGridOne subdirectory if it doesn't exist (for reading, this is safe)
            // Even if empty, it allows findChildFiles to work
            if (!iCloudDir.exists())
            {
                iCloudDir.createDirectory();
            }
            juce::Logger::writeToLog("GetiCloudSmartGridOneDirectory (read): " + iCloudDir.getFullPathName());
            return iCloudDir;
        }
    }
    
    juce::Logger::writeToLog("GetiCloudSmartGridOneDirectory: iCloud path not available for reading");
#endif
    
    // Return empty/invalid file if not available
    return juce::File();
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

void FileManager::PersistJSON(JSON json, juce::String filename)
{
    if (filename.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: PersistJSON called with empty filename");
        return;
    }

    juce::File file(filename);
    
    // Always save to app's Documents directory (writable location)
    // Extract just the filename and save to app Documents, even if path points to iCloud
    juce::String fileName = file.getFileName();
    juce::File saveDir = GetSmartGridOneDirectory();
    juce::File saveFile = saveDir.getChildFile(fileName);
    
    juce::Logger::writeToLog("PersistJSON: Redirecting save to app Documents: " + saveFile.getFullPathName());
    
    juce::File parentDir = saveFile.getParentDirectory();
    
    // Create parent directory if it doesn't exist
    if (!parentDir.exists())
    {
        juce::Result result = parentDir.createDirectory();
        if (result.failed())
        {
            juce::Logger::writeToLog("ERROR: Failed to create directory: " + parentDir.getFullPathName() + " - " + result.getErrorMessage());
            return;
        }
    }
    
    file = saveFile; // Use the redirected file path

    char* jsonStr = json.Dumps(JSON_ENCODE_ANY);
    juce::String jsonString(jsonStr);
    free(jsonStr);

    juce::Logger::writeToLog("Saving JSON: " + file.getFullPathName() + " (" + std::to_string(jsonString.length()) + " bytes)");
   
    // Delete existing file if it's 0 bytes (might be a placeholder from file chooser)
    if (file.exists() && file.getSize() == 0)
    {
        juce::Logger::writeToLog("Deleting existing 0-byte file");
        file.deleteFile();
    }
   
    bool success = file.replaceWithText(jsonString);
    if (!success)
    {
        juce::Logger::writeToLog("ERROR: replaceWithText returned false for: " + filename);
    }
    else
    {
        // Verify the file was written
        if (file.exists())
        {
            int64_t fileSize = file.getSize();
            juce::Logger::writeToLog("File saved successfully. Size: " + juce::String(fileSize) + " bytes");
            if (fileSize == 0)
            {
                juce::Logger::writeToLog("WARNING: File is 0 bytes after save!");
            }
        }
        else
        {
            juce::Logger::writeToLog("WARNING: File does not exist after save!");
        }
    }
}

void FileManager::SavePatch(JSON json)
{
    if (m_currentPatchFilename.isEmpty())
    {
        juce::Logger::writeToLog("WARNING: SavePatch called but no filename set, using ChooseSaveFile");
        ChooseSaveFile(false);
        return;
    }
    
    PersistJSON(json, m_currentPatchFilename);
}

JSON FileManager::LoadJSON(juce::String filename)
{
    if (filename.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: LoadJSON called with empty filename");
        return JSON::Null();
    }

    juce::File file(filename);
    
    // If file doesn't exist at the given path, try to find it in iCloud location
    if (!file.exists())
    {
        // Extract just the filename
        juce::String fileName = file.getFileName();
        
        // Try iCloud location first (for reading files created on Mac)
        juce::File iCloudDir = GetiCloudSmartGridOneDirectory();
        if (iCloudDir.exists())
        {
            juce::File iCloudFile = iCloudDir.getChildFile(fileName);
            if (iCloudFile.exists())
            {
                juce::Logger::writeToLog("LoadJSON: Found file in iCloud location: " + iCloudFile.getFullPathName());
                file = iCloudFile;
            }
        }
        
        // If still not found, try app Documents location
        if (!file.exists())
        {
            juce::File appDir = GetSmartGridOneDirectory();
            juce::File appFile = appDir.getChildFile(fileName);
            if (appFile.exists())
            {
                juce::Logger::writeToLog("LoadJSON: Found file in app Documents: " + appFile.getFullPathName());
                file = appFile;
            }
        }
    }
    
    if (!file.exists())
    {
        juce::Logger::writeToLog("ERROR: File does not exist: " + filename);
        return JSON::Null();
    }

    juce::String jsonString = file.loadFileAsString();
    if (jsonString.isEmpty())
    {
        juce::Logger::writeToLog("ERROR: File is empty or could not be read: " + filename);
        return JSON::Null();
    }

    juce::Logger::writeToLog("Loading JSON: " + file.getFullPathName() + " (" + std::to_string(jsonString.length()) + " bytes)");

    return JSON::Loads(jsonString.toUTF8().getAddress(), 0, nullptr);
}

void FileManager::ChooseSaveFile(bool saveAs, std::function<void(juce::String)> onFileSelected)
{
    // Always use custom patch chooser now - no OS file chooser
    // The callback is handled in MainComponent
    if (!saveAs && !m_currentPatchFilename.isEmpty())
    {
        // Just save to existing file
        m_mainComponent->RequestSave();
    }
    // Otherwise MainComponent will show the patch chooser
}

void FileManager::PickRecordingDirectory()
{
    // For now, just use the default recording directory
    // Can be enhanced later with a custom directory chooser if needed
    SetDefaultRecordingDirectory();
}

void FileManager::SetDefaultRecordingDirectory()
{
    juce::File smartGridDir = GetSmartGridOneDirectory();
    
    juce::File recordingsDir = smartGridDir.getChildFile("Recordings");
    juce::Result recordingsResult = recordingsDir.createDirectory();
    if (recordingsResult.failed() && !recordingsDir.exists())
    {
        juce::Logger::writeToLog("ERROR: Failed to create Recordings directory: " + recordingsResult.getErrorMessage());
    }
    
    m_mainComponent->SetRecordingDirectory(recordingsDir.getFullPathName().toUTF8().getAddress());
}

void FileManager::ChooseLoadFile(std::function<void(juce::String)> onFileSelected)
{
    // Trigger callback to show custom patch chooser
    // This will be handled in MainComponent
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
