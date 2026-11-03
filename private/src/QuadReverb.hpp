#pragma once

#include "QuadUtils.hpp"
#include "AllPassFilter.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "Filter.hpp"
#include "Saturator.hpp"
#include "PhaseUtils.hpp"
#include "SampleTimer.hpp"

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

    QuadTanhSaturator<true> m_saturator;

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

        // m_inputFilter is 8 quad all-pass stages in parallel.
        // SetDelaySamples(stage, channel, samples) indexes the channel as the second
        // arg, so the channel loop must stay within [0, 4).
        //
        for (size_t ch = 0; ch < 4; ++ch)
        {
            m_inputFilter.SetDelaySamples(0, ch, 37);
            m_inputFilter.SetDelaySamples(1, ch, 61);
            m_inputFilter.SetDelaySamples(2, ch, 113);
            m_inputFilter.SetDelaySamples(3, ch, 179);
            m_inputFilter.SetDelaySamples(4, ch, 281);
            m_inputFilter.SetDelaySamples(5, ch, 431);
            m_inputFilter.SetDelaySamples(6, ch, 613);
            m_inputFilter.SetDelaySamples(7, ch, 877);
        }
    }

    QuadReverb()
        : m_saturator(0.5f)
        , m_output()
    {
        SetAPFGain(0.6);
        SetAPFDelayTimes();
        m_lfo.SetSlew(SampleTimer::HzToCycPerSamp(10.0));
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
        DampingFilter::UIState m_dampingFilter[4];
    };

    void PopulateUIState(UIState* uiState)
    {
        for (int i = 0; i < 4; ++i)
        {
            uiState->m_dampingFilter[i].m_hpAlpha.store(m_postFeedbackFilter.m_bff.m_highPassFilter.m_alpha[i]);
            uiState->m_dampingFilter[i].m_lpAlpha.store(m_postFeedbackFilter.m_bff.m_lowPassFilter.m_alpha[i]);
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
    using QuadExpParam = PhaseUtils::ExpParamGeneric<float, 4>;
    using QuadZeroedExpParam = PhaseUtils::ZeroedExpParamGeneric<float, 4>;

    QuadExpParam m_delayTime;
    QuadExpParam m_dampingBase;
    QuadExpParam m_dampingWidth;
    QuadExpParam m_modFreq;

    // Each channel widens over its own range, so the wideners stay per channel.
    //
    PhaseUtils::ExpParam m_wideners[4];
    QuadZeroedExpParam m_modDepth;

    QuadZeroedExpParam m_feedback;

    OPLowPassFilterGeneric<float, 4> m_delayTimeFilter;
    OPLowPassFilterGeneric<float, 4> m_modDepthFilter;

    struct Input
    {
        QuadFloat m_reverbTimeKnob;
        QuadFloat m_modFreqKnob;
        QuadFloat m_modDepthKnob;
        QuadFloat m_feedbackKnob;
        QuadFloat m_dampingBaseKnob;
        QuadFloat m_dampingWidthKnob;
        QuadFloat m_lfoPhaseKnob;

        Input()
            : m_reverbTimeKnob()
            , m_modFreqKnob()
            , m_modDepthKnob()
            , m_feedbackKnob()
            , m_dampingBaseKnob()
            , m_dampingWidthKnob()
            , m_lfoPhaseKnob()
        {
        }
    };

    QuadReverbInputSetter()
      : m_delayTime(60.0, 1024.0 * 60.0)
      , m_dampingBase(1.0 / 2048.0, 0.5)
      , m_dampingWidth(1.0, 2048.0)
      , m_modFreq(SampleTimer::HzToCycPerSamp(0.05), SampleTimer::HzToCycPerSamp(1024.0 * 0.05))
      , m_wideners
      {
        PhaseUtils::ExpParam(1.0, 0.978615137775),
        PhaseUtils::ExpParam(1.0, 1.0),
        PhaseUtils::ExpParam(1.0, 0.986803398875),
        PhaseUtils::ExpParam(1.0, 0.989701964951)
      }
    {
        m_delayTimeFilter.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        m_modDepthFilter.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        m_feedback.SetBaseByCenter(0.25);
    }

    void Process(Input& input, QuadReverb::Input& reverbInput)
    {
        reverbInput.m_delayTimeSamples = m_delayTime.Update(m_delayTimeFilter.Process(input.m_reverbTimeKnob));
        reverbInput.m_bffBase = m_dampingBase.Update(input.m_dampingBaseKnob);
        reverbInput.m_bffWidth = m_dampingWidth.Update(input.m_dampingWidthKnob);
        reverbInput.m_modDepth = m_modDepthFilter.Process(m_modDepth.Update(input.m_modDepthKnob));
        reverbInput.m_lfoInput.m_freq = m_modFreq.Update(input.m_modFreqKnob);
        reverbInput.m_feedback = m_feedback.Update(input.m_feedbackKnob) * 1.25f;
        reverbInput.m_lfoInput.m_phaseKnob = input.m_lfoPhaseKnob;
        for (int i = 0; i < 4; ++i)
        {
            reverbInput.m_widen[i] = m_wideners[i].Update(1.0 /* reverb max width */);
        }
    }
};
