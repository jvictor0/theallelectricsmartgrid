#pragma once

#include <cmath>
#include <cstdio>

template<bool On = false>
struct DebugMeter
{
    // Running RMS (exponential moving average of squared values)
    //
    float m_rmsSquared;

    // Peak value (exponential decay)
    //
    float m_peak;

    // Smoothing coefficients
    //
    float m_rmsAlpha;
    float m_peakDecay;

    DebugMeter()
        : m_rmsSquared(0.0f)
        , m_peak(0.0f)
        , m_rmsAlpha(0.0001f)
        , m_peakDecay(0.99995f)
    {
    }

    void SetTimeConstants(float rmsAlpha, float peakDecay)
    {
        m_rmsAlpha = rmsAlpha;
        m_peakDecay = peakDecay;
    }

    void Process(float sample)
    {
        if (!On)
        {
            return;
        }

        // Update RMS (exponential moving average of squared values)
        //
        float squared = sample * sample;
        m_rmsSquared = m_rmsSquared + m_rmsAlpha * (squared - m_rmsSquared);

        // Update peak with decay, capture new peaks
        //
        float absSample = std::fabs(sample);
        m_peak = m_peak * m_peakDecay;
        if (absSample > m_peak)
        {
            m_peak = absSample;
        }
    }

    float GetRMS() const
    {
        return std::sqrt(m_rmsSquared);
    }

    float GetPeak() const
    {
        return m_peak;
    }

    void Reset()
    {
        m_rmsSquared = 0.0f;
        m_peak = 0.0f;
    }

    void Print(const char* name) const
    {
        printf("%s: RMS=%.6f Peak=%.6f\n", name, GetRMS(), GetPeak());
    }
};
