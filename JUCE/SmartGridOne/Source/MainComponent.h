#pragma once

#include <JuceHeader.h>

#include "NonagonWrapper.hpp"
#include "ConfigPage.hpp"
#include "FilePage.hpp"
#include "IOUtils.hpp"
#include "WrldBuildrComponent.hpp"

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

            m_fileManager.LoadCurrentPatch();
        }
    }

    void HandleStateInterchange()
    {
        StateInterchange* stateInterchange = m_nonagon.GetStateInterchange();
        if (stateInterchange->IsSavePending())
        {
            INFO("Saving patch to file");
            JSON toSave = stateInterchange->GetToSave();
            m_fileManager.SavePatch(toSave);
            stateInterchange->AckSaveCompleted();
        }
    }

    void RequestSave()
    {
        m_nonagon.GetStateInterchange()->RequestSave();
    }

    void RequestLoad(JSON patch)
    {
        m_nonagon.GetStateInterchange()->RequestLoad(patch);
    }

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    void OnConfigButtonClicked();
    void OnBackButtonClicked();
    void OnFileButtonClicked();
    void OnFileBackButtonClicked();

    NonagonWrapper m_nonagon;

    std::unique_ptr<ConfigPage> m_configPage;
    std::unique_ptr<FilePage> m_filePage;
    std::unique_ptr<WrldBuildrComponent> m_wrldBuildrGrid;
    juce::TextButton m_configButton;
    juce::TextButton m_backButton;
    juce::TextButton m_fileButton;
    bool m_showingConfig;
    bool m_showingFile;

    FileManager m_fileManager{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
