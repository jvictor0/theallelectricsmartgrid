#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "Filter.hpp"
#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
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
    
    static constexpr size_t x_delayLineSize = 1 << 24;
    QuadDelayLineMovableWriter<x_delayLineSize> m_delayLine;
    QuadGrainManager<x_delayLineSize> m_grainManager;
    QuadLFO m_lfo;

    PostFeedbackFilter m_postFeedbackFilter;

    TanhSaturator<true> m_saturator;

    QuadFloat m_output;

    QuadDelay()
        : m_grainManager(&m_delayLine)
        , m_saturator(0.5f)
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

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        QuadLFO::Input m_lfoInput;

        QuadGrainManager<x_delayLineSize>::Input m_grainManagerInput;

        Input()
            : m_feedback(0.5, 0.5, 0.5, 0.5)
            , m_rotate(0, 0, 0, 0)
        {
        }

        QuadFloat TransformOutput(QuadFloat output)
        {
            return output.RotateLinear(m_rotate);
        }
    };

    struct UIState
    {
        std::atomic<float> m_hpAlpha[4];
        std::atomic<float> m_lpAlpha[4];
        std::atomic<float> m_sigma[4];
        std::atomic<int> m_overlap[4];
        std::atomic<double> m_grainSize[4];
        GrainManager<x_delayLineSize>::UIState m_delayLineUIState[4];
    };

    void PopulateUIState(UIState* uiState, QuadDelay::Input& input)
    {
        for (int i = 0; i < 4; ++i)
        {
            uiState->m_hpAlpha[i].store(m_postFeedbackFilter.m_bff.m_filters[i].m_highPassFilter.m_alpha);
            uiState->m_lpAlpha[i].store(m_postFeedbackFilter.m_bff.m_filters[i].m_lowPassFilter.m_alpha);
            m_grainManager.m_grainManager[i].PopulateUIState(&uiState->m_delayLineUIState[i]);
        }
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

    int m_totLoopSelector[4];
    float m_bufferFrac[4];

    double m_masterLoopSamples[4];
    bool m_running[4];
    double m_glue[4];

    struct Input
    {
        float m_delayTimeFactorKnob[4];
        float m_loopSelectorKnob[4];
        float m_widenKnob[4];
        float m_rotateKnob[4];
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
        float m_resynthShiftKnob[4];
        float m_resynthSlewUpKnob[4];
        
        TheoryOfTime* m_theoryOfTime;

        Input()
            : m_theoryOfTime(nullptr)
        {
            for (int i = 0; i < 4; ++i)
            {
                m_delayTimeFactorKnob[i] = 0.0f;
                m_loopSelectorKnob[i] = 0.0f;
                m_widenKnob[i] = 0.0f;
                m_rotateKnob[i] = 0.0f;
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
                m_resynthShiftKnob[i] = 0.0f;                
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

            m_totLoopSelector[i] = TheoryOfTime::x_numLoops - 1;
            m_bufferFrac[i] = 1.0;
            m_glue[i] = 0.0;
            m_masterLoopSamples[i] = 1.0;
            m_running[i] = false;
        }
    }

    void Process(Input& input, QuadDelay::Input& delayInput)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!input.m_theoryOfTime->m_running)
            {
                if (m_running[i])
                {
                    m_running[i] = false;
                    m_glue[i] = delayInput.m_writeHeadPosition[i];
                }

                m_glue[i] = m_glue[i] + 1;
            }
            else if (!m_running[i])
            {
                m_running[i] = true;
            }

            if (m_masterLoopSamples[i] != input.m_theoryOfTime->m_masterLoopSamples)
            {
                m_glue[i] = delayInput.m_writeHeadPosition[i] - (delayInput.m_writeHeadPosition[i] - m_glue[i]) * input.m_theoryOfTime->m_masterLoopSamples / m_masterLoopSamples[i];
                m_masterLoopSamples[i] = input.m_theoryOfTime->m_masterLoopSamples;
            }

            float widen = m_wideners[i].Update(input.m_widenKnob[i]);

            int totLoopSelector = std::round((1.0 - input.m_loopSelectorKnob[i]) * (TheoryOfTime::x_numLoops - 1));
            if (totLoopSelector != m_totLoopSelector[i] && 
                input.m_theoryOfTime->m_loops[m_totLoopSelector[i]].m_top &&
                input.m_theoryOfTime->m_loops[totLoopSelector].m_top)
            {
                int oldTotLoopSelector = m_totLoopSelector[i];
                m_totLoopSelector[i] = totLoopSelector;

                m_glue[i] += input.m_theoryOfTime->PhasorUnwoundSamples(oldTotLoopSelector) - input.m_theoryOfTime->PhasorUnwoundSamples(totLoopSelector);
            }

            if (input.m_theoryOfTime->m_loops[m_totLoopSelector[i]].m_top)
            {
                int factorIx = std::round(input.m_delayTimeFactorKnob[i] * 4);
                float possibleBufferFracs[5] = {0.8, 2.0/3.0, 1.0, 3.0/4.0, 5.0/8.0};
                m_bufferFrac[i] = possibleBufferFracs[factorIx];
            }

            double writeHeadPosition = input.m_theoryOfTime->PhasorUnwoundSamples(m_totLoopSelector[i]) + m_glue[i];
            if (std::abs(writeHeadPosition - delayInput.m_writeHeadPosition[i]) > 64)
            {
                INFO("writeHeadPosition: %f -> %f (diff %f) external loop", 
                    delayInput.m_writeHeadPosition[i],
                    writeHeadPosition, 
                    std::abs(writeHeadPosition - delayInput.m_writeHeadPosition[i]));
            }

            delayInput.m_writeHeadPosition[i] = writeHeadPosition;

            delayInput.m_readHeadPosition[i] = input.m_theoryOfTime->LoopSamplesFraction(m_totLoopSelector[i], m_bufferFrac[i] * widen) + m_glue[i];

            float rotate = std::round(input.m_rotateKnob[i] * 4) / 4.0f;
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

            int possibleShiftIndex = std::round(input.m_resynthShiftKnob[i] * 9);
            resynthInput.m_shift[0] = possibleShifts[possibleShiftIndex];

            resynthInput.m_fade[0] = input.m_resynthShiftFadeKnob[i];
            resynthInput.m_unisonDetune = m_unisonDetune[i].Update(input.m_resynthUnisonKnob[i]);
            resynthInput.m_unisonGain = m_unisonGain[i].Update(std::min<float>(1.0, 10 * input.m_resynthUnisonKnob[i]));
            resynthInput.m_slewUp = m_slewUp[i].Update(1.0f - input.m_resynthSlewUpKnob[i]);
        }
    }
};
