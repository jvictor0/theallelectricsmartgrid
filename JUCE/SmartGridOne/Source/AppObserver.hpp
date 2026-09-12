#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "AsyncLogger.hpp"
#include "AudioCallbackDiagnostics.hpp"
#include "AudioPlatformDiagnostics.hpp"
#include "MidiSender.hpp"

struct AppObserver
{
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

    void PrepareAudio()
    {
        m_audioCallbackDiagnostics.Reset();
    }

    int64_t BeginAudioCallback(int frames, double sampleRate)
    {
        const auto start = juce::Time::getHighResolutionTicks();
        const auto startUs = static_cast<uint64_t>(juce::Time::highResolutionTicksToSeconds(start) * 1000000.0);
        const auto observation = m_audioCallbackDiagnostics.Observe(startUs, frames, sampleRate);
        m_audioCallbacks.store(observation.m_sequence, std::memory_order_relaxed);

        if (observation.m_previousBudgetUs > 0 && observation.m_gapUs > observation.m_previousBudgetUs * 2)
        {
            m_audioLongGaps.fetch_add(1, std::memory_order_relaxed);
            m_lastLongGapUs.store(observation.m_gapUs, std::memory_order_relaxed);
        }

        return start;
    }

    void EndAudioCallback(int64_t start, int frames, double sampleRate)
    {
        const auto duration = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start);
        if (sampleRate > 0.0 && duration > static_cast<double>(frames) / sampleRate)
        {
            m_audioOverruns.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void RecordFormatMute()
    {
        m_audioFormatMutes.fetch_add(1, std::memory_order_relaxed);
    }

    void Report(juce::AudioDeviceManager& deviceManager, MidiSender& midiSender)
    {
        const auto nowMs = juce::Time::getMillisecondCounter();
        if (m_lastDiagnosticsMs != 0 && nowMs - m_lastDiagnosticsMs < 1000)
        {
            return;
        }

        m_lastDiagnosticsMs = nowMs;
        const auto gaps = m_audioLongGaps.load(std::memory_order_relaxed);
        const auto overruns = m_audioOverruns.load(std::memory_order_relaxed);
        const auto mutes = m_audioFormatMutes.load(std::memory_order_relaxed);
        auto* device = deviceManager.getCurrentAudioDevice();
        const int xruns = device != nullptr ? device->getXRunCount() : -1;
        const bool timingChanged = gaps != m_reportedLongGaps || overruns != m_reportedOverruns
            || mutes != m_reportedFormatMutes || (xruns >= 0 && xruns != m_reportedXruns);
        const bool firstFailure = m_lastTimingLogMs == 0
            && (gaps > 0 || overruns > 0 || mutes > 0 || xruns > 0);
        if (timingChanged && (firstFailure || nowMs - m_lastTimingLogMs >= 60000))
        {
            m_lastTimingLogMs = nowMs;
            INFO("Audio timing callbacks=%llu long_gaps=%llu last_gap_us=%llu overruns=%llu format_mutes=%llu xruns=%d",
                static_cast<unsigned long long>(m_audioCallbacks.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(gaps),
                static_cast<unsigned long long>(m_lastLongGapUs.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(overruns),
                static_cast<unsigned long long>(mutes), xruns);
            m_reportedLongGaps = gaps;
            m_reportedOverruns = overruns;
            m_reportedFormatMutes = mutes;
            m_reportedXruns = xruns;
        }

        const auto platform = ReadAudioPlatformState();
        const bool periodic = m_lastRouteLogMs == 0 || nowMs - m_lastRouteLogMs >= 60000;
        if (periodic || platform.m_thermalState != m_reportedThermal
            || platform.m_lowPower != m_reportedLowPower)
        {
            INFO("Audio platform thermal=%d low_power=%d", platform.m_thermalState, platform.m_lowPower);
            m_reportedThermal = platform.m_thermalState;
            m_reportedLowPower = platform.m_lowPower;
        }

        if (periodic)
        {
            m_lastRouteLogMs = nowMs;
            if (device != nullptr)
            {
                INFO("Audio route name=%s rate=%.0f frames=%d inputs=%d outputs=%d cpu=%.3f",
                    device->getName().toRawUTF8(), device->getCurrentSampleRate(),
                    device->getCurrentBufferSizeSamples(),
                    device->getActiveInputChannels().countNumberOfSetBits(),
                    device->getActiveOutputChannels().countNumberOfSetBits(),
                    deviceManager.getCpuUsage());
            }

            LogAudioSessionDiagnostics();
            midiSender.LogDiagnostics();
        }
    }
};
