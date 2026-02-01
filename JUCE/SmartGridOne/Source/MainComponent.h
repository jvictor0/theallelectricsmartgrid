#pragma once

#include <JuceHeader.h>

#include "NonagonWrapper.hpp"
#include "ConfigPage.hpp"
#include "FilePage.hpp"
#include "PatchChooser.hpp"
#include "IOUtils.hpp"
#include "WrldBuildrComponent.hpp"
#include "Configuration.hpp"

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
        INFO("prepareToPlay: %d samples @ %.0f Hz (%.2f ms)", samplesPerBlockExpected, sampleRate, samplesPerBlockExpected * 1000.0 / sampleRate);
        SampleTimer::Init(samplesPerBlockExpected);
        m_nonagon.PrepareToPlay(samplesPerBlockExpected, sampleRate);
        m_sampleRate = sampleRate;
    }

    virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto start = juce::Time::getHighResolutionTicks();
        
        m_configuration.m_forceStereo = bufferToFill.buffer->getNumChannels() < 4;
        m_configuration.m_stereo = m_configuration.m_stereo || m_configuration.m_forceStereo;
        m_nonagon.Process(bufferToFill, MakeIOInfo());

        auto end = juce::Time::getHighResolutionTicks();
        auto duration = juce::Time::highResolutionTicksToSeconds(end - start);
        if (static_cast<double>(bufferToFill.numSamples) / m_sampleRate < duration)
        {
            INFO("Audio xrun %f ms / %f ms (samples = %d)", duration * 1000, static_cast<double>(bufferToFill.numSamples * 1000) / m_sampleRate, bufferToFill.numSamples);
        }
    }

    NonagonWrapper::IOInfo MakeIOInfo()
    {
        NonagonWrapper::IOInfo ioInfo;
        auto* device = deviceManager.getCurrentAudioDevice();
        int numInputs  = device ? device->getActiveInputChannels().countNumberOfSetBits() : 0;
        int numOutputs = device ? device->getActiveOutputChannels().countNumberOfSetBits() : 0;
        
        ioInfo.m_numInputs = numInputs;
        ioInfo.m_numOutputs = numOutputs;
        ioInfo.m_numChannels = std::min(numOutputs, m_configuration.m_stereo ? 2 : 4);
        ioInfo.m_stereo = m_configuration.m_stereo;
        return ioInfo;
    }

    virtual void releaseResources() override
    {
    }

    void SetRecordingDirectory(const char* directory)
    {
        INFO("Setting recording directory to: %s", directory);
        m_nonagon.SetRecordingDirectory(directory);
    }

    void SaveConfig()
    {
        JSON config = JSON::Object();
        JSON nonagonConfig = m_nonagon.ConfigToJSON();
        nonagonConfig.SetNew("stereo", JSON::Boolean(m_configuration.m_stereo));
        config.SetNew("nonagon_config", nonagonConfig);
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
                JSON stereoJ = nonagonConfig.Get("stereo");
                if (!stereoJ.IsNull())
                {
                    m_configuration.m_stereo = stereoJ.BooleanValue();
                }
             
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
    void ShowPatchChooser(bool isSaveMode);
    void ShowVersionChooser();

    NonagonWrapper m_nonagon;
    Configuration m_configuration;

    std::unique_ptr<ConfigPage> m_configPage;
    std::unique_ptr<FilePage> m_filePage;
    std::unique_ptr<PatchChooser> m_patchChooser;
    std::unique_ptr<VersionChooser> m_versionChooser;
    std::unique_ptr<WrldBuildrComponent> m_wrldBuildrGrid;
    juce::TextButton m_configButton;
    juce::TextButton m_backButton;
    juce::TextButton m_fileButton;
    juce::Label m_cpuLabel;
    RollingBuffer<256> m_cpuUsageBuffer;
    
    bool m_showingConfig;
    bool m_showingFile;

    double m_sampleRate;

    FileManager m_fileManager{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
