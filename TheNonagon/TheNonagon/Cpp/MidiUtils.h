#pragma once
#include <CoreAudio/CoreAudioTypes.h>

#include <AudioToolbox/AudioToolbox.h>
#include <mach/mach_time.h>

// Get the number of host ticks per sample
inline double HostTicksPerSample(double sampleRate)
{

    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    double ticksPerSecond = 1e9 * static_cast<double>(info.denom) / static_cast<double>(info.numer);

    return ticksPerSecond / static_cast<double>(sampleRate);
}

// Compute host time for a specific frame offset within a buffer
inline UInt64 AudioFrameToHostTime(
    const AudioTimeStamp& audioTimeStamp,
    uint32_t frameOffset,
    double sampleRate)
{
    if (!(audioTimeStamp.mFlags & kAudioTimeStampHostTimeValid))
    {
        return 0;
    }

    UInt64 baseHostTime = audioTimeStamp.mHostTime;
    UInt64 offsetTicks = static_cast<UInt64>(frameOffset * HostTicksPerSample(sampleRate));
    return baseHostTime + offsetTicks;
}
