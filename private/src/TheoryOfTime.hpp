#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

#include "Phasor2Tick.hpp"
#include "TheoryOfTimeBase.hpp"
#include "PolyXFader.hpp"
#include "ScopeWriter.hpp"
#include "PhaseUtils.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "MessageOut.hpp"
#include "SampleTimer.hpp"

struct TheoryOfTime : public TheoryOfTimeBase
{
    Phasor2Tick m_phasor2Tick;
    SmartGrid::MessageOutBuffer* m_messageOutBuffer;
    PolyXFaderInternal m_phaseModLFO;
    ScopeWriterHolder m_scopeWriter;
    double m_globalPeriodSamples;

    struct Input : public TheoryOfTimeBase::Input
    {
        Input()
            : m_freq(1.0 / 4)
            , m_lfoMult(1, 16)
        {
            m_phaseModLFOInput.m_size = x_numLoops;
            m_phaseModLFOInput.m_theoryOfTime = nullptr;
            m_phaseModLFOInput.m_phaseDomain = PhaseDomain::Unmodulated;
            m_phaseModLFOInput.m_phaseShift = -0.75;

            m_modIndex.SetBaseByCenter(1.0 / 16);

            float paramFilterFreq = 0.1 / 48000.0 * SampleTimer::x_controlFrameRate;
            m_lfoSkewFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoMultFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoShapeFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoCenterFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoSlopeFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoIndexFilter.SetAlphaFromNatFreq(paramFilterFreq);
        }

        OPLowPassFilter m_lfoSkewFilter;
        OPLowPassFilter m_lfoMultFilter;
        OPLowPassFilter m_lfoShapeFilter;
        OPLowPassFilter m_lfoCenterFilter;
        OPLowPassFilter m_lfoSlopeFilter;
        OPLowPassFilter m_lfoIndexFilter;

        double m_freq;
        PolyXFaderInternal::Input m_phaseModLFOInput;
        PhaseUtils::ZeroedExpParam m_modIndex;
        PhaseUtils::ExpParam m_lfoMult;
    };

    struct UIState
    {
        std::atomic<int> m_timeYModAmount;

        double GetTimeYModAmount()
        {
            return m_timeYModAmount.load();
        }

        void SetTimeYModAmount(double value)
        {
            m_timeYModAmount.store(value);
        }
    };

    void PopulateUIState(UIState* uiState)
    {
        int64_t lfoPeriod = 1;
        for (size_t i = 0; i < x_numLoops; ++i)
        {
            if (m_phaseModLFO.m_weights[i] > 0)
            {
                lfoPeriod = std::lcm(lfoPeriod, GetPeriodTicks(i, 0));
            }
        }

        uiState->m_timeYModAmount.store(static_cast<int>(GetPeriodTicks(x_globalLoop, 0) / lfoPeriod));
    }

    void ProcessPhaseModLFO(size_t j, Input& input)
    {
        for (size_t i = 0; i < x_numLoops; ++i)
        {
            input.m_phaseModLFOInput.m_externalWeights[i] = 1.0 / static_cast<double>(GetCycleRatio(i, j - 1));
        }

        input.m_phaseModLFOInput.m_mult = input.m_lfoMult.m_expParam;
        input.m_phaseModLFOInput.m_theoryOfTime = this;
        input.m_phaseModLFOInput.m_phaseDomain = PhaseDomain::Unmodulated;
        input.m_phaseModLFOInput.m_samplePosition = static_cast<float>(j - 1);
        m_phaseModLFO.Process(input.m_phaseModLFOInput);
        input.m_phaseOffset = -2 * input.m_modIndex.m_expParam * m_phaseModLFO.m_rawOutput;
    }

    void SetupMonoScopeWriter(ScopeWriter* scopeWriter)
    {
        m_scopeWriter = ScopeWriterHolder(scopeWriter, 0, static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime));
    }

    void SetupMessageOutBuffer(SmartGrid::MessageOutBuffer* messageOutBuffer)
    {
        m_messageOutBuffer = messageOutBuffer;
    }

    TheoryOfTime()
        : m_messageOutBuffer(nullptr)
    {
        m_globalPeriodSamples = 1.0;
    }

    void Process(size_t j, Input& input)
    {
        assert(j > 0 && j < x_microBlockBufferSize);
        if (input.m_running)
        {
            input.m_unmodulatedPhase += input.m_freq;
        }
        else
        {
            input.m_unmodulatedPhase = 0;
        }

        m_globalPeriodSamples = 1.0 / input.m_freq;
        bool wasRunning = m_samples[j - 1].m_running;

        ProcessPhaseModLFO(j, input);
        TheoryOfTimeBase::Process(j, input);

        if (wasRunning != input.m_running)
        {
            if (input.m_running)
            {
                m_phasor2Tick.UpdateDivisions(input.m_freq);
                if (m_messageOutBuffer)
                {
                    m_messageOutBuffer->Push(SmartGrid::MessageOut::Start());
                }
            }
            else if (m_messageOutBuffer)
            {
                m_messageOutBuffer->Push(SmartGrid::MessageOut::Stop());
            }
        }

        if (input.m_running)
        {
            if (m_phasor2Tick.Process(GetPhase(x_globalLoop, 0, PhaseDomain::Unmodulated)) && m_messageOutBuffer)
            {
                m_messageOutBuffer->Push(SmartGrid::MessageOut::Clock());
            }
        }

        double phase = GetPhase(x_globalLoop, j, PhaseDomain::Modulated);
        m_scopeWriter.Write(j, phase - std::floor(phase));
        if (m_phaseModLFO.m_top)
        {
            m_scopeWriter.RecordStart(j);
        }
    }

    void PrintState(size_t sampleIndex)
    {
        for (size_t i = 0; i < x_numLoops; ++i)
        {
            const TimeLoop& loop = GetLoop(i, sampleIndex);
            INFO("time sample %zu loop %zu unmodulated %f modulated %f ratio %lld period %lld gate %d",
                sampleIndex, i,
                GetPhase(i, sampleIndex, PhaseDomain::Unmodulated),
                GetPhase(i, sampleIndex, PhaseDomain::Modulated),
                static_cast<long long>(loop.m_cycleRatio),
                static_cast<long long>(loop.m_periodTicks), loop.m_gate);
        }
    }
};
