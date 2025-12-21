#import <Foundation/Foundation.h>
#include <JuceHeader.h>

juce::String GetiCloudDocumentsPath()
{
#if JUCE_IOS || JUCE_MAC
    NSFileManager* fileManager = [NSFileManager defaultManager];
    
#if JUCE_IOS
    // On iOS, without iCloud capability, we can't access the system iCloud Drive Documents folder.
    // However, we can try to use the app's bundle identifier to access a shared container.
    // But actually, the real issue is that the Files app shows "iCloud Drive/Documents" which
    // is the system-level folder that syncs from Mac's Desktop and Documents.
    //
    // Without special entitlements, apps can't write there. However, the app's Documents folder
    // WILL sync to iCloud if the user has iCloud Drive enabled, it just appears in a different
    // location in Files app (under "On My iPhone" → app name).
    //
    // Since the user wants to use the same location as Mac (iCloud Drive/Documents), and we
    // can't access that without entitlements, we need to accept that limitation OR use a
    // workaround: try to access the shared container using the bundle ID.
    
    // Try with explicit bundle identifier (might work even without capability if iCloud Drive is enabled)
    NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
    if (bundleID != nil)
    {
        NSString* containerID = [@"iCloud." stringByAppendingString:bundleID];
        NSURL* iCloudURL = [fileManager URLForUbiquityContainerIdentifier:containerID];
        
        if (iCloudURL != nil)
        {
            NSURL* documentsURL = [iCloudURL URLByAppendingPathComponent:@"Documents"];
            NSString* pathString = [documentsURL path];
            BOOL isDirectory = NO;
            if ([fileManager fileExistsAtPath:pathString isDirectory:&isDirectory] && isDirectory)
            {
                BOOL isWritable = [fileManager isWritableFileAtPath:pathString];
                juce::Logger::writeToLog("iCloudHelper: Found container with bundle ID: " + juce::String([pathString UTF8String]) + " (writable: " + juce::String(isWritable ? "YES" : "NO") + ")");
                if (isWritable)
                {
                    return juce::String([pathString UTF8String]);
                }
            }
        }
        
        juce::Logger::writeToLog("iCloudHelper: Container with bundle ID not available: " + juce::String([containerID UTF8String]));
    }
    
    // Try nil identifier (default container - requires capability)
    NSURL* iCloudURL = [fileManager URLForUbiquityContainerIdentifier:nil];
    if (iCloudURL != nil)
    {
        NSURL* documentsURL = [iCloudURL URLByAppendingPathComponent:@"Documents"];
        NSString* pathString = [documentsURL path];
        BOOL isDirectory = NO;
        if ([fileManager fileExistsAtPath:pathString isDirectory:&isDirectory] && isDirectory)
        {
            BOOL isWritable = [fileManager isWritableFileAtPath:pathString];
            juce::Logger::writeToLog("iCloudHelper: Found default ubiquity container: " + juce::String([pathString UTF8String]) + " (writable: " + juce::String(isWritable ? "YES" : "NO") + ")");
            if (isWritable)
            {
                return juce::String([pathString UTF8String]);
            }
        }
    }
    
    // Try one more thing: access Mobile Documents directly (might work for reading)
    // This is the system path where iCloud Drive stores files visible in Files app
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    if ([paths count] > 0)
    {
        NSString* libraryPath = [paths objectAtIndex:0];
        NSString* mobileDocsPath = [[libraryPath stringByAppendingPathComponent:@"Mobile Documents"] stringByAppendingPathComponent:@"com~apple~CloudDocs/Documents"];
        
        BOOL isDirectory = NO;
        BOOL exists = [fileManager fileExistsAtPath:mobileDocsPath isDirectory:&isDirectory];
        juce::Logger::writeToLog("iCloudHelper: Trying system Mobile Documents: " + juce::String([mobileDocsPath UTF8String]) + " (exists: " + juce::String(exists ? "YES" : "NO") + ")");
        
        if (exists && isDirectory)
        {
            // Even if not writable, we can try to read from it
            juce::Logger::writeToLog("iCloudHelper: Found system Mobile Documents path (read-only access)");
            return juce::String([mobileDocsPath UTF8String]);
        }
    }
    
    juce::Logger::writeToLog("iCloudHelper: No accessible iCloud path found - iOS apps need iCloud capability to access iCloud Drive/Documents");
#else
    // On macOS, try ubiquity container first
    NSURL* iCloudURL = [fileManager URLForUbiquityContainerIdentifier:nil];
    if (iCloudURL != nil)
    {
        NSURL* documentsURL = [iCloudURL URLByAppendingPathComponent:@"Documents"];
        NSString* pathString = [documentsURL path];
        return juce::String([pathString UTF8String]);
    }
#endif
    
    // Return empty string if iCloud is not available
    return juce::String();
#else
    // Stub for non-Apple platforms
    return juce::String();
#endif
}

