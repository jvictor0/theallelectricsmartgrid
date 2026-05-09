#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "PositionalBufferRecorder.hpp"
#include "QuadLFO.hpp"
#include "Filter.hpp"
#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
#include "TapeHead.hpp"
#include "Q.hpp"
#include <atomic>
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
    
    static constexpr size_t x_delayLineSize = 1 << 24;
    static constexpr size_t x_positionalBufferSize = 1024;
    QuadDelayLineMovableWriter<x_delayLineSize> m_delayLine;
    QuadGrainManager<DelayLineMovableWriter<x_delayLineSize>> m_grainManager;
    QuadLFO m_lfo;

    PositionalBufferRecorder<x_positionalBufferSize> m_positionalBufferRecorder[4];

    PostFeedbackFilter m_postFeedbackFilter;

    TanhSaturator<true> m_saturator;

    QuadFloat m_output;

    QuadDelay()
        : m_grainManager(&m_delayLine)
        , m_saturator(0.5f)
        , m_output()
    {
        m_lfo.SetSlew(10.0 / 48000.0);
    }
    
    struct Input
    {
        QuadFloat m_input;
        QuadFloat m_return;
        QuadFloat m_feedback;
        QuadFloat m_rotate;
        QuadFloat m_modDepth;

        QuadDouble m_writeHeadPosition;
        QuadDouble m_readHeadPosition;
        QuadFloat m_relativeWriteHeadPosition;
        QuadFloat m_relativeReadHeadPosition;

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        QuadLFO::Input m_lfoInput;

        QuadGrainManager<DelayLineMovableWriter<x_delayLineSize>>::Input m_grainManagerInput;

        Input()
            : m_feedback(0.5, 0.5, 0.5, 0.5)
            , m_rotate(0, 0, 0, 0)
            , m_relativeWriteHeadPosition(-1.0f, -1.0f, -1.0f, -1.0f)
            , m_relativeReadHeadPosition(-1.0f, -1.0f, -1.0f, -1.0f)
        {
        }

        QuadFloat TransformOutput(QuadFloat output)
        {
            return output.RotateLinear(m_rotate);
        }
    };

    struct UIState
    {
        static constexpr int x_delayEnvelopeSlotCount = 10;

        DampingFilter::UIState m_dampingFilter[4];

        int GetCurrent() const
        {
            return (m_which.load(std::memory_order_acquire) + x_delayEnvelopeSlotCount - 1) % x_delayEnvelopeSlotCount;
        }

        struct DelayEnvelopeState
        {
            float m_maxEnvelope[x_positionalBufferSize];
            float m_minEnvelope[x_positionalBufferSize];
            std::atomic<float> m_relativeReadHeadPosition{-1.0f};
            std::atomic<float> m_relativeWriteHeadPosition{-1.0f};

            DelayEnvelopeState()
            {
                for (int i = 0; i < x_positionalBufferSize; ++i)
                {
                    m_maxEnvelope[i] = 0.0f;
                    m_minEnvelope[i] = 0.0f;
                }
            }
        };

        DelayEnvelopeState m_delayEnvelopeState[4][x_delayEnvelopeSlotCount];
        std::atomic<int> m_which{0};
    };

    void PopulateUIState(UIState* uiState, QuadDelay::Input& input)
    {
        for (int i = 0; i < 4; ++i)
        {
            uiState->m_dampingFilter[i].m_hpAlpha.store(m_postFeedbackFilter.m_bff.m_filters[i].m_highPassFilter.m_alpha);
            uiState->m_dampingFilter[i].m_lpAlpha.store(m_postFeedbackFilter.m_bff.m_filters[i].m_lowPassFilter.m_alpha);
        }

        int which = uiState->m_which.load(std::memory_order_relaxed);
        for (int delayLineIdx = 0; delayLineIdx < 4; ++delayLineIdx)
        {
            auto& envelopeState = uiState->m_delayEnvelopeState[delayLineIdx][which];
            envelopeState.m_relativeReadHeadPosition.store(input.m_relativeReadHeadPosition[delayLineIdx], std::memory_order_release);
            envelopeState.m_relativeWriteHeadPosition.store(input.m_relativeWriteHeadPosition[delayLineIdx], std::memory_order_release);

            auto& recorder = m_positionalBufferRecorder[delayLineIdx];
            auto& delayLine = m_delayLine.m_delayLine[delayLineIdx];

            for (size_t bufIdx = 0; bufIdx < x_positionalBufferSize; ++bufIdx)
            {
                double warpedTime = recorder.Get(bufIdx);
                double realTime = delayLine.GetRealTime(warpedTime);
                auto [maxEnv, minEnv] = delayLine.GetEnvelopeAtRealTime(realTime);
                envelopeState.m_maxEnvelope[bufIdx] = maxEnv;
                envelopeState.m_minEnvelope[bufIdx] = minEnv;
            }
        }

        uiState->m_which.store((which + 1) % UIState::x_delayEnvelopeSlotCount, std::memory_order_release);
    }

    QuadFloat Process(Input& input)
    {
        m_lfo.Process(input.m_lfoInput);
        m_postFeedbackFilter.SetBFFBaseWidth(input.m_bffBase, input.m_bffWidth);

        QuadFloat qInput = input.m_input;
        QuadFloat retrn = input.m_return;
        qInput = qInput + retrn * input.m_feedback;
        qInput = m_saturator.Process(qInput);
        m_delayLine.Write(qInput, input.m_writeHeadPosition);

        QuadFloat sampleOffset = m_lfo.m_output * 1000 * input.m_modDepth;
        QuadFloat delayedSignal = m_grainManager.Process(input.m_readHeadPosition, sampleOffset, input.m_grainManagerInput);

        m_output = m_postFeedbackFilter.Process(input.TransformOutput(delayedSignal));

        return m_output;
    }           
};

struct QuadDelayInputSetter
{
    PhaseUtils::ExpParam m_dampingBase[4];
    PhaseUtils::ExpParam m_dampingWidth[4];
    PhaseUtils::ExpParam m_modFreq[4];

    PhaseUtils::ExpParam m_wideners[4];
    PhaseUtils::ZeroedExpParam m_modDepth[4];

    PhaseUtils::ZeroedExpParam m_feedback[4];

    OPLowPassFilter m_modDepthFilter[4];

    OPLowPassFilter m_rotateFilter[4];

    PhaseUtils::ExpParam m_unisonDetune[4];
    PhaseUtils::ZeroedExpParam m_unisonGain[4];
    PhaseUtils::ExpParam m_slewUp[4];
    PhaseUtils::ExpParam m_slewDown[4];

    PhaseUtils::ExpParam m_rmsThreshold[4];
    PhaseUtils::ExpParam m_rmsQuiet[4];
    PhaseUtils::ExpParam m_rmsLoud[4];
    PhaseUtils::ZeroedExpParam m_loudShift[4];

    float m_bufferFrac[4];
    WriteTapeHead m_writeTapeHead[4];
    ReadTapeHead m_readTapeHead[4];

    struct Input
    {
        int m_delayTimeFactorSwitchVal[4];
        int m_readHeadSpeedSwitchVal[4];
        int m_loopSelectorSwitchVal[4];
        float m_widenKnob[4];
        int m_rotateSwitchVal[4];
        float m_modFreqKnob[4];
        float m_modDepthKnob[4];
        float m_feedbackKnob[4];
        float m_dampingBaseKnob[4];
        float m_dampingWidthKnob[4];
        float m_grainSamplesKnob[4];
        float m_grainOverlapKnob[4];
        float m_lfoPhaseKnob[4];

        float m_resynthShiftFadeKnob[4];
        float m_resynthUnisonKnob[4];
        int m_resynthShiftSwitchVal[4];
        float m_resynthSlewUpKnob[4];
        
        TheoryOfTime* m_theoryOfTime;

        Input()
            : m_theoryOfTime(nullptr)
        {
            for (int i = 0; i < 4; ++i)
            {
                m_delayTimeFactorSwitchVal[i] = 3;
                m_readHeadSpeedSwitchVal[i] = 10;
                m_loopSelectorSwitchVal[i] = 0;
                m_widenKnob[i] = 0.0f;
                m_rotateSwitchVal[i] = 1;
                m_modFreqKnob[i] = 0.0f;
                m_modDepthKnob[i] = 0.0f;
                m_feedbackKnob[i] = 0.0f;
                m_dampingBaseKnob[i] = 0.0f;
                m_dampingWidthKnob[i] = 0.0f;
                m_grainSamplesKnob[i] = 0.0f;
                m_grainOverlapKnob[i] = 0.0f;
                m_lfoPhaseKnob[i] = 0.0f;

                m_resynthShiftFadeKnob[i] = 0.0f;
                m_resynthUnisonKnob[i] = 0.0f;
                m_resynthShiftSwitchVal[i] = 9;
                m_resynthSlewUpKnob[i] = 0.0f;                
            }
        }
    };

    QuadDelayInputSetter()
      : m_wideners
      {
        PhaseUtils::ExpParam(1.0, 0.978615137775),
        PhaseUtils::ExpParam(1.0, 1.0),
        PhaseUtils::ExpParam(1.0, 0.986803398875),
        PhaseUtils::ExpParam(1.0, 0.989701964951)
      }
    {
        for (int i = 0; i < 4; ++i)
        {
            m_dampingBase[i] = PhaseUtils::ExpParam(1.0 / 2048.0, 0.5);
            m_dampingWidth[i] = PhaseUtils::ExpParam(1.0, 2048.0);
            m_modFreq[i] = PhaseUtils::ExpParam(0.05 / 48000.0, 1024.0 * 0.05 / 48000.0);
            m_modDepthFilter[i].SetAlphaFromNatFreq(1.0 / 48000.0);
            m_rotateFilter[i].SetAlphaFromNatFreq(1.0 / 48000.0);
            m_feedback[i].SetBaseByCenter(0.25);

            m_unisonDetune[i] = PhaseUtils::ExpParam(1.03);
            m_unisonGain[i] = PhaseUtils::ZeroedExpParam(20);

            // Range: 0.25 / (grain launch samples) to 0.5
            //
            m_slewUp[i] = PhaseUtils::ExpParam(0.25 / Resynthesizer::GetGrainLaunchSamples(), 0.5);

            m_bufferFrac[i] = 1.0;
            m_readTapeHead[i].m_writeTapeHead = &m_writeTapeHead[i];
        }
    }

    void Process(Input& input, QuadDelay::Input& delayInput, QuadDelay* delay)
    {
        int totLoopSelectorSample = static_cast<int>(std::round(SampleTimer::GetUBlockIndex()));
        for (int i = 0; i < 4; ++i)
        {
            float widen = m_wideners[i].Update(input.m_widenKnob[i]);

            int totLoopSelector =
                TheoryOfTimeBase::x_numLoops - input.m_loopSelectorSwitchVal[i] - 1;
            WriteTapeHead::Input writeInput;
            writeInput.m_theoryOfTime = input.m_theoryOfTime;
            writeInput.m_sampleIndex = totLoopSelectorSample;
            writeInput.m_running = input.m_theoryOfTime->m_running;
            writeInput.m_masterLoopSamples = input.m_theoryOfTime->m_masterLoopSamples;
            writeInput.m_requestedLoopSelector = totLoopSelector;
            m_writeTapeHead[i].Update(writeInput);

            if (input.m_theoryOfTime->GetIndirectTop(totLoopSelectorSample, m_writeTapeHead[i].m_loopSelector))
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

            double writeHeadPosition = m_writeTapeHead[i].m_actualPosition;
            if (std::abs(writeHeadPosition - delayInput.m_writeHeadPosition[i]) >= 64 && i == 0)
            {
                INFO("writeHeadPosition: %f -> %f (diff %f) external loop", 
                    delayInput.m_writeHeadPosition[i],
                    writeHeadPosition, 
                    std::abs(writeHeadPosition - delayInput.m_writeHeadPosition[i]));
                INFO("theory of time microblock %d index %d phasor indirect %f direct %f master %f master indirect %f master unwound %f loop external mult %d",
                    totLoopSelectorSample,
                    m_writeTapeHead[i].m_loopSelector,
                    input.m_theoryOfTime->GetIndirectPhasor(totLoopSelectorSample, m_writeTapeHead[i].m_loopSelector),
                    input.m_theoryOfTime->GetDirectPhasor(totLoopSelectorSample, m_writeTapeHead[i].m_loopSelector),
                    input.m_theoryOfTime->GetPhasorIndependent(m_writeTapeHead[i].m_loopSelector),
                    input.m_theoryOfTime->GetIndirectPhasor(totLoopSelectorSample, TheoryOfTimeBase::x_masterLoop),
                    input.m_theoryOfTime->m_globalPhase.UnWind(),
                    input.m_theoryOfTime->GetLoopExternalMultiplier(totLoopSelectorSample, m_writeTapeHead[i].m_loopSelector));
            }

            delayInput.m_writeHeadPosition[i] = writeHeadPosition;

            ReadTapeHead::Input readInput;
            readInput.m_theoryOfTime = input.m_theoryOfTime;
            readInput.m_sampleIndex = totLoopSelectorSample;
            readInput.m_bufferFraction = m_bufferFrac[i] * widen;
            readInput.m_readHeadSpeed = possibleReadHeadSpeeds[input.m_readHeadSpeedSwitchVal[i]];
            m_readTapeHead[i].Update(readInput);
            delayInput.m_readHeadPosition[i] = m_readTapeHead[i].m_actualPosition;
            delayInput.m_relativeWriteHeadPosition[i] = static_cast<float>(m_writeTapeHead[i].m_relativePosition);
            delayInput.m_relativeReadHeadPosition[i] = static_cast<float>(m_readTapeHead[i].m_relativePosition);

            float rotate = static_cast<float>(input.m_rotateSwitchVal[i]) / 4.0f;
            delayInput.m_rotate[i] = m_rotateFilter[i].Process(rotate);

            delayInput.m_modDepth[i] = m_modDepthFilter[i].Process(m_modDepth[i].Update(input.m_modDepthKnob[i]));
            delayInput.m_lfoInput.m_freq[i] = m_modFreq[i].Update(input.m_modFreqKnob[i]);

            delayInput.m_feedback[i] = 1.25 * m_feedback[i].Update(input.m_feedbackKnob[i]);

            delayInput.m_bffBase[i] = m_dampingBase[i].Update(input.m_dampingBaseKnob[i]);
            delayInput.m_bffWidth[i] = m_dampingWidth[i].Update(input.m_dampingWidthKnob[i]);

            delayInput.m_lfoInput.m_phaseKnob[i] = input.m_lfoPhaseKnob[i];

            // Resynth parameters
            //
            Resynthesizer::Input& resynthInput = delayInput.m_grainManagerInput.m_input[i].m_resynthInput;

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
            resynthInput.m_unisonDetune = m_unisonDetune[i].Update(input.m_resynthUnisonKnob[i]);
            resynthInput.m_unisonGain = m_unisonGain[i].Update(std::min<float>(1.0, 10 * input.m_resynthUnisonKnob[i]));
            resynthInput.m_slewUp = m_slewUp[i].Update(1.0f - input.m_resynthSlewUpKnob[i]);
        }

        if (delay)
        {
            size_t sampleIndex = static_cast<size_t>(std::round(SampleTimer::GetUBlockIndex())) % TheoryOfTimeBase::x_microBlockSize;

            for (int i = 0; i < 4; ++i)
            {
                double x = input.m_theoryOfTime->GetIndirectPhasor(sampleIndex);
                double y = delayInput.m_writeHeadPosition[i];
                delay->m_positionalBufferRecorder[i].Record(x, y);
            }
        }
    }
};
