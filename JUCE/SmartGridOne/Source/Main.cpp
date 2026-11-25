/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainComponent.h"
#include "ThreadId.hpp"
#include "GeneratedNaiveReuppingLadder.hpp"
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>

static bool LoadNaiveLadderResponse()
{
    static_assert(SampleTimer::x_sampleRate == NaiveLadderResponse::x_engineRate);
    auto url = CFBundleCopyResourceURL(CFBundleGetMainBundle(),
        CFSTR("NaiveReuppingLadderResponse"), CFSTR("bin"), nullptr);
    if (url == nullptr)
    {
        return false;
    }

    UInt8 path[4096];
    const bool havePath = CFURLGetFileSystemRepresentation(url, true, path, sizeof(path));
    CFRelease(url);
    if (!havePath)
    {
        return false;
    }

    static const juce::MemoryMappedFile file(juce::File(juce::String::fromUTF8(
        reinterpret_cast<const char*>(path))), juce::MemoryMappedFile::readOnly);
    if (file.getData() == nullptr || file.getSize() != NaiveLadderResponse::x_bytes)
    {
        return false;
    }

    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(file.getData(), static_cast<CC_LONG>(file.getSize()), digest);
    if (juce::String::toHexString(digest, sizeof(digest), 0) != NaiveLadderResponse::x_sha256)
    {
        return false;
    }

    NaiveLadderResponse::s_table.emplace(file.getData(), file.getSize());
    return true;
}

//==============================================================================
class SmartGridOneApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    SmartGridOneApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        SetCurrentThreadId(ThreadId::Message);

        if (!LoadNaiveLadderResponse())
        {
            juce::Logger::writeToLog("Naive Reupping Ladder response resource is unavailable or failed validation.");
        }

        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (SmartGridOneApplication)
