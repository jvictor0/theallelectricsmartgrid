#pragma once

#include "Filter.hpp"
#include <random>

struct RandomLFO
{
    float m_trg;
    OPLowPassFilter m_filter;
    int m_sampleCount;
    int m_samplesToChange;
    static std::mt19937 s_rng;
    static std::uniform_real_distribution<float> s_distribution;

    RandomLFO()
        : m_trg(0)
        , m_sampleCount(0)
        , m_samplesToChange(64)
    {
        m_trg = s_distribution(s_rng);
        m_filter.SetAlphaFromNatFreq(0.05 / 48000);
        m_filter.m_output = m_trg;
    }

    float Process()
    {
        ++m_sampleCount;
        if (m_sampleCount >= m_samplesToChange)
        {
            m_trg = s_distribution(s_rng);
            m_sampleCount = 0;
        }

        m_filter.Process(m_trg);
        return m_filter.m_output;
    }
};

inline std::mt19937 RandomLFO::s_rng;
inline std::uniform_real_distribution<float> RandomLFO::s_distribution(0.0f, 1.0f);