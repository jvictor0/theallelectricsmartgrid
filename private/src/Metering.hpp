#pragma once

#include <atomic>
#include <cmath>
#include "StereoUtils.hpp"
#include "QuadUtils.hpp"

struct Meter
{
    static constexpr float x_smoothingAlphaDown = 0.0002;
    static constexpr float x_smoothingAlphaUp = 0.0008;
    static constexpr float x_smoothingAlphaReduction = 0.0005;
    static constexpr float x_peakSmoothingAlpha = 0.00005;

    static constexpr size_t x_peakHistorySize = 48000;

    float m_rms;
    float m_peak;
    float m_reduction;
    size_t m_samplesSincePeak;

    Meter()
      : m_rms(0.0f)
      , m_peak(0.0f)
      , m_reduction(1.0f)
    {
    }

    void Process(float input)
    {
        float inputSquared = input * input;
        if (m_rms < inputSquared)
        {
            m_rms = m_rms * (1 - x_smoothingAlphaUp) + inputSquared * x_smoothingAlphaUp;
        }
        else
        {
            m_rms = m_rms * (1 - x_smoothingAlphaDown) + inputSquared * x_smoothingAlphaDown;
        }

        if (m_peak < std::abs(input))
        {
            m_peak = std::abs(input);
            m_samplesSincePeak = 0;
        }
        else if (m_samplesSincePeak < x_peakHistorySize)
        {
            m_samplesSincePeak++;
        }
        else
        {
            m_peak = m_peak * (1 - x_peakSmoothingAlpha) + std::abs(input) * x_peakSmoothingAlpha;
        }
    }

    void ProcessReduction(float reduction)
    {
        m_reduction = m_reduction * (1 - x_smoothingAlphaReduction) + reduction * x_smoothingAlphaReduction;
    }

    float ProcessAndSaturate(float input)
    {
        float output = std::atan(input * M_PI / 2) / (M_PI / 2);
        Process(output);
        ProcessReduction(std::max(0.00000000001f, std::abs(output)) / std::max(0.00000000001f, std::abs(input)));
        return output;
    }

    static float GetRMSDbFS(float rms)
    {
        return 10 * std::log10(std::max(0.00000000001f, rms));
    }

    static float GetPeakDbFS(float peak)
    {
        return 20 * std::log10(std::max(0.00000000001f, peak));
    }
};

struct MeterReader
{
    Meter* m_meter;

    std::atomic<float> m_rms;
    std::atomic<float> m_peak;
    std::atomic<float> m_reduction;

    MeterReader()
      : m_meter(nullptr)
      , m_rms(0.0f)
      , m_peak(0.0f)
      , m_reduction(0.0f)
    {
    }

    MeterReader(Meter* meter)
      : m_meter(meter)
      , m_rms(0.0f)
      , m_peak(0.0f)
      , m_reduction(0.0f)
    {
    }
    
    void Process()
    {
        m_rms.store(m_meter->m_rms);
        m_peak.store(m_meter->m_peak);
        m_reduction.store(m_meter->m_reduction);
    }

    float GetRMSDbFS() const
    {
        return Meter::GetRMSDbFS(m_rms.load());
    }
    
    float GetPeakDbFS() const
    {
        return Meter::GetPeakDbFS(m_peak.load());
    }

    float GetReductionDbFS() const
    {
        return Meter::GetPeakDbFS(m_reduction.load());
    }

    float GetRMSLinear() const
    {
        return m_rms.load();
    }

    float GetPeakLinear() const
    {
        return m_peak.load();
    }

    float GetRMSDbFSNormalized() const
    {
        return std::max(0.0f, (60.0f + GetRMSDbFS()) / 60.0f);
    }

    float GetPeakDbFSNormalized() const
    {
        return std::max(0.0f, (60.0f + GetPeakDbFS()) / 60.0f);
    }

    float GetReductionDbFSNormalized() const
    {
        return std::max(0.0f, (18.0f + GetReductionDbFS()) / 18.0f);
    }
};

struct QuadMeter
{
    Meter m_meters[4];

    QuadMeter()
    {
    }
    
    void Process(QuadFloat input)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_meters[i].Process(input[i]);
        }
    }
};

struct QuadMeterReader
{
    QuadMeter* m_meter;

    MeterReader m_meterReaders[4];

    QuadMeterReader()
      : m_meter(nullptr)

    {
    }

    QuadMeterReader(QuadMeter* meter)
    {
        SetMeterReaders(meter);
    }

    void SetMeterReaders(QuadMeter* meter)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_meterReaders[i].m_meter = &meter->m_meters[i];
        }

        m_meter = meter;
    }
    
    void Process()
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_meterReaders[i].Process();
        }
    }

    float GetRMSDbFS(size_t i) const
    {
        return m_meterReaders[i].GetRMSDbFS();
    }
    
    float GetPeakDbFS(size_t i) const
    {
        return m_meterReaders[i].GetPeakDbFS();
    }

    float GetReductionDbFS(size_t i) const
    {
        return m_meterReaders[i].GetReductionDbFS();
    }

    float GetRMSLinear(size_t i) const
    {
        return m_meterReaders[i].GetRMSLinear();
    }

    float GetPeakLinear(size_t i) const
    {
        return m_meterReaders[i].GetPeakLinear();
    }

    float GetRMSDbFSNormalized(size_t i) const
    {
        return m_meterReaders[i].GetRMSDbFSNormalized();
    }

    float GetPeakDbFSNormalized(size_t i) const
    {
        return m_meterReaders[i].GetPeakDbFSNormalized();
    }

    float GetReductionDbFSNormalized(size_t i) const
    {
        return m_meterReaders[i].GetReductionDbFSNormalized();
    }
};

struct StereoMeter
{
    Meter m_meters[2];

    StereoMeter()
    {
    }
    
    void Process(StereoFloat input)
    {
        for (size_t i = 0; i < 2; ++i)
        {
            m_meters[i].Process(input[i]);
        }
    }
};

struct StereoMeterReader
{
    StereoMeter* m_meter;

    MeterReader m_meterReaders[2];

    StereoMeterReader()
      : m_meter(nullptr)

    {
    }

    StereoMeterReader(StereoMeter* meter)
    {
        SetMeterReaders(meter);
    }

    void SetMeterReaders(StereoMeter* meter)
    {
        for (size_t i = 0; i < 2; ++i)
        {
            m_meterReaders[i].m_meter = &meter->m_meters[i];
        }

        m_meter = meter;
    }
    
    void Process()
    {
        for (size_t i = 0; i < 2; ++i)
        {
            m_meterReaders[i].Process();
        }
    }

    float GetRMSDbFS(size_t i) const
    {
        return m_meterReaders[i].GetRMSDbFS();
    }
    
    float GetPeakDbFS(size_t i) const
    {
        return m_meterReaders[i].GetPeakDbFS();
    }

    float GetReductionDbFS(size_t i) const
    {
        return m_meterReaders[i].GetReductionDbFS();
    }

    float GetRMSLinear(size_t i) const
    {
        return m_meterReaders[i].GetRMSLinear();
    }

    float GetPeakLinear(size_t i) const
    {
        return m_meterReaders[i].GetPeakLinear();
    }

    float GetRMSDbFSNormalized(size_t i) const
    {
        return m_meterReaders[i].GetRMSDbFSNormalized();
    }

    float GetPeakDbFSNormalized(size_t i) const
    {
        return m_meterReaders[i].GetPeakDbFSNormalized();
    }

    float GetReductionDbFSNormalized(size_t i) const
    {
        return m_meterReaders[i].GetReductionDbFSNormalized();
    }
};