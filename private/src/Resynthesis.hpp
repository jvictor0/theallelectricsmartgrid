#pragma once

#include "QuadUtils.hpp"
#include "AdaptiveWaveTable.hpp"

struct Resynthesizer
{
    struct Grain
    {
        float Process()
        {
            if (m_index < BasicWaveTable::x_tableSize)
            {
                float value = m_buffer.m_table[m_index] * (0.5 - 0.5 * m_windowTable->m_table[m_index]);
                ++m_index;
                return value;
            }
            else
            {
                m_running = false;
                return 0.0f;
            }        
        }

        void Start()
        {
            m_index = 0;
            m_running = true;
        }

        size_t m_index;
        BasicWaveTable m_buffer;
        const WaveTable* m_windowTable;
        bool m_running;
    };

    void Clear()
    {
        m_startTime = 0.0;
        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_synthesisPhase[i] = 0.0;
            m_analysisPhase[i] = 0.0;
        }
    }

    Resynthesizer()
        : m_leaderElector(this)
    {
        Clear();
    }

    void FixupPhases(DiscreteFourierTransform& dft, double startTime)
    {
        double deltaTime = startTime - m_startTime;
        m_startTime = startTime;

        if (std::abs(deltaTime) < 1e-12)
        {
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                dft.m_components[i] = std::polar(std::abs(dft.m_components[i]), m_synthesisPhase[i]);
            }

            return;
        }

        constexpr float twoPi = 2.0f * static_cast<float>(M_PI);
        constexpr float N = static_cast<float>(BasicWaveTable::x_tableSize);
        constexpr float H = N / static_cast<float>(x_hopDenom);
    
        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {    
            double phiCurr = std::arg(dft.m_components[i]);
            double phiPrev = m_analysisPhase[i];
            m_analysisPhase[i] = phiCurr;

            if (m_leaderElector.m_leader[i] != i)
            {
                continue;
            }
    
            double omegaBin = twoPi * static_cast<double>(i) / N;
            
            double deltaPhiExpected = omegaBin * deltaTime;
    
            double deltaPhi = phiCurr - phiPrev;
    
            deltaPhi -= deltaPhiExpected;
    
            deltaPhi = std::fmod(deltaPhi + static_cast<double>(M_PI), twoPi);
            if (deltaPhi <= 0.0f)
            {
                deltaPhi += twoPi;
            }

            deltaPhi -= static_cast<double>(M_PI);
    
            double omegaInstantaneous = omegaBin + deltaPhi / deltaTime;
            m_synthesisPhase[i] = m_synthesisPhase[i] + omegaInstantaneous * H;
        }

        for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            if (m_leaderElector.m_leader[i] != i)
            {
                float delta = m_analysisPhase[i] - m_analysisPhase[m_leaderElector.m_leader[i]];
                delta = std::fmod(delta + static_cast<float>(M_PI), twoPi);
                if (delta < 0.0f)
                {
                    delta += twoPi;
                }

                delta -= static_cast<float>(M_PI);
                m_synthesisPhase[i] = m_synthesisPhase[m_leaderElector.m_leader[i]] + delta;            
            }

            float mag = std::abs(dft.m_components[i]);
            dft.m_components[i] = std::polar(mag, m_synthesisPhase[i]);
        }
    }

    void Analyze(BasicWaveTable& buffer, DiscreteFourierTransform& dft)
    {
        dft.Transform(buffer);
        m_leaderElector.ProcessElection(dft);
    }

    void Synthesize(Grain* grain, DiscreteFourierTransform& dft, double startTime)
    {
        FixupPhases(dft, startTime);
        dft.InverseTransform(grain->m_buffer, DiscreteFourierTransform::x_maxComponents);
        grain->Start();
    }

    struct LeaderElector
    {
        LeaderElector(Resynthesizer* owner)
            : m_owner(owner)
        {
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                m_leader[i] = i;
                m_electionsWon[i] = 0;
                m_magnitudes[i] = 0.0f;
                m_clusterMagnitudes[i] = 0.0f;
            }

            m_magnitudeSmoothing = 0.75f;
            m_electionsToWin = 5;
            m_autoWinAmount = 2.0f;
            m_minSlope = 0.707;
            m_influenceRadius = 0;
        }

        void SetMagnitudes(DiscreteFourierTransform& dft)
        {
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                float newMag = 0.0f;
                int denom = 0;
                for (size_t j = std::max(0, static_cast<int>(i) - 1); j < std::min(DiscreteFourierTransform::x_maxComponents, i + 2); ++j)
                {
                    newMag += std::abs(dft.m_components[j]);
                    ++denom;
                }

                newMag /= denom;
                m_clusterMagnitudes[i] = newMag * (1.0f - m_magnitudeSmoothing) + m_clusterMagnitudes[i] * m_magnitudeSmoothing;
                m_magnitudes[i] = std::abs(dft.m_components[i]);
            }
        }

        bool IsLeaderCandidate(size_t index) const
        {
            if (0 < index)
            {
                if (m_clusterMagnitudes[index] < m_clusterMagnitudes[index - 1])
                {
                    return false;
                }

                if (m_magnitudes[index] < m_magnitudes[index - 1] * m_minSlope &&
                    (index == 1 || m_magnitudes[index] < m_magnitudes[index - 2] * m_minSlope))
                {
                    return false;
                }
            }
            
            if (index < DiscreteFourierTransform::x_maxComponents - 1)
            {
                if (m_clusterMagnitudes[index] < m_clusterMagnitudes[index + 1])
                {
                    return false;
                }

                if (m_magnitudes[index] < m_magnitudes[index + 1] * m_minSlope &&
                    (index == DiscreteFourierTransform::x_maxComponents - 2 || m_magnitudes[index] < m_magnitudes[index + 2] * m_minSlope))
                {
                    return false;
                }
            }

            return true;
        }

        void ProcessElection(DiscreteFourierTransform& dft)
        {
            SetMagnitudes(dft);

            int16_t leaders[DiscreteFourierTransform::x_maxComponents];
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                leaders[i] = -1;
            }
 
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)            
            {
                if (IsLeaderCandidate(i))
                {
                    ++m_electionsWon[i];
                    if (m_leader[i] == i || 
                        m_clusterMagnitudes[m_leader[i]] < m_clusterMagnitudes[i] * m_autoWinAmount ||
                        m_electionsToWin <= m_electionsWon[i])
                    {
                        leaders[i] = i;
                        m_electionsWon[i] = 0;
                    }
                }
                else
                {
                    m_electionsWon[i] = 0;
                }
            }

            for (int i = 0; i < static_cast<int>(DiscreteFourierTransform::x_maxComponents); ++i)
            {
                if (leaders[i] == -1)
                {
                    for (int j = 1; j <= m_influenceRadius; ++j)
                    {
                        bool lowerLeader = i - j >= 0 && leaders[i - j] != -1;
                        bool higherLeader = i + j < DiscreteFourierTransform::x_maxComponents && leaders[i + j] != -1;
                        if (lowerLeader && higherLeader && m_clusterMagnitudes[i - j] < m_clusterMagnitudes[i + j])
                        {
                            leaders[i] = i + j;
                            break;
                        }
                        else if (lowerLeader || higherLeader)
                        {
                            leaders[i] = lowerLeader ? i - j : i + j;
                            break;
                        }
                    }

                    if (leaders[i] == -1)
                    {
                        leaders[i] = i;
                    }
                }
            }
            
            for (size_t i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
            {
                m_leader[i] = leaders[i];
            }
        }            

        int16_t m_leader[DiscreteFourierTransform::x_maxComponents];
        uint8_t m_electionsWon[DiscreteFourierTransform::x_maxComponents];
        float m_clusterMagnitudes[DiscreteFourierTransform::x_maxComponents];
        float m_magnitudes[DiscreteFourierTransform::x_maxComponents];

        int m_influenceRadius;

        float m_magnitudeSmoothing;
        uint8_t m_electionsToWin;
        float m_minSlope;
        float m_autoWinAmount;

        Resynthesizer* m_owner;
    };

    static constexpr size_t x_hopDenom = 4;
    const WaveTable* m_windowTable;
    float m_synthesisPhase[DiscreteFourierTransform::x_maxComponents];
    float m_analysisPhase[DiscreteFourierTransform::x_maxComponents];
    double m_startTime;
    LeaderElector m_leaderElector;
};