#include <JuceHeader.h>
#include "AudioPlatformDiagnostics.hpp"
#include "AsyncLogger.hpp"

#import <Foundation/Foundation.h>
#if JUCE_IOS
#import <AVFoundation/AVFoundation.h>
#endif

AudioPlatformState ReadAudioPlatformState()
{
    AudioPlatformState state;
    @autoreleasepool
    {
        NSProcessInfo* process = [NSProcessInfo processInfo];
        state.m_thermalState = static_cast<int>(process.thermalState);
        if (@available(macOS 12.0, iOS 9.0, *))
        {
            state.m_lowPower = process.lowPowerModeEnabled;
        }
    }

    return state;
}

void LogAudioSessionDiagnostics()
{
#if JUCE_IOS
    @autoreleasepool
    {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        INFO("Audio session rate=%.0f preferred_rate=%.0f io_us=%.0f preferred_io_us=%.0f inputs=%ld outputs=%ld",
            session.sampleRate, session.preferredSampleRate,
            session.IOBufferDuration * 1000000.0, session.preferredIOBufferDuration * 1000000.0,
            static_cast<long>(session.inputNumberOfChannels),
            static_cast<long>(session.outputNumberOfChannels));

        for (AVAudioSessionPortDescription* port in session.currentRoute.inputs)
        {
            INFO("Audio session input name=%s type=%s", port.portName.UTF8String, port.portType.UTF8String);
        }

        for (AVAudioSessionPortDescription* port in session.currentRoute.outputs)
        {
            INFO("Audio session output name=%s type=%s", port.portName.UTF8String, port.portType.UTF8String);
        }
    }
#endif
}
