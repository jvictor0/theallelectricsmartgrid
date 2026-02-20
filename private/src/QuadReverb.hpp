#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "Filter.hpp"
#include "PhaseUtils.hpp"

struct QuadReverb
{
    struct PostFeedbackFilter
    {
        QuadAllPassFilter m_apf1;
        QuadOPBaseWidthFilter m_bff;
        QuadAllPassFilter m_apf2;

        QuadFloat Process(QuadFloat input)
        {
            QuadFloat output = m_apf1.Process(input);
            output = m_bff.Process(output);
            output = m_apf2.Process(output);
            return output;
        }

        void SetAPFGain(float gain)
        {
            m_apf1.SetGain(gain);
            m_apf2.SetGain(gain);
        }

        void SetBFFBaseWidth(QuadFloat base, QuadFloat width)
        {
            m_bff.SetBaseWidth(base, width);
        }
    };
    
    static constexpr size_t x_delayLineSize = 1 << 16;
    QuadDelayLine<x_delayLineSize> m_delayLine;
    QuadLFO m_lfo;

    QuadParallelAllPassFilter<8> m_inputFilter;
    
    QuadAllPassFilter m_preFeedbackFilter;
    PostFeedbackFilter m_postFeedbackFilter;

    TanhSaturator<true> m_saturator;

    QuadFloat m_output;

    void SetAPFGain(float gain)
    {
        m_inputFilter.SetGain(gain);
        m_preFeedbackFilter.SetGain(gain);
        m_postFeedbackFilter.SetAPFGain(gain);
    }
    
    void SetAPFDelayTimes()
    {
        m_preFeedbackFilter.SetDelaySamples(0, 3);
        m_preFeedbackFilter.SetDelaySamples(1, 5);
        m_preFeedbackFilter.SetDelaySamples(2, 7);
        m_preFeedbackFilter.SetDelaySamples(3, 11);

        m_postFeedbackFilter.m_apf1.SetDelaySamples(0, 17);
        m_postFeedbackFilter.m_apf1.SetDelaySamples(1, 31);
        m_postFeedbackFilter.m_apf1.SetDelaySamples(2, 53);
        m_postFeedbackFilter.m_apf1.SetDelaySamples(3, 23);

        m_postFeedbackFilter.m_apf2.SetDelaySamples(0, 149);
        m_postFeedbackFilter.m_apf2.SetDelaySamples(1, 211);
        m_postFeedbackFilter.m_apf2.SetDelaySamples(2, 113);
        m_postFeedbackFilter.m_apf2.SetDelaySamples(3, 293);

        for (size_t i = 0; i < 8; ++i)
        {
            m_inputFilter.SetDelaySamples(0, i, 37);
            m_inputFilter.SetDelaySamples(1, i, 61);
            m_inputFilter.SetDelaySamples(2, i, 113);
            m_inputFilter.SetDelaySamples(3, i, 179);
            m_inputFilter.SetDelaySamples(4, i, 281);
            m_inputFilter.SetDelaySamples(5, i, 431);
            m_inputFilter.SetDelaySamples(6, i, 613);
            m_inputFilter.SetDelaySamples(7, i, 877);
        }
    }

    QuadReverb()
        : m_saturator(0.5f)
        , m_output()
    {
        SetAPFGain(0.6);
        SetAPFDelayTimes();
        m_lfo.SetSlew(10.0 / 48000.0);
    }
    
    struct Input
    {
        QuadFloat m_input;
        QuadFloat m_return;
        QuadFloat m_delayTimeSamples;
        QuadFloat m_feedback;
        QuadFloat m_widen;
        QuadFloat m_modDepth;

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        QuadLFO::Input m_lfoInput; 

        Input()
            : m_delayTimeSamples(64, 64, 64, 64)
            , m_feedback(0.5, 0.5, 0.5, 0.5)
            , m_widen(1.0, 1.0, 1.0, 1.0)
        {
        }

        QuadFloat TransformOutput(QuadFloat output)
        {
            return output.Hadamard();
        }
    };

    struct UIState
    {
        std::atomic<float> m_hpAlpha[4];
        std::atomic<float> m_lpAlpha[4];
    };

    void PopulateUIState(UIState* uiState)
    {
        for (int i = 0; i < 4; ++i)
        {
            uiState->m_hpAlpha[i].store(m_postFeedbackFilter.m_bff.m_filters[i].m_highPassFilter.m_alpha);
            uiState->m_lpAlpha[i].store(m_postFeedbackFilter.m_bff.m_filters[i].m_lowPassFilter.m_alpha);
        }
    }

    QuadFloat Process(Input& input)
    {
        m_lfo.Process(input.m_lfoInput);
        m_postFeedbackFilter.SetBFFBaseWidth(input.m_bffBase, input.m_bffWidth);

        QuadFloat qInput = m_inputFilter.Process(input.m_input);
        QuadFloat retrn = m_preFeedbackFilter.Process(input.m_return);
        qInput = qInput + retrn * input.m_feedback;
        qInput = m_saturator.Process(qInput);
        m_delayLine.Write(qInput);

        QuadFloat delayTime = input.m_delayTimeSamples * input.m_widen;
        delayTime += m_lfo.m_output * 1000 * input.m_modDepth;
        QuadFloat delayedSignal = m_delayLine.Read(delayTime);

        m_output = m_postFeedbackFilter.Process(input.TransformOutput(delayedSignal));

        return m_output;
    }           
};

struct QuadReverbInputSetter
{
    PhaseUtils::ExpParam m_delayTime[4];
    PhaseUtils::ExpParam m_dampingBase[4];
    PhaseUtils::ExpParam m_dampingWidth[4];
    PhaseUtils::ExpParam m_modFreq[4];

    PhaseUtils::ExpParam m_wideners[4];
    PhaseUtils::ZeroedExpParam m_modDepth[4];

    PhaseUtils::ZeroedExpParam m_feedback[4];

    OPLowPassFilter m_delayTimeFilter[4];
    OPLowPassFilter m_modDepthFilter[4];

    struct Input
    {
        float m_reverbTimeKnob[4];
        float m_modFreqKnob[4];
        float m_modDepthKnob[4];
        float m_feedbackKnob[4];
        float m_dampingBaseKnob[4];
        float m_dampingWidthKnob[4];
        float m_widenKnob[4];
        float m_lfoPhaseKnob[4];

        Input()
        {
            for (int i = 0; i < 4; ++i)
            {
                m_reverbTimeKnob[i] = 0.0f;
                m_modFreqKnob[i] = 0.0f;
                m_modDepthKnob[i] = 0.0f;
                m_feedbackKnob[i] = 0.0f;
                m_dampingBaseKnob[i] = 0.0f;
                m_dampingWidthKnob[i] = 0.0f;
                m_widenKnob[i] = 0.0f;
                m_lfoPhaseKnob[i] = 0.0f;
            }
        }
    };

    QuadReverbInputSetter()
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
            m_delayTime[i] = PhaseUtils::ExpParam(60.0, 1024.0 * 60.0);
            m_dampingBase[i] = PhaseUtils::ExpParam(1.0 / 2048.0, 0.5);
            m_dampingWidth[i] = PhaseUtils::ExpParam(1.0, 2048.0);
            m_modFreq[i] = PhaseUtils::ExpParam(0.05 / 48000.0, 1024.0 * 0.05 / 48000.0);
            m_delayTimeFilter[i].SetAlphaFromNatFreq(1.0 / 48000.0);
            m_modDepthFilter[i].SetAlphaFromNatFreq(1.0 / 48000.0);
            m_feedback[i].SetBaseByCenter(0.25);
        }
    }

    void Process(Input& input, QuadReverb::Input& reverbInput)
    {
        for (int i = 0; i < 4; ++i)
        {
            reverbInput.m_delayTimeSamples[i] = m_delayTime[i].Update(m_delayTimeFilter[i].Process(input.m_reverbTimeKnob[i]));

            reverbInput.m_bffBase[i] = m_dampingBase[i].Update(input.m_dampingBaseKnob[i]);
            reverbInput.m_bffWidth[i] = m_dampingWidth[i].Update(input.m_dampingWidthKnob[i]);

            reverbInput.m_modDepth[i] = m_modDepthFilter[i].Process(m_modDepth[i].Update(input.m_modDepthKnob[i]));
            reverbInput.m_lfoInput.m_freq[i] = m_modFreq[i].Update(input.m_modFreqKnob[i]);

            reverbInput.m_widen[i] = m_wideners[i].Update(1.0 /* reverb max width */);

            reverbInput.m_feedback[i] = 1.25 * m_feedback[i].Update(input.m_feedbackKnob[i]);

            reverbInput.m_lfoInput.m_phaseKnob[i] = input.m_lfoPhaseKnob[i];
        }
    }
};

