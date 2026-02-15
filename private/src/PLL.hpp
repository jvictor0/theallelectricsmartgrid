#pragma once
#include "PhaseUtils.hpp"

struct PLL
{
    struct Input
    {
        PhaseUtils::ExpParam m_phaseLearnRate;
        PhaseUtils::ExpParam m_freqLearnRate;
        PhaseUtils::ExpParam m_phaseLearnApplicationRate;
        PhaseUtils::ExpParam m_freqLearnApplicationRate;

        Input()
            : m_phaseLearnRate(0.001f, 1.0f)
            , m_freqLearnRate(0.0005f, 1.0f)
            , m_phaseLearnApplicationRate(0.000001f, 0.0001f)
            , m_freqLearnApplicationRate(0.000001f, 0.0001f)
        {
        }
    };

    double m_phase;
    double m_freq;
    double m_phaseError;
    double m_freqError;
    int64_t m_samplesSinceLastHit;
    double m_lastSampleEstimatedPhase;
    double m_lastSampleActualPhase;

    PLL()
    {
        m_phase = 0;
        m_freq = 0;
        m_phaseError = 0;
        m_freqError = 0;
        m_samplesSinceLastHit = 0;
        m_lastSampleEstimatedPhase = 0;
        m_lastSampleActualPhase = 0;
    }

    void Process(const Input& input)
    {
        m_freq += m_freqError * input.m_freqLearnApplicationRate.m_expParam;
        m_freqError = (1 - input.m_freqLearnApplicationRate.m_expParam) * m_freqError;

        m_phase += m_freq;
        m_phase += m_phaseError * input.m_phaseLearnApplicationRate.m_expParam;
        m_phaseError = (1 - input.m_phaseLearnApplicationRate.m_expParam) * m_phaseError;

        ++m_samplesSinceLastHit;
    }

    void ProcessHit(const Input& input, int64_t division)
    {
        double estimatedPhase = std::round(m_phase * division) / division;
        double error = m_phase - estimatedPhase;

        m_phaseError = error * input.m_phaseLearnRate.m_expParam;
        m_freqError = error * input.m_freqLearnRate.m_expParam;

        m_samplesSinceLastHit = 0;
        m_lastSampleEstimatedPhase = estimatedPhase;
        m_lastSampleActualPhase = m_phase;
    }
};
