#include "MainComponent.h"
#include "AsyncLogger.hpp"

//==============================================================================
MainComponent::MainComponent()
    : m_configPage(nullptr)
    , m_filePage(nullptr)
    , m_wrldBuildrGrid(nullptr)
    , m_configButton("Config")
    , m_backButton("Back")
    , m_fileButton("File")
    , m_showingConfig(false)
    , m_showingFile(false)
{
    juce::File logsDir = FileManager::GetLogsDirectory();
    AsyncLogQueue::s_instance.ConfigureLogDirectory(logsDir.getFullPathName().toUTF8().getAddress());
    INFO("Audio experiment juce_version=%s", juce::SystemStats::getJUCEVersion().toRawUTF8());
    INFO("Audio experiment audio_mode=%s ui_render=%d transport_autostart=%d",
        m_useTestTone ? "tone" : "normal", m_renderTestUi, !m_useTestTone && m_startTestPlayback);

    setSize (1000, 700);
    
    // Set up config button
    m_configButton.setButtonText("Config");
    m_configButton.setSize(50, 50);
    m_configButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_configButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    m_configButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_configButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
    m_configButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_configButton.onClick = [this]() { OnConfigButtonClicked(); };
    addAndMakeVisible(m_configButton);
    
    // Set up back button
    m_backButton.setSize(80, 30);
    m_backButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_backButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    m_backButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_backButton.onClick = [this]() { OnBackButtonClicked(); };
    addAndMakeVisible(m_backButton);
    m_backButton.setVisible(false);
    
    // Set up file button
    m_fileButton.setSize(50, 50);
    m_fileButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    m_fileButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    m_fileButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_fileButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
    m_fileButton.onClick = [this]() { OnFileButtonClicked(); };
    addAndMakeVisible(m_fileButton);
    
    // Set up CPU label
    m_cpuLabel.setSize(50, 50);
    m_cpuLabel.setText("0%", juce::dontSendNotification);
    m_cpuLabel.setJustificationType(juce::Justification::centred);
    m_cpuLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    m_cpuLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
    addAndMakeVisible(m_cpuLabel);

    // Set up world builder grid
    m_wrldBuildrGrid = std::make_unique<WrldBuildrComponent>(&m_nonagon);
    addAndMakeVisible(m_wrldBuildrGrid.get());
    
    // Ensure config page starts closed
    m_showingConfig = false;

    m_fileManager.SetDefaultRecordingDirectory();
    LoadConfig();
    if (!m_useTestTone && m_startTestPlayback)
    {
        m_nonagon.m_internal.m_running = true;
    }

#if JUCE_IOS
    StartRemoteIOCapture(logsDir.getFullPathName().toRawUTF8());
#endif
    OpenAudioDevice();

    // Start the 60 FPS timer for re-rendering
    //
    startTimer(1000 / 60); // 60 FPS


    // Force initial layout calculation after everything is set up
    resized();
    
    // Ensure the component is properly sized and laid out
    setSize(1000, 700);
    resized();
    
    // Force the world builder grid to update its display mode
    if (m_renderTestUi && m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->SetDisplayMode();
    }

    if (!m_renderTestUi)
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            getChildComponent(i)->setVisible(false);
        }
    }
    
    // Trigger a repaint to ensure everything is drawn correctly
    repaint();
}

MainComponent::~MainComponent()
{
    CloseAudioDevice();
#if JUCE_IOS
    StopRemoteIOCapture();
#endif
    m_nonagon.ClearLEDs();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (!m_renderTestUi)
    {
        g.setColour(juce::Colours::white);
        g.drawText("SmartGridOne: UI rendering paused for audio test", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    if (!m_showingConfig)
    {
        g.setFont (juce::FontOptions (24.0f));
        g.setColour (juce::Colours::white);
        g.drawText ("SmartGrid One", getLocalBounds().removeFromTop(100), juce::Justification::centred, true);
        
        g.setFont (juce::FontOptions (16.0f));
        g.drawText ("Click the gear icon to configure launchpads", getLocalBounds().removeFromTop(150), juce::Justification::centred, true);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Right side panel area
    auto rightPanel = bounds.removeFromRight(50);
    
    // Config button in top right
    m_configButton.setBounds(rightPanel.removeFromTop(50));
    
    // File button directly below config button, same size
    m_fileButton.setBounds(rightPanel.removeFromTop(50));
    
    // CPU label below file button, same size
    m_cpuLabel.setBounds(rightPanel.removeFromTop(50));
    
    // Back button in top left (only visible when showing config or file)
    if (m_showingConfig || m_showingFile)
    {
        m_backButton.setBounds(bounds.removeFromLeft(100).removeFromTop(50));
    }
    
    // Show appropriate page
    if (m_configPage && m_showingConfig)
    {
        m_configPage->setBounds(bounds);
    }
    else if (m_filePage && m_showingFile)
    {
        m_filePage->setBounds(bounds);
    }
    else if (m_wrldBuildrGrid && !m_showingConfig && !m_showingFile)
    {
        // Let the world builder grid handle its own sizing and positioning
        //
        m_wrldBuildrGrid->setBounds(bounds);
    }
    
    // Show patch chooser as overlay if visible
    //
    if (m_patchChooser && m_patchChooser->isVisible())
    {
        // Center it on screen
        //
        auto chooserBounds = bounds.reduced(bounds.getWidth() / 4, bounds.getHeight() / 4);
        m_patchChooser->setBounds(chooserBounds);
    }
    
    // Show version chooser as overlay if visible
    //
    if (m_versionChooser && m_versionChooser->isVisible())
    {
        // Center it on screen
        //
        auto chooserBounds = bounds.reduced(bounds.getWidth() / 4, bounds.getHeight() / 4);
        m_versionChooser->setBounds(chooserBounds);
    }
}

//==============================================================================
void MainComponent::OnConfigButtonClicked()
{
    if (!m_configPage)
    {
        m_configPage = std::make_unique<ConfigPage>(&m_nonagon, &m_configuration, &m_deviceManager);
        addAndMakeVisible(m_configPage.get());
    }
    
    m_showingConfig = true;
    m_showingFile = false;
    m_configButton.setVisible(false);
    m_fileButton.setVisible(false);
    m_backButton.setVisible(true);
    
    if (m_configPage)
    {
        m_configPage->setVisible(true);
    }
    
    if (m_filePage)
    {
        m_filePage->setVisible(false);
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(false);
    }
    
    resized();
    repaint();
}

void MainComponent::OnBackButtonClicked()
{
    bool shouldRestartAudio = false;

    if (m_configPage)
    {
        shouldRestartAudio = m_configPage->AudioDeviceConfigurationChanged();

        if (shouldRestartAudio)
        {
            m_configPage->ApplyAudioConfiguration();
        }
    }

    m_showingConfig = false;
    m_showingFile = false;
    m_configButton.setVisible(true);
    m_fileButton.setVisible(true);
    m_backButton.setVisible(false);
    
    if (m_configPage)
    {
        removeChildComponent(m_configPage.get());
        m_configPage.reset();
    }
    
    if (m_filePage)
    {
        removeChildComponent(m_filePage.get());
        m_filePage.reset();
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(true);
    }
    
    resized();
    repaint();

    if (shouldRestartAudio)
    {
        RestartAudioDeviceForConfiguration();
    }

    SaveConfig();
}

void MainComponent::OpenAudioDevice()
{
#if JUCE_IOS
    SmartGridRemoteIOLifecycle("app.open", nullptr, 0, 0, 1);
#endif
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.sampleRate = static_cast<double>(SampleTimer::x_sampleRate);
    setup.bufferSize = 512;
    // Temporary output-only test. NonagonWrapper supplies zero for absent physical inputs.
    //
    setup.inputDeviceName = {};
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.outputDeviceName = m_configuration.m_audioOutputDeviceName;
    const auto error = m_deviceManager.initialise(
        0, 7, nullptr, false, {}, &setup);
    if (error.isNotEmpty())
    {
        INFO("Audio initialization failed: %s", error.toRawUTF8());
#if JUCE_IOS
        SmartGridRemoteIOLifecycle("app.open", nullptr, 0, -1, 2);
#endif
        return;
    }

    auto* device = m_deviceManager.getCurrentAudioDevice();
    if (device == nullptr || !AudioCallbackDiagnostics::CanRender(device->getCurrentSampleRate()))
    {
        INFO("Audio initialization rejected: actual=%.0f required=%zu",
            device != nullptr ? device->getCurrentSampleRate() : 0.0, SampleTimer::x_sampleRate);
        m_deviceManager.closeAudioDevice();
#if JUCE_IOS
        SmartGridRemoteIOLifecycle("app.open", nullptr, 0, -2, 2);
#endif
        return;
    }

    INFO("Audio setup requested_sr=%.0f requested_block=%d actual_sr=%.0f actual_block=%d",
        setup.sampleRate, setup.bufferSize, device->getCurrentSampleRate(),
        device->getCurrentBufferSizeSamples());
    INFO("Audio input test: requested_inputs=0 active_inputs=%d active_outputs=%d; absent DSP inputs are zero",
        device->getActiveInputChannels().countNumberOfSetBits(),
        device->getActiveOutputChannels().countNumberOfSetBits());

    // Attach directly so the accepted device is not closed and reopened.
    //
    m_deviceManager.addAudioCallback(&m_audioSourcePlayer);
    m_audioSourcePlayer.setSource(this);
#if JUCE_IOS
    SmartGridRemoteIOLifecycle("app.open", nullptr, 0, 0, 2);
#endif
}

void MainComponent::CloseAudioDevice()
{
#if JUCE_IOS
    SmartGridRemoteIOLifecycle("app.close", nullptr, 0, 0, 1);
#endif
    m_audioSourcePlayer.setSource(nullptr);
    m_deviceManager.removeAudioCallback(&m_audioSourcePlayer);
    m_deviceManager.closeAudioDevice();
#if JUCE_IOS
    SmartGridRemoteIOLifecycle("app.close", nullptr, 0, 0, 2);
#endif
}

void MainComponent::RestartAudioDeviceForConfiguration()
{
#if JUCE_IOS
    SmartGridRemoteIOLifecycle("app.restartConfiguration", nullptr, 0, 0, 1);
#endif
    CloseAudioDevice();
    OpenAudioDevice();
#if JUCE_IOS
    SmartGridRemoteIOLifecycle("app.restartConfiguration", nullptr, 0, 0, 2);
#endif
}

void MainComponent::OnFileButtonClicked()
{
    if (!m_filePage)
    {
        m_filePage = std::make_unique<FilePage>(
            [this]() { ShowPatchChooser(false); },
            [this]() { ShowVersionChooser(); },
            [this]()
            { 
                // Save - if no patch name, show chooser, otherwise just save
                //
                if (m_fileManager.GetCurrentPatchName().isEmpty())
                {
                    ShowPatchChooser(true);
                }
                else
                {
                    RequestSave();
                }
            },
            [this]() { ShowPatchChooser(true); },
            [this]() { ShowNewPatchChooser(); }
        );

        addAndMakeVisible(m_filePage.get());
    }
    
    m_showingFile = true;
    m_showingConfig = false;
    m_configButton.setVisible(false);
    m_fileButton.setVisible(false);
    m_backButton.setVisible(true);
    
    if (m_filePage)
    {
        m_filePage->setVisible(true);
    }
    
    if (m_configPage)
    {
        m_configPage->setVisible(false);
    }
    
    if (m_wrldBuildrGrid)
    {
        m_wrldBuildrGrid->setVisible(false);
    }
    
    resized();
    repaint();
}

void MainComponent::OnFileBackButtonClicked()
{
    OnBackButtonClicked();
}

void MainComponent::ShowPatchChooser(bool isSaveMode)
{
    // Always recreate the chooser to ensure fresh state and correct mode
    //
    m_patchChooser.reset();
    
    m_patchChooser = std::make_unique<PatchChooser>(
        [this, isSaveMode](juce::String patchName)
        {
            if (isSaveMode)
            {
                // Set patch name and trigger save
                //
                m_fileManager.SetCurrentPatchName(patchName);
                RequestSave();
                SaveConfig();
            }
            else
            {
                m_fileManager.LoadPatch(patchName);
                SaveConfig();
            }
            
            // Destroy the chooser completely
            //
            m_patchChooser.reset();
            
            // Go back to main screen
            //
            OnBackButtonClicked();
        },
        [this]()
        {
            // Cancel - destroy the chooser completely
            //
            m_patchChooser.reset();
            resized();
            repaint();
        },
        isSaveMode
    );
    
    addAndMakeVisible(m_patchChooser.get());
    resized();
    repaint();
}

void MainComponent::ShowNewPatchChooser()
{
    // Always recreate the chooser to ensure fresh state
    //
    m_patchChooser.reset();
    
    m_patchChooser = std::make_unique<PatchChooser>(
        [this](juce::String patchName)
        {
            // Create the new patch directory and set it as current
            //
            if (m_fileManager.CreateNewPatch(patchName))
            {
                // Revert everything to defaults
                //
                RequestNew();
                SaveConfig();
            }
            
            // Destroy the chooser completely
            //
            m_patchChooser.reset();
            
            // Go back to main screen
            //
            OnBackButtonClicked();
        },
        [this]()
        {
            // Cancel - destroy the chooser completely
            //
            m_patchChooser.reset();
            resized();
            repaint();
        },
        true  // Save mode to allow entering new name
    );
    
    addAndMakeVisible(m_patchChooser.get());
    resized();
    repaint();
}

void MainComponent::ShowVersionChooser()
{
    // Only show if we have a current patch
    //
    if (m_fileManager.GetCurrentPatchName().isEmpty())
    {
        return;
    }

    // Always recreate the chooser to ensure fresh state
    //
    m_versionChooser.reset();
    
    m_versionChooser = std::make_unique<VersionChooser>(
        m_fileManager.GetCurrentPatchName(),
        [this](juce::String versionFilePath)
        {
            m_fileManager.LoadPatchVersion(versionFilePath);
            
            // Destroy the chooser completely
            //
            m_versionChooser.reset();
            
            // Go back to main screen
            //
            OnBackButtonClicked();
        },
        [this]()
        {
            // Cancel - destroy the chooser completely
            //
            m_versionChooser.reset();
            resized();
            repaint();
        }
    );
    
    addAndMakeVisible(m_versionChooser.get());
    resized();
    repaint();
}

//==============================================================================
void MainComponent::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounter();
    if (m_lastPlatformDiagnosticsMs == 0 || nowMs - m_lastPlatformDiagnosticsMs >= 1000)
    {
        m_lastPlatformDiagnosticsMs = nowMs;
        const auto nowUs = static_cast<uint64_t>(juce::Time::highResolutionTicksToSeconds(
            juce::Time::getHighResolutionTicks()) * 1000000.0);
        const auto state = LogAudioPlatformDiagnostics(nowUs, juce::Time::currentTimeMillis());
        m_thermalState.store(state.m_thermalState, std::memory_order_relaxed);
        m_lowPower.store(state.m_lowPower, std::memory_order_relaxed);
        m_nonagon.m_midiSender.LogDiagnostics();
        if (auto* device = m_deviceManager.getCurrentAudioDevice())
        {
            INFO("Audio device name=%s sr=%.0f block=%d in_latency=%d out_latency=%d cpu=%.4f",
                device->getName().toRawUTF8(), device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples(),
                device->getInputLatencyInSamples(), device->getOutputLatencyInSamples(), m_deviceManager.getCpuUsage());
        }
    }

    // Re-renderthe component at 60 FPS
    //
    HandleStateInterchange();
    if (m_renderTestUi)
    {
        m_wrldBuildrGrid->SetDisplayMode();

        // Update CPU label
        //
        m_cpuUsageBuffer.Write(m_deviceManager.getCpuUsage() * 100.0);
        m_cpuLabel.setText(juce::String(m_cpuUsageBuffer.Max(), 1) + "%", juce::dontSendNotification);

        repaint();
    }

    AsyncLogQueue::s_instance.DoLog();
}
