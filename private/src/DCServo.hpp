#pragma once

#include <cmath>

// DC Servo - tracks and removes DC from a signal
// Uses a very slow one-pole lowpass to estimate DC, then subtracts it.
//

struct DCServo
{
    float m_value;
    float m_dc;
    float m_alpha;

    DCServo()
        : m_value(0.0f)
        , m_dc(0.0f)
        , m_alpha(0.0f)
    {
    }

    // Set cutoff frequency as cycles per sample
    // For 5 Hz at 192kHz: SetAlphaFromNatFreq(5.0f / 192000.0f)
    //
    void SetAlphaFromNatFreq(float cyclesPerSample)
    {
        // One-pole lowpass alpha from natural frequency
        //
        float omega = 2.0f * static_cast<float>(M_PI) * cyclesPerSample;
        m_alpha = 1.0f - std::exp(-omega);
    }

    // Set the raw value
    //
    void Set(float value)
    {
        m_value = value;
        m_dc += m_alpha * (m_value - m_dc);
    }

    // Get the raw value
    //
    float Get() const
    {
        return m_value;
    }

    // Get AC component (value - DC)
    //
    float GetAC() const
    {
        return m_value - m_dc;
    }

    // Get the current DC estimate
    //
    float GetDC() const
    {
        return m_dc;
    }

    void Reset()
    {
        m_value = 0.0f;
        m_dc = 0.0f;
    }
};
