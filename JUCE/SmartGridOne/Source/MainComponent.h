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
        json_t* config = json_object();
        json_object_set_new(config, "nonagon_config", m_nonagon.ConfigToJSON());
        json_object_set_new(config, "file_config", m_fileManager.ToJSON());
        FileManager::PersistConfig(config);
        json_decref(config);
    }

    void LoadConfig()
    {
        json_t* config = FileManager::LoadConfig();
        if (config)
        {
            json_t* nonagonConfig = json_object_get(config, "nonagon_config");
            if (nonagonConfig)
            {
                m_nonagon.ConfigFromJSON(nonagonConfig);
            }

            json_t* fileConfig = json_object_get(config, "file_config");
            if (fileConfig)
            {
                m_fileManager.FromJSON(fileConfig);
            }

            json_decref(config);

            LoadCurrentPatch();
        }
    }

    void SavePatch()
    {
        json_t* json = m_nonagon.ToJSON();
        m_fileManager.SavePatch(json);
        json_decref(json);
    }

    void SavePatchAs()
    {
        json_t* json = m_nonagon.ToJSON();
        m_fileManager.SavePatchAs(json);
        json_decref(json);
    }

    void LoadPatch(juce::String filename)
    {
        json_t* patch = m_fileManager.LoadPatch(filename);
        if (patch)
        {
            m_nonagon.FromJSON(patch);
        }

        json_decref(patch);
    }

    void LoadCurrentPatch()
    {
        json_t* patch = m_fileManager.LoadCurrentPatch();
        if (patch)
        {
            m_nonagon.FromJSON(patch);
        }

        json_decref(patch);
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
