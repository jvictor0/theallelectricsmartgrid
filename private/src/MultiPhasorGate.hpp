#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"
#include "CircleTracker.hpp"
#include "AHD.hpp"

struct MultiPhasorGateInternal
{
    static constexpr size_t x_maxPoly = 16;

    struct Input
    {
        bool m_trigs[x_maxPoly];
        float m_phasor;
        double m_masterLoopSamples;
        size_t m_numTrigs;
        int m_phasorDenominator[x_maxPoly];
        bool m_newTrigCanStart[x_maxPoly];
        bool m_mute[x_maxPoly];

        Input()
            : m_numTrigs(0)
        {
            for (size_t i = 0; i < x_maxPoly; ++i)
            {
                m_newTrigCanStart[i] = false;
                m_trigs[i] = false;
                m_phasor = 0;
                m_phasorDenominator[i] = 1;
                m_mute[i] = false;
            }
        }
    };
    
    MultiPhasorGateInternal()
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_gate[i] = false;
            m_set[i] = false;
            m_preGate[i] = false;
            m_phasorDenominator[i] = 1;
        }
    }

    void Reset()
    {
        for (size_t i = 0; i < x_maxPoly; ++i)
        {
            m_gate[i] = false;
            m_preGate[i] = false;
            m_ahdControl[i].Reset();
            m_ahdControl[i].m_release = true;
        }
    }
    
    struct PhasorBounds
    {
        int m_phasorDenom;
        CircleDistanceTracker m_circleDistanceTracker;
        
        void Set(float phasor, int phasorDenominator)
        {
            m_phasorDenom = phasorDenominator;
            m_circleDistanceTracker.Reset(phasor);
        }

        float Process(float phase)
        {
            m_circleDistanceTracker.Process(phase);
            return GetPhase();
        }

        float GetPhase()
        {
            return m_circleDistanceTracker.Distance() * m_phasorDenom;
        }
    };

    bool m_anyGate;
    bool m_gate[x_maxPoly];
    bool m_preGate[x_maxPoly];
    bool m_set[x_maxPoly];
    PhasorBounds m_bounds[x_maxPoly];
    int m_phasorDenominator[x_maxPoly];
    AHD::AHDControl m_ahdControl[x_maxPoly];

    void Process(Input& input)
    {
        m_anyGate = false;
        for (size_t i = 0; i < input.m_numTrigs; ++i)
        {
            m_ahdControl[i].m_trig = input.m_trigs[i] && input.m_newTrigCanStart[i] && !input.m_mute[i];
            if (m_ahdControl[i].m_trig)
            {
                m_ahdControl[i].m_release = false;
            }

            if (input.m_trigs[i] && input.m_newTrigCanStart[i])
            {
                if (!input.m_mute[i])
                {
                    m_gate[i] = true;
                    m_ahdControl[i].m_samples = 0.0;
                }

                m_preGate[i] = true;
                m_set[i] = true;
                m_bounds[i].Set(input.m_phasor, input.m_phasorDenominator[i]);
            }

            // Compute envelopeTimeSamples for this voice
            //
            double envelopeTimeSamples = input.m_masterLoopSamples / static_cast<double>(input.m_phasorDenominator[i]);
            m_ahdControl[i].m_envelopeTimeSamples = envelopeTimeSamples;

            if (m_set[i])
            {
                float thisPhase = m_bounds[i].Process(input.m_phasor);

                if (0.5 <= thisPhase)
                {
                    m_gate[i] = false;
                    m_preGate[i] = false;
                    if (!input.m_newTrigCanStart[i] || input.m_mute[i])
                    {
                        m_ahdControl[i].m_release = true;
                        m_set[i] = false;
                    }
                }

                m_ahdControl[i].m_samples = static_cast<double>(thisPhase) * envelopeTimeSamples;
            }

            if (m_gate[i])
            {
                m_anyGate = true;
            }
        }
    }    

    struct NonagonTrigLogic
    {
        static constexpr size_t x_numVoices = 9;
        static constexpr size_t x_numTrios = 3;
        static constexpr size_t x_voicesPerTrio = x_numVoices / x_numTrios;

        bool m_pitchChanged[x_numVoices];
        bool m_earlyMuted[x_numVoices];
        bool m_subTrigger[x_numVoices];
        bool m_mute[x_numVoices];

        bool m_trigOnSubTrigger[x_numTrios];
        bool m_trigOnPitchChanged[x_numTrios];

        bool m_interrupt[x_numTrios][x_numTrios];

        int m_unisonMaster[x_numTrios];

        bool m_running;

        bool IsUnisonMaster(size_t voice)
        {
            return m_unisonMaster[voice / x_voicesPerTrio] == -1 || m_unisonMaster[voice / x_voicesPerTrio] == voice;
        }

        NonagonTrigLogic()
            : m_running(false)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_pitchChanged[i] = false;
                m_earlyMuted[i] = false;
                m_subTrigger[i] = false;
                m_mute[i] = false;
            }

            for (size_t i = 0; i < x_numTrios; ++i)
            {
                m_trigOnSubTrigger[i] = false;
                m_trigOnPitchChanged[i] = true;
                m_unisonMaster[i] = -1;

                for (size_t j = 0; j < x_numTrios; ++j)
                {
                    m_interrupt[i][j] = false;
                }
            }
        }

        void SetInput(Input& input)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                size_t trioId = i / x_voicesPerTrio;
                size_t ixToCheck = m_unisonMaster[trioId] == -1 ? i : m_unisonMaster[trioId];
                input.m_mute[i] = m_mute[i];
                input.m_trigs[i] = (m_trigOnSubTrigger[trioId] && m_subTrigger[ixToCheck]) || 
                                   (m_trigOnPitchChanged[trioId] && m_pitchChanged[ixToCheck]);
                input.m_trigs[i] &= !m_earlyMuted[ixToCheck];
                input.m_newTrigCanStart[i] = m_running && !m_earlyMuted[ixToCheck];


                for (size_t j = 0; j < i; ++j)
                {
                    size_t jTrioId = j / x_voicesPerTrio;
                    if (jTrioId == trioId && m_unisonMaster[trioId] != -1)
                    {
                        continue;
                    }

                    if (m_interrupt[trioId][jTrioId] && input.m_trigs[j] && !input.m_mute[j])
                    {
                        input.m_trigs[i] = false;
                    }
                }
            }
        }
    };
};
