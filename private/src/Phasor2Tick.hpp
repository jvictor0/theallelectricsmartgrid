#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

struct Phasor2Tick
{
    // MIDI clock: 24 pulses per quarter note (PPQN)
    // 90 BPM = 36 Hz = 36/48000 cycles per sample
    // 180 BPM = 72 Hz = 72/48000 cycles per sample
    // Target: 135 BPM = 54 Hz = 54/48000 cycles per sample (middle of range)
    //
    static constexpr double x_sampleRate = 48000.0;
    static constexpr double x_targetTickFreq = 54.0 / x_sampleRate;
    static constexpr double x_minTickFreq = 36.0 / x_sampleRate;
    static constexpr double x_maxTickFreq = 72.0 / x_sampleRate;

    size_t m_divisions;
    double m_lastPhase;

    Phasor2Tick()
    {
        m_divisions = 1;
        m_lastPhase = 0;
    }

    bool Process(double phase)
    {
        bool tick = false;
        if (static_cast<size_t>(phase * m_divisions) != static_cast<size_t>(m_lastPhase * m_divisions))
        {
            tick = true;
        }

        m_lastPhase = phase;
        return tick;
    }

    void UpdateDivisions(double freq)
    {
        if (freq <= 0.0)
        {
            m_divisions = 24;
        }
        else
        {
            // Calculate divisions to target 135 BPM (54 Hz tick rate in cycles per sample)
            //
            double targetDivisions = x_targetTickFreq / freq;
            
            // Clamp to ensure we stay within 90-180 BPM range
            //
            double minDivisions = x_minTickFreq / freq;
            double maxDivisions = x_maxTickFreq / freq;
            
            targetDivisions = std::max(minDivisions, std::min(maxDivisions, targetDivisions));
            
            // Round to nearest value that is 24 * 2^n
            //
            double powerOf2Factor = targetDivisions / 24.0;
            int n = static_cast<int>(std::round(std::log2(std::max(1.0, powerOf2Factor))));
            
            // Calculate candidate divisions (24 * 2^n)
            //
            size_t candidate = static_cast<size_t>(24 * std::pow(2.0, n));
            
            // Check if candidate is within valid range, if not try adjacent values
            //
            double candidateTickFreq = freq * candidate;
            if (candidateTickFreq < x_minTickFreq)
            {
                // Try next higher power of 2
                //
                candidate = static_cast<size_t>(24 * std::pow(2.0, n + 1));
            }
            else if (candidateTickFreq > x_maxTickFreq && n > 0)
            {
                // Try next lower power of 2
                //
                candidate = static_cast<size_t>(24 * std::pow(2.0, n - 1));
            }
            
            m_divisions = std::max<size_t>(24, candidate);
        }
    }
};
