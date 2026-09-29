#include <JuceHeader.h>
#include "AudioPlatformDiagnostics.hpp"
#include "AsyncLogger.hpp"

#import <Foundation/Foundation.h>
#if JUCE_IOS
#import <AVFoundation/AVFoundation.h>
#endif

AudioPlatformState LogAudioPlatformDiagnostics(uint64_t monotonicUs, int64_t wallTimeMs)
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

        INFO("Audio system t_us=%llu wall_ms=%lld thermal=%d low_power=%d (thermal: 0=nominal 1=fair 2=serious 3=critical)",
            static_cast<unsigned long long>(monotonicUs),
            static_cast<long long>(wallTimeMs), state.m_thermalState, state.m_lowPower);

#if JUCE_IOS
        AVAudioSession* session = [AVAudioSession sharedInstance];
        INFO("Audio session sr=%.0f preferred_sr=%.0f io_us=%.0f preferred_io_us=%.0f in_latency_us=%.0f out_latency_us=%.0f inputs=%ld outputs=%ld",
            session.sampleRate, session.preferredSampleRate,
            session.IOBufferDuration * 1000000.0, session.preferredIOBufferDuration * 1000000.0,
            session.inputLatency * 1000000.0, session.outputLatency * 1000000.0,
            static_cast<long>(session.inputNumberOfChannels), static_cast<long>(session.outputNumberOfChannels));

        for (AVAudioSessionPortDescription* port in session.currentRoute.inputs)
        {
            INFO("Audio route input name=%s type=%s uid=%s",
                port.portName.UTF8String, port.portType.UTF8String, port.UID.UTF8String);
        }

        for (AVAudioSessionPortDescription* port in session.currentRoute.outputs)
        {
            INFO("Audio route output name=%s type=%s uid=%s",
                port.portName.UTF8String, port.portType.UTF8String, port.UID.UTF8String);
        }
#endif
    }

    return state;
}

#if JUCE_IOS
#include "RemoteIOProbeBridge.mm"
#endif
