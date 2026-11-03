#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "PositionalBufferRecorder.hpp"
#include "Filter.hpp"
#include "Saturator.hpp"
#include "Oversample.hpp"
#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
#include "TapeHead.hpp"
#include "SnapshotUIState.hpp"
#include "Q.hpp"

struct QuadDelay
{
    struct PostFeedbackFilter
    {
        QuadOPBaseWidthFilter m_bff;

        QuadFloat Process(QuadFloat input)
        {
            return m_bff.Process(input);
        }

        void SetBFFBaseWidth(QuadFloat base, QuadFloat width)
        {
            m_bff.SetBaseWidth(base, width);
        }
    };
    
    // The delay line and its positions are in host samples. The saturated input and feedback are decimated and written
    // once per host frame; grains play back at the engine rate.
    //
    static constexpr size_t x_oversample = SampleTimer::x_oversample;
    static constexpr size_t x_delayLineSize = 1 << 24;
    static constexpr size_t x_positionalBufferSize = 1024;
    using Writer = QuadDelayLineMovableWriter<x_delayLineSize>;
    using WriteDecimator = DownsamplerGeneric<float, 4, 8, x_oversample>;
    Writer m_delayLine;
    WriteDecimator m_writeDecimator;
    size_t m_writeSubSample;
    QuadGrainManager<Writer> m_grainManager;

    PositionalBufferRecorder<x_positionalBufferSize> m_positionalBufferRecorder;

    PostFeedbackFilter m_postFeedbackFilter;

    QuadTanhSaturator<true> m_saturator;

    QuadFloat m_output;

    QuadDelay()
        : m_writeSubSample(0)
        , m_grainManager(&m_delayLine)
        , m_saturator(0.5f)
        , m_output()
    {
    }
    
    struct Input
    {
        QuadFloat m_input;
        QuadFloat m_return;
        QuadFloat m_feedback;
        QuadFloat m_rotate;

        double m_writeHeadPosition;
        QuadDouble m_readHeadPosition;
        float m_relativeWriteHeadPosition;
        QuadFloat m_relativeReadHeadPosition;

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        QuadGrainManager<Writer>::Input m_grainManagerInput;

        Input()
            : m_feedback(0.5, 0.5, 0.5, 0.5)
            , m_rotate(0, 0, 0, 0)
            , m_writeHeadPosition(0)
            , m_relativeWriteHeadPosition(-1.0f)
            , m_relativeReadHeadPosition(-1.0f, -1.0f, -1.0f, -1.0f)
        {
        }

        QuadFloat TransformOutput(QuadFloat output)
        {
            return output.RotateLinear(m_rotate);
        }
    };

    struct DelayEnvelopeState
    {
        float m_maxEnvelope[x_positionalBufferSize];
        float m_minEnvelope[x_positionalBufferSize];
        float m_relativeReadHeadPosition;
        float m_relativeWriteHeadPosition;

        DelayEnvelopeState()
            : m_relativeReadHeadPosition(-1.0f)
            , m_relativeWriteHeadPosition(-1.0f)
        {
            for (int i = 0; i < x_positionalBufferSize; ++i)
            {
                m_maxEnvelope[i] = 0.0f;
                m_minEnvelope[i] = 0.0f;
            }
        }
    };

    struct UISnapshot
    {
        DelayEnvelopeState m_delayEnvelopeState[4];
    };

    struct UIState : SnapshotUIState<UISnapshot>
    {
        DampingFilter::UIState m_dampingFilter[4];
    };

    void PopulateUIState(UIState* uiState, QuadDelay::Input& input)
    {
        for (int i = 0; i < 4; ++i)
        {
            uiState->m_dampingFilter[i].m_hpAlpha.store(m_postFeedbackFilter.m_bff.m_highPassFilter.m_alpha[i]);
            uiState->m_dampingFilter[i].m_lpAlpha.store(m_postFeedbackFilter.m_bff.m_lowPassFilter.m_alpha[i]);
        }

        UISnapshot& snapshot = uiState->BeginSnapshot();
        for (int delayLineIdx = 0; delayLineIdx < 4; ++delayLineIdx)
        {
            auto& envelopeState = snapshot.m_delayEnvelopeState[delayLineIdx];
            envelopeState.m_relativeReadHeadPosition = input.m_relativeReadHeadPosition[delayLineIdx];
            envelopeState.m_relativeWriteHeadPosition = input.m_relativeWriteHeadPosition;
        }

        for (size_t bufIdx = 0; bufIdx < x_positionalBufferSize; ++bufIdx)
        {
            double warpedTime = m_positionalBufferRecorder.Get(bufIdx);
            double realTime = m_delayLine.GetRealTime(warpedTime);
            auto [maxEnv, minEnv] = m_delayLine.GetEnvelopeAtRealTime(realTime);
            for (int delayLineIdx = 0; delayLineIdx < 4; ++delayLineIdx)
            {
                auto& envelopeState = snapshot.m_delayEnvelopeState[delayLineIdx];
                envelopeState.m_maxEnvelope[bufIdx] = maxEnv[delayLineIdx];
                envelopeState.m_minEnvelope[bufIdx] = minEnv[delayLineIdx];
            }
        }

        uiState->CommitSnapshot();
    }

    QuadFloat Process(Input& input)
    {
        m_postFeedbackFilter.SetBFFBaseWidth(input.m_bffBase, input.m_bffWidth);

        QuadFloat qInput = input.m_input;
        QuadFloat retrn = input.m_return;
        qInput = qInput + retrn * input.m_feedback;
        qInput = m_saturator.Process(qInput);
        m_writeDecimator.Process(qInput);
        ++m_writeSubSample;
        if (m_writeSubSample == x_oversample)
        {
            m_writeSubSample = 0;
            m_delayLine.Write(m_writeDecimator.GetOutput(), input.m_writeHeadPosition);
        }

        QuadFloat delayedSignal = m_grainManager.Process(input.m_readHeadPosition, QuadFloat(), input.m_grainManagerInput, m_delayLine.m_lastTime - 1.0);

        m_output = m_postFeedbackFilter.Process(input.TransformOutput(delayedSignal));

        return m_output;
    }           
};

struct QuadDelayInputSetter
{
    using QuadExpParam = PhaseUtils::ExpParamGeneric<float, 4>;
    using QuadZeroedExpParam = PhaseUtils::ZeroedExpParamGeneric<float, 4>;

    QuadExpParam m_dampingBase;
    QuadExpParam m_dampingWidth;

    // Each channel widens over its own range, so the wideners stay per channel.
    //
    PhaseUtils::ExpParam m_wideners[4];

    QuadZeroedExpParam m_feedback;

    // Double state: 1 Hz one-poles at the engine rate stall short of their
    // targets in float.
    //

    OPLowPassFilterGeneric<double, 4> m_rotateFilter;

    QuadExpParam m_unisonDetune;
    QuadZeroedExpParam m_unisonGain;
    QuadExpParam m_slewUp;

    float m_bufferFrac[4];
    WriteTapeHead m_writeTapeHead;
    ReadTapeHead m_readTapeHead[4];

    struct Input
    {
        int m_delayTimeFactorSwitchVal[4];
        int m_readHeadSpeedSwitchVal[4];
        int m_loopSelectorSwitchVal[4];
        QuadFloat m_widenKnob;
        int m_rotateSwitchVal[4];
        QuadFloat m_feedbackKnob;
        QuadFloat m_dampingBaseKnob;
        QuadFloat m_dampingWidthKnob;

        QuadFloat m_resynthShiftFadeKnob;
        QuadFloat m_resynthUnisonKnob;
        int m_resynthShiftSwitchVal[4];
        QuadFloat m_resynthSlewUpKnob;
        
        TheoryOfTime* m_theoryOfTime;

        Input()
            : m_widenKnob()
            , m_feedbackKnob()
            , m_dampingBaseKnob()
            , m_dampingWidthKnob()
            , m_resynthShiftFadeKnob()
            , m_resynthUnisonKnob()
            , m_resynthSlewUpKnob()
            , m_theoryOfTime(nullptr)
        {
            for (int i = 0; i < 4; ++i)
            {
                m_delayTimeFactorSwitchVal[i] = 3;
                m_readHeadSpeedSwitchVal[i] = 10;
                m_loopSelectorSwitchVal[i] = 0;
                m_rotateSwitchVal[i] = 1;
                m_resynthShiftSwitchVal[i] = 9;
            }
        }
    };

    QuadDelayInputSetter()
      : m_dampingBase(SampleTimer::HzToCycPerSamp(DampingFilter::x_minBaseHz), SampleTimer::HzToCycPerSamp(DampingFilter::x_maxBaseHz))
      , m_dampingWidth(1.0, 2048.0)
      , m_wideners
      {
        PhaseUtils::ExpParam(1.0, 0.978615137775),
        PhaseUtils::ExpParam(1.0, 1.0),
        PhaseUtils::ExpParam(1.0, 0.986803398875),
        PhaseUtils::ExpParam(1.0, 0.989701964951)
      }
      , m_unisonDetune(1.03)
      , m_unisonGain(20)
      , m_slewUp(0.25 / Resynthesizer::GetGrainLaunchSamples(), 0.5)
    {
        m_rotateFilter.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        m_feedback.SetBaseByCenter(0.25);
        for (int i = 0; i < 4; ++i)
        {
            m_bufferFrac[i] = 1.0;
            m_readTapeHead[i].m_writeTapeHead = &m_writeTapeHead;
        }
    }

    void Process(Input& input, QuadDelay::Input& delayInput, QuadDelay* delay)
    {
        WriteTapeHead::Input writeInput;
        writeInput.m_theoryOfTime = input.m_theoryOfTime;
        writeInput.m_running = input.m_theoryOfTime->m_sample.m_running;
        writeInput.m_globalPeriodSamples = input.m_theoryOfTime->m_globalPeriodSamples / static_cast<double>(SampleTimer::x_oversample);
        m_writeTapeHead.Update(writeInput);

        double writeHeadPosition = m_writeTapeHead.m_actualPosition;
        if (std::abs(writeHeadPosition - delayInput.m_writeHeadPosition) >= 64)
        {
            INFO("writeHeadPosition: %f -> %f (diff %f) external loop", 
                delayInput.m_writeHeadPosition,
                writeHeadPosition, 
                std::abs(writeHeadPosition - delayInput.m_writeHeadPosition));
            INFO("time loop %d modulated %f unmodulated %f global %f ratio %lld",
                m_readTapeHead[0].m_loopSelector,
                input.m_theoryOfTime->GetPhase(m_readTapeHead[0].m_loopSelector, PhaseDomain::Modulated),
                input.m_theoryOfTime->GetPhase(m_readTapeHead[0].m_loopSelector, PhaseDomain::Unmodulated),
                input.m_theoryOfTime->GetPhase(TheoryOfTimeBase::x_globalLoop, PhaseDomain::Modulated),
                static_cast<long long>(input.m_theoryOfTime->GetCycleRatio(m_readTapeHead[0].m_loopSelector)));
        }

        delayInput.m_writeHeadPosition = writeHeadPosition;
        double writePhase = m_writeTapeHead.m_relativePosition;
        delayInput.m_relativeWriteHeadPosition = static_cast<float>(writePhase - std::floor(writePhase));

        QuadFloat rotate;
        for (int i = 0; i < 4; ++i)
        {
            rotate[i] = static_cast<float>(input.m_rotateSwitchVal[i]) / 4.0f;
        }

        delayInput.m_rotate = m_rotateFilter.Process(rotate.Convert<double>()).Convert<float>();
        delayInput.m_feedback = m_feedback.Update(input.m_feedbackKnob) * 1.25f;
        delayInput.m_bffBase = m_dampingBase.Update(input.m_dampingBaseKnob);
        delayInput.m_bffWidth = m_dampingWidth.Update(input.m_dampingWidthKnob);

        QuadFloat unisonDetune = m_unisonDetune.Update(input.m_resynthUnisonKnob);
        QuadFloat unisonGain = m_unisonGain.Update(QuadFloat::Min(QuadFloat::Splat(1.0f), input.m_resynthUnisonKnob * 10.0f));
        QuadFloat slewUp = m_slewUp.Update(QuadFloat::Splat(1.0f) - input.m_resynthSlewUpKnob);

        for (int i = 0; i < 4; ++i)
        {
            float widen = m_wideners[i].Update(input.m_widenKnob[i]);

            int totLoopSelector =
                TheoryOfTimeBase::x_numLoops - input.m_loopSelectorSwitchVal[i] - 1;

            if (input.m_theoryOfTime->CrossedCycleBoundary(m_readTapeHead[i].m_loopSelector, PhaseDomain::Modulated))
            {
                float possibleBufferFracs[5] = {0.8, 2.0/3.0, 1.0, 3.0/4.0, 5.0/8.0};
                m_bufferFrac[i] = possibleBufferFracs[input.m_delayTimeFactorSwitchVal[i]];
            }

            double possibleReadHeadSpeeds[16] =
            {
                -4.0,
                -3.0,
                -2.0,
                -3.0 / 2.0,
                -4.0 / 3.0,
                -1.0,
                -1.0 / 2.0,
                -1.0 / 4.0,
                1.0 / 4.0,
                1.0 / 2.0,
                1.0,
                4.0 / 3.0,
                3.0 / 2.0,
                2.0,
                3.0,
                4.0
            };

            ReadTapeHead::Input readInput;
            readInput.m_theoryOfTime = input.m_theoryOfTime;
            readInput.m_bufferFraction = m_bufferFrac[i] * widen;
            readInput.m_readHeadSpeed = possibleReadHeadSpeeds[input.m_readHeadSpeedSwitchVal[i]];
            readInput.m_requestedLoopSelector = totLoopSelector;
            m_readTapeHead[i].Update(readInput);
            delayInput.m_readHeadPosition[i] = m_readTapeHead[i].m_actualPosition;
            double readPhase = m_readTapeHead[i].m_relativePosition;
            delayInput.m_relativeReadHeadPosition[i] = static_cast<float>(readPhase - std::floor(readPhase));

            // Resynth parameters
            //
            Resynthesizer::Input& resynthInput = delayInput.m_grainManagerInput.m_resynthInput[i];

            Q possibleShifts[10] = 
            { 
                Q(1, 2) /* octave down*/, 
                Q(2, 3), /* fifth down */
                Q(3, 4), /* fourth down */
                Q(3, 5), /* minor third down */
                Q(4, 5), /* major third down */
                Q(5, 4), /* major third up */
                Q(5, 3), /* major sixth up */
                Q(4, 3), /* fourth up */
                Q(3, 2), /* fifth up */
                Q(2, 1) /* octave up */
            };

            resynthInput.m_shift[0] = possibleShifts[input.m_resynthShiftSwitchVal[i]];

            resynthInput.m_fade[0] = input.m_resynthShiftFadeKnob[i];
            resynthInput.m_unisonDetune = unisonDetune[i];
            resynthInput.m_unisonGain = unisonGain[i];
            resynthInput.m_slewUp = input.m_resynthSlewUpKnob[i] < 0.05f
                ? Resynthesizer::x_immediateSlewUp
                : slewUp[i];
        }

        if (delay)
        {
            double phase = input.m_theoryOfTime->GetPhase(TheoryOfTimeBase::x_globalLoop, PhaseDomain::Modulated);
            double x = phase - std::floor(phase);
            delay->m_positionalBufferRecorder.Record(x, delayInput.m_writeHeadPosition);
        }
    }
};
