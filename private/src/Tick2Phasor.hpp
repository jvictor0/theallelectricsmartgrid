#pragma once

#include <cmath>

struct Tick2Phasor
{
    static constexpr float x_alpha = 10;
    double m_phaseIncrement;
    double m_samplesSinceLastTick;
    double m_output;
    double m_prePhase;
    double m_tickStart;
    double m_tickEnd;
    uint64_t m_tickCount;
    bool m_reset;

    struct Input
    {
        bool m_tick;
        bool m_reset;
        int m_ticksPerPhase;

        Input()
        {
            m_tick = false;
            m_reset = false;
            m_ticksPerPhase = 24 * 4 * 4;
        }
    };

    Tick2Phasor()
    {
        m_phaseIncrement = 0;
        m_samplesSinceLastTick = 1;
        m_output = 0;
        m_prePhase = 0;
        m_tickCount = 0;
        m_tickStart = 0;
        m_tickEnd = 1;
    }

    void Reset()
    {
        m_reset = false;
        m_tickCount = 0;
        m_prePhase = 0;
        m_tickStart = 0;
        m_tickEnd = 1;
        m_output = 0;
    }

    void ComputeOutput(const Input& input)
    {
        double phase = m_prePhase / std::pow(1 + std::pow(m_prePhase, x_alpha), 1.0 / x_alpha);
        m_output = m_tickStart + phase * (m_tickEnd - m_tickStart);
    }

    void Process(const Input& input)
    {
        if (input.m_reset)
        {
            m_reset = true;
        }

        if (input.m_tick)
        {
            if (m_reset)
            {
                Reset();
            }
            else
            {
                m_prePhase += m_phaseIncrement;            
                ComputeOutput(input);

                m_tickCount++;
                m_prePhase = 0;
            }

            m_phaseIncrement = 1.0f / m_samplesSinceLastTick;
            m_samplesSinceLastTick = 0;
            m_tickStart = m_output;
            m_tickEnd = static_cast<double>(m_tickCount % input.m_ticksPerPhase) / input.m_ticksPerPhase;
        }
        else
        {
            m_prePhase += m_phaseIncrement;
            ++m_samplesSinceLastTick;
        }

        ComputeOutput(input);
    }
};

struct Phasor2Tick
{
    // MIDI clock: 24 pulses per quarter note (PPQN)
    // 90 BPM = 36 Hz = 36/48000 cycles per sample
    // 180 BPM = 72 Hz = 72/48000 cycles per sample
    // Target: 135 BPM = 54 Hz = 54/48000 cycles per sample (middle of range)
    //
    static constexpr double x_sampleRate = 48000.0;
    static constexpr double x_targetTickFreq = 54.0 / x_sampleRate;  // 135 BPM * 24 PPQN / 60 / sampleRate
    static constexpr double x_minTickFreq = 36.0 / x_sampleRate;     // 90 BPM * 24 PPQN / 60 / sampleRate
    static constexpr double x_maxTickFreq = 72.0 / x_sampleRate;     // 180 BPM * 24 PPQN / 60 / sampleRate

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