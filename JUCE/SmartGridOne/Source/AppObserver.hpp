#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "AsyncLogger.hpp"
#include "AudioCallbackMetrics.hpp"
#include "AudioPlatformDiagnostics.hpp"
#include "MidiSender.hpp"

struct AppObserver
{
    AudioCallbackMetrics m_audioMetrics;
    AudioCallbackMetrics::Statistics m_metricsWindow;
    uint64_t m_omittedIncidents = 0;
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
        m_audioMetrics.Prepare();
    }

    AudioCallbackMetrics::Start BeginAudioCallback(int frames, double sampleRate)
    {
        return m_audioMetrics.Begin(frames, sampleRate, SampleTimer::GetSample());
    }

    void EndAudioCallback(AudioCallbackMetrics::Start start)
    {
        m_audioMetrics.End(start);
    }

    void FinishAudio()
    {
        m_audioMetrics.Publish();
    }

    void RecordFormatMute()
    {
        m_audioFormatMutes.fetch_add(1, std::memory_order_relaxed);
    }

    static void LogCallback(const char* kind, const AudioCallbackMetrics::Record& record)
    {
        INFO("Audio callback kind=%s id=%llu mono_us=%llu engine_sample=%llu frames=%d rate=%.0f gap_us=%llu",
            kind, static_cast<unsigned long long>(record.m_callback),
            static_cast<unsigned long long>(record.m_startUs),
            static_cast<unsigned long long>(record.m_engineSample), record.m_frames, record.m_sampleRate,
            static_cast<unsigned long long>(record.m_gapUs));
        INFO("Audio callback timing id=%llu wall_us=%llu cpu_us=%llu cpu_ok=%d excess_us=%llu budget_us=%llu prev_budget_us=%llu",
            static_cast<unsigned long long>(record.m_callback),
            static_cast<unsigned long long>(record.m_wallUs), static_cast<unsigned long long>(record.m_cpuUs),
            static_cast<int>(record.m_cpuValid),
            static_cast<unsigned long long>(record.ArrivalExcessUs()),
            static_cast<unsigned long long>(record.m_budgetUs), static_cast<unsigned long long>(record.m_previousBudgetUs));
    }

    void CollectMetrics()
    {
        size_t incidents = 0;
        AudioCallbackMetrics::Batch batch;
        for (size_t i = 0; i < AudioCallbackMetrics::x_batches && m_audioMetrics.m_batches.Pop(batch); ++i)
        {
            m_metricsWindow.Merge(batch.m_statistics);
            m_omittedIncidents += batch.m_omittedIncidents;
            for (size_t j = 0; j < batch.m_incidentCount; ++j)
            {
                if (incidents < AudioCallbackMetrics::x_incidentsPerBatch)
                {
                    LogCallback("incident", batch.m_incidents[j]);
                    ++incidents;
                }
                else
                {
                    ++m_omittedIncidents;
                }
            }
        }
    }

    void LogMetrics()
    {
        const auto& metrics = m_metricsWindow;
        INFO("Audio metrics count=%llu cpu_count=%llu wall_us=%llu cpu_us=%llu budget_us=%llu omitted_incidents=%llu",
            static_cast<unsigned long long>(metrics.m_count), static_cast<unsigned long long>(metrics.m_cpuCount),
            static_cast<unsigned long long>(metrics.m_wallUs), static_cast<unsigned long long>(metrics.m_cpuUs),
            static_cast<unsigned long long>(metrics.m_budgetUs), static_cast<unsigned long long>(m_omittedIncidents));
        INFO("Audio metrics range first_mono_us=%llu last_mono_us=%llu",
            static_cast<unsigned long long>(metrics.m_firstStartUs), static_cast<unsigned long long>(metrics.m_lastStartUs));
        if (metrics.m_count > 0)
        {
            LogCallback("worst_wall", metrics.m_worstWall);
            LogCallback("worst_arrival", metrics.m_worstArrival);
            if (metrics.m_cpuCount > 0)
            {
                LogCallback("worst_cpu", metrics.m_worstCpu);
            }
        }

        m_metricsWindow = {};
        m_omittedIncidents = 0;
    }

    bool Report(juce::AudioDeviceManager& deviceManager, MidiSender& midiSender)
    {
        const auto nowMs = juce::Time::getMillisecondCounter();
        if (m_lastDiagnosticsMs != 0 && nowMs - m_lastDiagnosticsMs < 1000)
        {
            return false;
        }

        m_lastDiagnosticsMs = nowMs;
        CollectMetrics();
        const auto gaps = m_audioMetrics.m_longGaps.load(std::memory_order_relaxed);
        const auto overruns = m_audioMetrics.m_overruns.load(std::memory_order_relaxed);
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
                static_cast<unsigned long long>(m_audioMetrics.m_callbacks.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(gaps),
                static_cast<unsigned long long>(m_audioMetrics.m_lastLongGapUs.load(std::memory_order_relaxed)),
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
            const auto monoUs = AudioCallbackClock::WallUs();
            const auto utcMs = juce::Time::getCurrentTime().toMilliseconds();
            INFO("Audio clock mono_us=%llu utc_ms=%lld engine_rate=%zu",
                static_cast<unsigned long long>(monoUs), static_cast<long long>(utcMs), SampleTimer::x_sampleRate);
            LogMetrics();
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

        return periodic;
    }
};
