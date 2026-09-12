#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "Trig.hpp"
#include "AHD.hpp"
#include "TheoryOfTimeBase.hpp"

struct MultiPhasorGateInternal
{
    static constexpr size_t x_maxPoly = 16;

    struct Input
    {
        bool m_trigs[x_maxPoly];
        TheoryOfTimeBase* m_theoryOfTime;
        double m_globalPeriodSamples;
        double m_phaseRatio;
        size_t m_numTrigs;
        int64_t m_voiceCycleRatio[x_maxPoly];
        bool m_newTrigCanStart[x_maxPoly];
        bool m_mute[x_maxPoly];

        Input()
            : m_theoryOfTime(nullptr)
            , m_globalPeriodSamples(1.0)
            , m_phaseRatio(1.0)
            , m_numTrigs(0)
        {
            for (size_t i = 0; i < x_maxPoly; ++i)
            {
                m_newTrigCanStart[i] = false;
                m_trigs[i] = false;
                m_voiceCycleRatio[i] = 1;
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
        double m_startGlobalPhase = 0.0;
        int64_t m_voiceCycleRatio = 1;
        double m_globalPeriodSamples = 1.0;

        void Set(double globalPhase, int64_t voiceCycleRatio, double globalPeriodSamples)
        {
            m_startGlobalPhase = globalPhase;
            m_voiceCycleRatio = voiceCycleRatio;
            m_globalPeriodSamples = globalPeriodSamples;
        }

        double Process(double globalPhase)
        {
            return std::abs(globalPhase - m_startGlobalPhase)
                * static_cast<double>(m_voiceCycleRatio);
        }

        double EnvelopePeriodSamples()
        {
            return m_globalPeriodSamples / static_cast<double>(m_voiceCycleRatio);
        }
    };

    bool m_anyGate;
    bool m_gate[x_maxPoly];
    bool m_preGate[x_maxPoly];
    bool m_set[x_maxPoly];
    PhasorBounds m_bounds[x_maxPoly];
    AHD::AHDControl m_ahdControl[x_maxPoly];

    void Process(Input& input)
    {
        double globalPhase = input.m_theoryOfTime->GetPhase(
            TheoryOfTimeBase::x_globalLoop,
            0.0,
            PhaseDomain::Modulated);

        m_anyGate = false;
        for (size_t i = 0; i < input.m_numTrigs; ++i)
        {
            m_ahdControl[i].m_phaseRatio = input.m_phaseRatio;
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
                }

                m_preGate[i] = true;
                m_set[i] = true;
                m_bounds[i].Set(globalPhase, input.m_voiceCycleRatio[i], input.m_globalPeriodSamples);
                m_ahdControl[i].m_envelopePeriodSamples = m_bounds[i].EnvelopePeriodSamples();
            }

            if (m_set[i])
            {
                double thisPhase = m_bounds[i].Process(globalPhase);

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
