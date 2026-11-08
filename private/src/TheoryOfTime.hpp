#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>

#include "Phasor2Tick.hpp"
#include "TheoryOfTimeBase.hpp"
#include "PolyXFader.hpp"
#include "ScopeWriter.hpp"
#include "PhaseUtils.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "MessageOut.hpp"
#include "SampleTimer.hpp"
#include "TheoryOfTimeBaseUIState.hpp"
#include "TimeWarpInverter.hpp"
#include "WriteTapeHead.hpp"

struct TheoryOfTime : public TheoryOfTimeBase
{
    // The delay write head's inverse in engine samples, covering 2^24 host
    // samples of delay.
    //
    static constexpr size_t x_timeWarpInverterSamples = SampleTimer::x_oversample << 24;
    using Inverter = TimeWarpInverter<x_timeWarpInverterSamples>;

    Phasor2Tick m_phasor2Tick;
    SmartGrid::MessageOutBuffer* m_messageOutBuffer;
    PolyXFaderInternal m_phaseModLFO;
    ScopeWriterHolder m_scopeWriter;
    WriteTapeHead m_writeTapeHead;
    std::unique_ptr<Inverter> m_timeWarpInverter;

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

            double paramFilterFreq = SampleTimer::HzToCycPerSamp(0.8);
            m_lfoSkewFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoMultFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoShapeFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoCenterFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoSlopeFilter.SetAlphaFromNatFreq(paramFilterFreq);
            m_lfoIndexFilter.SetAlphaFromNatFreq(paramFilterFreq);
        }

        // Double state: 0.8 Hz one-poles at the engine rate stall short of
        // their targets in float.
        //
        OPLowPassFilterDouble m_lfoSkewFilter;
        OPLowPassFilterDouble m_lfoMultFilter;
        OPLowPassFilterDouble m_lfoShapeFilter;
        OPLowPassFilterDouble m_lfoCenterFilter;
        OPLowPassFilterDouble m_lfoSlopeFilter;
        OPLowPassFilterDouble m_lfoIndexFilter;

        double m_freq;
        PolyXFaderInternal::Input m_phaseModLFOInput;
        PhaseUtils::ZeroedExpParam m_modIndex;
        PhaseUtils::ExpParam m_lfoMult;
    };

    struct UIState : public TheoryOfTimeBaseUIState
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

    void PopulateUIState(UIState* uiState, const Input& input)
    {
        TheoryOfTimeBase::PopulateUIState(*uiState, input);

        int64_t lfoPeriod = 1;
        for (size_t i = 0; i < x_numLoops; ++i)
        {
            if (m_phaseModLFO.m_weights[i] > 0)
            {
                lfoPeriod = std::lcm(lfoPeriod, GetPeriodTicks(i));
            }
        }

        uiState->SetTimeYModAmount(GetPeriodTicks(x_globalLoop) / lfoPeriod);
    }

    // Reads the previous sample, which m_sample still holds until Process
    // advances it.
    //
    void ProcessPhaseModLFO(Input& input)
    {
        for (size_t i = 0; i < x_numLoops; ++i)
        {
            input.m_phaseModLFOInput.m_externalWeights[i] = 1.0 / static_cast<double>(GetCycleRatio(i));
        }

        PolyXFaderInternal::Input lfoInput = input.m_phaseModLFOInput;
        lfoInput.m_attackFrac = 0.1f + 0.8f * lfoInput.m_attackFrac;
        lfoInput.m_mult = input.m_lfoMult.m_expParam;
        lfoInput.m_theoryOfTime = this;
        lfoInput.m_phaseDomain = PhaseDomain::Unmodulated;
        m_phaseModLFO.Process(lfoInput);
        input.m_phaseOffset = -2 * input.m_modIndex.m_expParam * m_phaseModLFO.m_rawOutput;
    }

    void SetupMonoScopeWriter(ScopeWriterInput<1>* scopeWriter)
    {
        m_scopeWriter = ScopeWriterHolder(scopeWriter, static_cast<size_t>(SmartGridOne::MonoScopes::TheoryOfTime));
    }

    void SetupMessageOutBuffer(SmartGrid::MessageOutBuffer* messageOutBuffer)
    {
        m_messageOutBuffer = messageOutBuffer;
    }

    TheoryOfTime()
        : m_messageOutBuffer(nullptr)
        , m_timeWarpInverter(std::make_unique<Inverter>())
    {
    }

    // Advances the delay write head from the current modulated phase and records
    // it in the inverter. Process calls this every engine sample.
    //
    void ProcessWriteTapeHead()
    {
        WriteTapeHead::Input writeInput;
        writeInput.m_theoryPhase = GetPhase(x_globalLoop, PhaseDomain::Modulated);
        writeInput.m_running = m_sample.m_running;
        writeInput.m_globalPeriodSamples = m_globalPeriodSamples;
        m_writeTapeHead.Update(writeInput);
        m_timeWarpInverter->Write(m_writeTapeHead.m_actualPosition, static_cast<double>(SampleTimer::GetSample()));
    }

    void Process(Input& input)
    {
        if (input.m_running)
        {
            input.m_unmodulatedPhase += input.m_freq;
        }
        else
        {
            input.m_unmodulatedPhase = 0;
        }

        m_globalPeriodSamples = 1.0 / input.m_freq;
        bool wasRunning = m_sample.m_running;

        ProcessPhaseModLFO(input);
        TheoryOfTimeBase::Process(input);
        ProcessWriteTapeHead();

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
            if (m_phasor2Tick.Process(GetPhase(x_globalLoop, PhaseDomain::Unmodulated)) && m_messageOutBuffer)
            {
                m_messageOutBuffer->Push(SmartGrid::MessageOut::Clock());
            }
        }

        if (m_scopeWriter.IsWriteSample())
        {
            double phase = GetPhase(x_globalLoop, PhaseDomain::Modulated);
            m_scopeWriter.Write(MultiChannelNumber<float, 1>(phase - std::floor(phase)));
        }

        if (m_phaseModLFO.m_top[0])
        {
            m_scopeWriter.RecordStart(m_phaseModLFO.m_top[0].m_offset);
        }
    }

    void PrintState()
    {
        for (size_t i = 0; i < x_numLoops; ++i)
        {
            const TimeLoop& loop = GetLoop(i);
            INFO("time loop %zu unmodulated %f modulated %f ratio %lld period %lld gate %d",
                i,
                GetPhase(i, PhaseDomain::Unmodulated),
                GetPhase(i, PhaseDomain::Modulated),
                static_cast<long long>(loop.m_cycleRatio),
                static_cast<long long>(loop.m_periodTicks), loop.m_gate);
        }
    }
};
