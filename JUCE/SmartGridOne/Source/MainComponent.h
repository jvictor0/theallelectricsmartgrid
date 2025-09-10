#pragma once

#include <JuceHeader.h>

#include "NonagonWrapper.hpp"
#include "ConfigPage.hpp"
#include "IOUtils.hpp"
#include "WrldBuildrComponent.hpp"

class MainComponentMenuBarModel;

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent, public juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    virtual void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
    }

    virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        m_nonagon.Process(bufferToFill);
    }

    virtual void releaseResources() override
    {
    }

    void SaveConfig()
    {
        JSON config = JSON::Object();
        config.SetNew("nonagon_config", m_nonagon.ConfigToJSON());
        config.SetNew("file_config", m_fileManager.ToJSON());
        FileManager::PersistConfig(config);
    }

    void LoadConfig()
    {
        JSON config = FileManager::LoadConfig();
        if (!config.IsNull())
        {
            JSON nonagonConfig = config.Get("nonagon_config");
            if (!nonagonConfig.IsNull())
            {
                m_nonagon.ConfigFromJSON(nonagonConfig);
            }

            JSON fileConfig = config.Get("file_config");
            if (!fileConfig.IsNull())
            {
                m_fileManager.FromJSON(fileConfig);
            }

            LoadCurrentPatch();
        }
    }

    void SavePatch()
    {
        JSON json = m_nonagon.ToJSON();
        m_fileManager.SavePatch(json);
    }

    void SavePatchAs()
    {
        JSON json = m_nonagon.ToJSON();
        m_fileManager.SavePatchAs(json);
    }

    void LoadPatch(juce::String filename)
    {
        JSON patch = m_fileManager.LoadPatch(filename);
        if (!patch.IsNull())
        {
            m_nonagon.FromJSON(patch);
        }
    }

    void LoadCurrentPatch()
    {
        JSON patch = m_fileManager.LoadCurrentPatch();
        if (!patch.IsNull())
        {
            m_nonagon.FromJSON(patch);
        }
    }

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    void OnConfigButtonClicked();
    void OnBackButtonClicked();

    NonagonWrapper m_nonagon;

    std::unique_ptr<ConfigPage> m_configPage;
    std::unique_ptr<WrldBuildrComponent> m_wrldBuildrGrid;
    juce::TextButton m_configButton;
    juce::TextButton m_backButton;
    bool m_showingConfig;

    FileManager m_fileManager{this};

    std::unique_ptr<MainComponentMenuBarModel> m_menuBarModel;
    juce::MenuBarComponent m_menuBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
