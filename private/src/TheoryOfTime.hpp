#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

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

struct TheoryOfTime : public TheoryOfTimeBase
{
    // The audio ring retains 2^24 host samples independently of the phase grid.
    // Four master cycles at 2^18 cells/cycle use 32 MiB including the shared
    // fixed fallback/replacement traversals. This does not band-limit the LFO.
    //
    static constexpr size_t x_timeWarpInverterSamples = SampleTimer::x_oversample << 24;
    static constexpr double x_phaseResolution = 1 << 18;
    static constexpr size_t x_phaseHistoryCells = 1 << 20;
    using Inverter = TimeWarpInverter<x_phaseHistoryCells>;
    static_assert(Inverter::x_payloadBytes == (32 << 20));

    // The unwarped scope needs one period, at most one master cycle. MIDI
    // clock corrections can move backwards, so it also retains branch data.
    // Two cycles with fixed cell storage occupy 16 MiB.
    //
    using UnmodulatedInverter = TimeWarpInverter<1 << 19>;

    Phasor2Tick m_phasor2Tick;
    SmartGrid::MessageOutBuffer* m_messageOutBuffer;
    PolyXFaderInternal m_phaseModLFO;
    ScopeWriterHolder m_scopeWriter;
    std::unique_ptr<Inverter> m_timeWarpInverter;
    PhaseClock m_unmodulatedPhaseClock;
    std::unique_ptr<UnmodulatedInverter> m_unmodulatedPhaseInverter;

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

    using UIState = TheoryOfTimeBaseUIState;

    void PopulateUIState(UIState* uiState, const Input& input)
    {
        TheoryOfTimeBase::PopulateUIState(*uiState, input);
        m_scopeWriter.SetPeriodicAtVoice(0,
            {m_unmodulatedPhaseClock.m_offset, m_phaseModLFO.ComputeLogicalPeriods(*this)[0]});
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
        scopeWriter->SetTimeWarp(m_scopeWriter.m_scopeIx, m_unmodulatedPhaseInverter.get());
    }

    void SetupMessageOutBuffer(SmartGrid::MessageOutBuffer* messageOutBuffer)
    {
        m_messageOutBuffer = messageOutBuffer;
    }

    TheoryOfTime()
        : m_messageOutBuffer(nullptr)
        , m_timeWarpInverter(std::make_unique<Inverter>(x_phaseResolution))
        , m_unmodulatedPhaseInverter(std::make_unique<UnmodulatedInverter>(x_phaseResolution))
    {
    }

    // Record the phase clock already advanced by TheoryOfTimeBase::Process.
    //
    void ObservePhase()
    {
        m_timeWarpInverter->Write(m_phaseClock.m_phase, static_cast<double>(SampleTimer::GetSample()));
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
        ObservePhase();
        m_unmodulatedPhaseClock.Update(m_sample.m_unmodulatedPhase,
            input.m_running, GetUnmodulatedGlobalPhaseIncrement());
        m_unmodulatedPhaseInverter->Write(m_unmodulatedPhaseClock.m_phase,
            static_cast<double>(SampleTimer::GetSample()));

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
            // Store the bounded displacement, not a wrapped phase. The UI
            // adds its unwarped coordinate before wrapping the drawn value.
            //
            m_scopeWriter.Write(MultiChannelNumber<float, 1>(
                m_sample.m_modulatedPhase - m_sample.m_unmodulatedPhase));
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
