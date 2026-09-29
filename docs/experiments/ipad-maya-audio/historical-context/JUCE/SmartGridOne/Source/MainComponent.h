#pragma once

#include <JuceHeader.h>
#include <filesystem>

#include "NonagonWrapper.hpp"
#include "ConfigPage.hpp"
#include "FilePage.hpp"
#include "PatchChooser.hpp"
#include "IOUtils.hpp"
#include "WrldBuildrComponent.hpp"
#include "Configuration.hpp"
#include "ClockModeConfigJSON.hpp"
#include "ThreadId.hpp"
#include "AudioCallbackDiagnostics.hpp"
#include "AudioPlatformDiagnostics.hpp"
#include "RemoteIOProbeBridge.hpp"
#include <atomic>
#include <cmath>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component, public juce::AudioSource, public juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    const bool m_useTestTone = juce::SystemStats::getEnvironmentVariable("SMARTGRID_AUDIO_TEST_MODE", "normal") != "normal";
    const bool m_renderTestUi = juce::SystemStats::getEnvironmentVariable("SMARTGRID_UI_RENDER", "1") != "0";
    const bool m_startTestPlayback = juce::SystemStats::getEnvironmentVariable("SMARTGRID_AUDIO_TEST_PLAY", "1") == "1";
    static constexpr double x_testToneFrequency = 1000.0;
    static constexpr float x_testToneAmplitude = 0.05f;
    double m_testTonePhase = 0.0;

    virtual void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        INFO("prepareToPlay: %d samples @ %.0f Hz (%.2f ms)", samplesPerBlockExpected, sampleRate, samplesPerBlockExpected * 1000.0 / sampleRate);
        SampleTimer::Init(samplesPerBlockExpected);
        m_nonagon.PrepareToPlay(samplesPerBlockExpected, sampleRate);
        m_sampleRate = sampleRate;
        m_audioCallbackDiagnostics.Reset();
        m_testTonePhase = 0.0;
        if (m_useTestTone)
        {
            INFO("Audio test mode=tone frequency_hz=%.0f peak=%.6f channels=all; normal callback processing bypassed",
                x_testToneFrequency, x_testToneAmplitude);
        }
        else
        {
            INFO("Audio test mode=normal; normal callback processing enabled");
        }

        if (!AudioCallbackDiagnostics::CanRender(sampleRate))
        {
            INFO("Audio rate rejected: actual=%.0f required=%zu; DSP output muted", sampleRate, SampleTimer::x_sampleRate);
        }

    }

    virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        ScopedThreadId scopedThreadId(ThreadId::Audio);
        const auto start = juce::Time::getHighResolutionTicks();
        const auto startUs = static_cast<uint64_t>(juce::Time::highResolutionTicksToSeconds(start) * 1000000.0);
        const auto observation = m_audioCallbackDiagnostics.Observe(startUs, bufferToFill.numSamples, m_sampleRate);
#if JUCE_IOS
        SmartGridRemoteIOAppCallback(observation.m_sequence);
#endif
        const bool canRender = AudioCallbackDiagnostics::CanRender(m_sampleRate);

        if (canRender)
        {
            m_configuration.m_forceStereo = bufferToFill.buffer->getNumChannels() < 4;
            m_configuration.m_stereo = m_configuration.m_stereo || m_configuration.m_forceStereo;
            if (m_useTestTone)
            {
                // Temporary isolation test: bypass all normal callback processing.
                // Keep tone phase continuous across callbacks and fill every output.
                //
                const double phaseIncrement = juce::MathConstants<double>::twoPi * x_testToneFrequency / m_sampleRate;
                for (int i = 0; i < bufferToFill.numSamples; ++i)
                {
                    const float sample = x_testToneAmplitude * static_cast<float>(std::sin(m_testTonePhase));
                    for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
                    {
                        bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample)[i] = sample;
                    }

                    m_testTonePhase += phaseIncrement;
                    if (m_testTonePhase >= juce::MathConstants<double>::twoPi)
                    {
                        m_testTonePhase -= juce::MathConstants<double>::twoPi;
                    }
                }
            }
            else
            {
                m_nonagon.Process(bufferToFill, MakeIOInfo());
            }
        }
        else
        {
            bufferToFill.clearActiveBufferRegion();
        }

        const auto end = juce::Time::getHighResolutionTicks();
        const auto duration = juce::Time::highResolutionTicksToSeconds(end - start);
        const auto* device = m_deviceManager.getCurrentAudioDevice();
        const int xruns = device != nullptr ? device->getXRunCount() : -1;
        INFO("Audio cb=%llu t_us=%llu gap_us=%llu prev_budget_us=%llu dsp_us=%.0f n=%d sr=%.0f xr=%d thermal=%d lp=%d muted=%d",
            static_cast<unsigned long long>(observation.m_sequence),
            static_cast<unsigned long long>(startUs),
            static_cast<unsigned long long>(observation.m_gapUs),
            static_cast<unsigned long long>(observation.m_previousBudgetUs),
            duration * 1000000.0, bufferToFill.numSamples, m_sampleRate, xruns,
            m_thermalState.load(std::memory_order_relaxed), m_lowPower.load(std::memory_order_relaxed), !canRender);

        if (m_sampleRate > 0.0 && static_cast<double>(bufferToFill.numSamples) / m_sampleRate < duration)
        {
            INFO("Audio xrun %f ms / %f ms (samples = %d)", duration * 1000,
                bufferToFill.numSamples * 1000.0 / m_sampleRate, bufferToFill.numSamples);
        }
    }

    NonagonWrapper::IOInfo MakeIOInfo()
    {
        NonagonWrapper::IOInfo ioInfo;
        auto* device = m_deviceManager.getCurrentAudioDevice();
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

    void SetSampleDirectoryRootAbsolute(const std::filesystem::path& absolutePath)
    {
        INFO("Setting sample directory root to: %s", absolutePath.string().c_str());
        m_nonagon.SetSampleDirectoryRootAbsolute(absolutePath);
    }

    void SaveConfig()
    {
        // Config is built on the message thread, so the arena is local and may
        // be sized generously; it lives until PersistConfig has dumped it.
        //
        JsonArena arena(JsonArena::kDefaultCapacity);
        JSON config = arena.Object();
        JSON nonagonConfig = m_nonagon.ConfigToJSON(arena);
        nonagonConfig.SetNew("stereo", arena.Boolean(m_configuration.m_stereo));
        ClockModeConfigJSON::WriteExternalClock(nonagonConfig, arena, m_configuration.m_externalClock);
        nonagonConfig.SetNew("audio_input_device", arena.String(m_configuration.m_audioInputDeviceName.toUTF8().getAddress()));
        nonagonConfig.SetNew("audio_output_device", arena.String(m_configuration.m_audioOutputDeviceName.toUTF8().getAddress()));
        config.SetNew("nonagon_config", nonagonConfig);
        config.SetNew("file_config", m_fileManager.ToJSON(arena));
        FileManager::PersistConfig(config);
    }

    void LoadConfig()
    {
        // The parsed config tree points into `arena`, so it must outlive every
        // read below.
        //
        JsonArena arena(JsonArena::kDefaultCapacity);
        JSON config = FileManager::LoadConfig(arena);
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

                m_configuration.m_externalClock = ClockModeConfigJSON::ReadExternalClock(nonagonConfig, false);

                JSON audioInputDeviceJ = nonagonConfig.Get("audio_input_device");
                const char* audioInputDeviceName = audioInputDeviceJ.StringValue();
                if (audioInputDeviceName)
                {
                    m_configuration.m_audioInputDeviceName = juce::String(audioInputDeviceName);
                }

                JSON audioOutputDeviceJ = nonagonConfig.Get("audio_output_device");
                const char* audioOutputDeviceName = audioOutputDeviceJ.StringValue();
                if (audioOutputDeviceName)
                {
                    m_configuration.m_audioOutputDeviceName = juce::String(audioOutputDeviceName);
                }

                m_nonagon.ConfigFromJSON(nonagonConfig);
                m_nonagon.SetExternalClock(m_configuration.m_externalClock);
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

    void RequestNew()
    {
        m_nonagon.GetStateInterchange()->RequestNew();
    }

    // Parse the patch text into the interchange's load arena (message thread)
    // and arm the load. The parsed tree must outlive the audio thread's read,
    // so it is owned by the StateInterchange, not a local. Returns false on
    // parse failure or if a load is already in flight.
    //
    bool RequestLoad(const juce::String& jsonText)
    {
        return RequestLoad(jsonText, m_nonagon.ShouldRestoreFadersForPatchLoad(false));
    }

    bool RequestReload(const juce::String& jsonText)
    {
        return RequestLoad(jsonText, false);
    }

    bool RequestLoad(const juce::String& jsonText, bool restoreFaders)
    {
        StateInterchange* stateInterchange = m_nonagon.GetStateInterchange();
        JSON patch = stateInterchange->ParseForLoad(jsonText.toUTF8().getAddress());
        if (patch.IsNull())
        {
            return false;
        }

        return stateInterchange->RequestLoad(patch, restoreFaders);
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
    void ShowNewPatchChooser();
    void ShowVersionChooser();
    void OpenAudioDevice();
    void CloseAudioDevice();
    void RestartAudioDeviceForConfiguration();

    juce::AudioDeviceManager m_deviceManager;
    juce::AudioSourcePlayer m_audioSourcePlayer;

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

    double m_sampleRate = 0.0;
    AudioCallbackDiagnostics m_audioCallbackDiagnostics;
    std::atomic<int> m_thermalState{-1};
    std::atomic<bool> m_lowPower{false};
    uint32_t m_lastPlatformDiagnosticsMs = 0;

    FileManager m_fileManager{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
