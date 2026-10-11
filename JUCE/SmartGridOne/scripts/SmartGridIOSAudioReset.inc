// Included inside the pinned JUCE iOS device Pimpl by the Apple audio overlay.
// The message thread owns lifecycle work; callbackLock only detaches rendering.
//
bool m_resetPending = false;
AudioIODeviceCallback* m_resumeCallback = nullptr;

bool CheckAudioStatus(OSStatus status, const char* operation)
{
    if (status == noErr)
    {
        return true;
    }

    lastError = String(operation) + " failed (OSStatus " + String(status) + ")";
    return false;
}

bool CheckSessionResult(BOOL succeeded, NSError* const& error, const char* operation)
{
    if (succeeded)
    {
        return true;
    }

    lastError = String(operation) + " failed: "
        + (error != nil ? nsStringToJuce(error.localizedDescription) + " (" + String(static_cast<int64>(error.code)) + ")"
                        : String("no error details"));
    return false;
}

bool DisposeAudioUnit()
{
    if (audioUnit != nullptr)
    {
        // Do not hold callbackLock while native teardown waits for rendering.
        // Keep the unit and its callback storage alive if teardown fails.
        //
        const auto stopStatus = AudioOutputUnitStop(audioUnit);
        const auto uninitializeStatus = AudioUnitUninitialize(audioUnit);
        const auto disposeStatus = AudioComponentInstanceDispose(audioUnit);
        if (disposeStatus == noErr)
        {
            audioUnit = nullptr;
            workgroup = {};
        }

        return CheckAudioStatus(disposeStatus, "AudioComponentInstanceDispose")
            && (stopStatus == kAudioUnitErr_Uninitialized || CheckAudioStatus(stopStatus, "AudioOutputUnitStop"))
            && (uninitializeStatus == kAudioUnitErr_Uninitialized || CheckAudioStatus(uninitializeStatus, "AudioUnitUninitialize"));
    }

    return true;
}

String ResetAudio()
{
    if (!MessageManager::getInstance()->isThisTheMessageThread())
    {
        return "Audio reset must run on the message thread";
    }

    {
        const ScopedLock lock(callbackLock);
        if (!m_resetPending)
        {
            m_resumeCallback = callback;
        }

        callback = nullptr;
        isRunning = false;
        m_resetPending = true;
    }

    cancelPendingUpdate();
    if (m_resumeCallback != nullptr)
    {
        m_resumeCallback->audioDeviceStopped();
    }

    lastError.clear();
    if (!DisposeAudioUnit())
    {
        return lastError;
    }

    auto session = [AVAudioSession sharedInstance];
    const auto category = requestedInputChannels > 0 ? AVAudioSessionCategoryPlayAndRecord
                                                   : AVAudioSessionCategoryPlayback;
    const auto options = session.categoryOptions;
    const auto mode = session.mode;
    NSError* error = nil;
    if (!CheckSessionResult([session setActive:NO error:&error], error, "Deactivate audio session")
        || !CheckSessionResult([session setCategory:category mode:mode options:options error:&error], error, "Configure audio session")
        || !CheckSessionResult([session setPreferredSampleRate:targetSampleRate error:&error], error, "Set preferred sample rate")
        || !CheckSessionResult([session setPreferredIOBufferDuration:static_cast<double>(targetBufferSize) / targetSampleRate error:&error], error, "Set preferred buffer duration")
        || !CheckSessionResult([session setActive:YES error:&error], error, "Activate audio session"))
    {
        return lastError;
    }

    // JUCE's iOS 18 workaround: session buffer duration can stay stale until
    // the first native callback. Reuse its temporary RemoteIO warm-up.
    //
    if (@available(iOS 18, *))
    {
        lastError = SubstituteAudioUnit::WarmUp();
        if (lastError.isNotEmpty())
        {
            return lastError;
        }
    }

    sampleRate = getSampleRate();
    bufferSize = getBufferSize(sampleRate);
    if (!approximatelyEqual(sampleRate, targetSampleRate) || bufferSize != targetBufferSize)
    {
        lastError = "Audio format unavailable: " + String(sampleRate) + " Hz / " + String(bufferSize)
            + " frames (requested " + String(targetSampleRate) + " Hz / " + String(targetBufferSize) + " frames)";
        return lastError;
    }

    channelData.reconfigure(requestedInputChannels, requestedOutputChannels);
    if (channelData.outputs->numActiveChannels == 0)
    {
        lastError = "No audio output channels available";
        return lastError;
    }

    firstHostTime = true;
    lastNumFrames = 0;
    xrun = 0;
    if (!createAudioUnit() || !CheckAudioStatus(AudioOutputUnitStart(audioUnit), "AudioOutputUnitStart"))
    {
        return lastError;
    }

    // Start with silence, prepare the retained source, then reconnect it.
    // This does not wait for or reconstruct anything on the rendering thread.
    //
    if (m_resumeCallback != nullptr)
    {
        m_resumeCallback->audioDeviceAboutToStart(&owner);
    }

    {
        const ScopedLock lock(callbackLock);
        callback = std::exchange(m_resumeCallback, nullptr);
        isRunning = true;
        m_resetPending = false;
    }

    cancelPendingUpdate();
    return {};
}
