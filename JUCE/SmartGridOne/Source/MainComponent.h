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
#include <atomic>

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

    static constexpr int x_requiredBlockFrames = 512;

    virtual void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        m_audioReady.store(false, std::memory_order_release);
        m_sampleRate = sampleRate;
        m_audioCallbackDiagnostics.Reset();
        if (AudioCallbackDiagnostics::CanRender(sampleRate) && samplesPerBlockExpected == x_requiredBlockFrames)
        {
            SampleTimer::Init(samplesPerBlockExpected);
            m_nonagon.PrepareToPlay(samplesPerBlockExpected, sampleRate);
            m_audioReady.store(true, std::memory_order_release);
        }
        else
        {
            INFO("Audio format rejected: rate=%.0f frames=%d required_rate=%zu required_frames=%d",
                sampleRate, samplesPerBlockExpected, SampleTimer::x_sampleRate, x_requiredBlockFrames);
        }
    }

    virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        ScopedThreadId scopedThreadId(ThreadId::Audio);
        const auto start = juce::Time::getHighResolutionTicks();
        const auto startUs = static_cast<uint64_t>(juce::Time::highResolutionTicksToSeconds(start) * 1000000.0);
        const auto observation = m_audioCallbackDiagnostics.Observe(startUs, bufferToFill.numSamples, m_sampleRate);
        m_audioCallbacks.store(observation.m_sequence, std::memory_order_relaxed);

        if (observation.m_previousBudgetUs > 0 && observation.m_gapUs > observation.m_previousBudgetUs * 2)
        {
            m_audioLongGaps.fetch_add(1, std::memory_order_relaxed);
            m_lastLongGapUs.store(observation.m_gapUs, std::memory_order_relaxed);
        }

        const bool canRender = m_audioReady.load(std::memory_order_acquire)
            && AudioCallbackDiagnostics::CanRender(m_sampleRate)
            && bufferToFill.numSamples == x_requiredBlockFrames;
        if (canRender)
        {
            m_configuration.m_forceStereo = bufferToFill.buffer->getNumChannels() < 4;
            m_configuration.m_stereo = m_configuration.m_stereo || m_configuration.m_forceStereo;
            m_nonagon.Process(bufferToFill, MakeIOInfo());
        }
        else
        {
            bufferToFill.clearActiveBufferRegion();
            m_audioFormatMutes.fetch_add(1, std::memory_order_relaxed);
        }

        const auto duration = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start);
        if (m_sampleRate > 0.0 && duration > static_cast<double>(bufferToFill.numSamples) / m_sampleRate)
        {
            m_audioOverruns.fetch_add(1, std::memory_order_relaxed);
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
        m_audioReady.store(false, std::memory_order_release);
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
    std::atomic<bool> m_audioReady{false};
    AudioCallbackDiagnostics m_audioCallbackDiagnostics;
    std::atomic<uint64_t> m_audioCallbacks{0};
    std::atomic<uint64_t> m_audioLongGaps{0};
    std::atomic<uint64_t> m_lastLongGapUs{0};
    std::atomic<uint64_t> m_audioOverruns{0};
    std::atomic<uint64_t> m_audioFormatMutes{0};
    uint32_t m_lastTimingLogMs = 0;
    uint64_t m_reportedLongGaps = 0;
    uint64_t m_reportedOverruns = 0;
    uint64_t m_reportedFormatMutes = 0;
    int m_reportedXruns = -1;
    int m_reportedThermal = -1;
    bool m_reportedLowPower = false;
    uint32_t m_lastDiagnosticsMs = 0;
    uint32_t m_lastRouteLogMs = 0;

    FileManager m_fileManager{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
