#pragma once

#include "QuadUtils.hpp"
#include "AllPassFilter.hpp"
#include "DelayLine.hpp"
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
    
    // Delay times are defined in host samples and run in engine samples, so
    // the physical times stay fixed as the engine rate changes.
    //
    static constexpr size_t x_oversample = SampleTimer::x_oversample;
    static constexpr size_t x_delayLineSize = (static_cast<size_t>(1) << 16) * x_oversample;
    static constexpr float x_minDelayHostSamples = 60.0f;
    static constexpr float x_maxDelayHostSamples = 1024.0f * 60.0f;
    static constexpr float x_defaultDelayHostSamples = 64.0f;
    static constexpr float x_modulationHostSamples = 1000.0f;
    static constexpr int x_longestAllPassHostSamples = 877;
    static_assert(x_maxDelayHostSamples + x_modulationHostSamples < static_cast<float>(x_delayLineSize / x_oversample), "Reverb ring must hold the longest modulated delay");
    static_assert(static_cast<size_t>(x_longestAllPassHostSamples) * x_oversample < QuadAllPassFilter::x_maxDelaySamples, "All-pass buffers must hold the longest all-pass delay");
    QuadDelayLine<x_delayLineSize> m_delayLine;

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
    
    static int EngineSamples(int hostSamples)
    {
        return hostSamples * static_cast<int>(x_oversample);
    }

    void SetAPFDelayTimes()
    {
        m_preFeedbackFilter.SetDelaySamples(0, EngineSamples(3));
        m_preFeedbackFilter.SetDelaySamples(1, EngineSamples(5));
        m_preFeedbackFilter.SetDelaySamples(2, EngineSamples(7));
        m_preFeedbackFilter.SetDelaySamples(3, EngineSamples(11));

        m_postFeedbackFilter.m_apf1.SetDelaySamples(0, EngineSamples(17));
        m_postFeedbackFilter.m_apf1.SetDelaySamples(1, EngineSamples(31));
        m_postFeedbackFilter.m_apf1.SetDelaySamples(2, EngineSamples(53));
        m_postFeedbackFilter.m_apf1.SetDelaySamples(3, EngineSamples(23));

        m_postFeedbackFilter.m_apf2.SetDelaySamples(0, EngineSamples(149));
        m_postFeedbackFilter.m_apf2.SetDelaySamples(1, EngineSamples(211));
        m_postFeedbackFilter.m_apf2.SetDelaySamples(2, EngineSamples(113));
        m_postFeedbackFilter.m_apf2.SetDelaySamples(3, EngineSamples(293));

        // m_inputFilter is 8 quad all-pass stages in parallel.
        // SetDelaySamples(stage, channel, samples) indexes the channel as the second
        // arg, so the channel loop must stay within [0, 4).
        //
        for (size_t ch = 0; ch < 4; ++ch)
        {
            m_inputFilter.SetDelaySamples(0, ch, EngineSamples(37));
            m_inputFilter.SetDelaySamples(1, ch, EngineSamples(61));
            m_inputFilter.SetDelaySamples(2, ch, EngineSamples(113));
            m_inputFilter.SetDelaySamples(3, ch, EngineSamples(179));
            m_inputFilter.SetDelaySamples(4, ch, EngineSamples(281));
            m_inputFilter.SetDelaySamples(5, ch, EngineSamples(431));
            m_inputFilter.SetDelaySamples(6, ch, EngineSamples(613));
            m_inputFilter.SetDelaySamples(7, ch, EngineSamples(x_longestAllPassHostSamples));
        }
    }

    QuadReverb()
        : m_saturator(0.5f)
        , m_output()
    {
        SetAPFGain(0.6);
        SetAPFDelayTimes();
    }
    
    struct Input
    {
        QuadFloat m_input;
        QuadFloat m_return;
        QuadFloat m_delayTimeSamples;
        QuadFloat m_feedback;
        QuadFloat m_widen;
        QuadFloat m_delayOffsetSamples;

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        Input()
            : m_delayTimeSamples(QuadFloat::Splat(x_defaultDelayHostSamples * static_cast<float>(x_oversample)))
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
        QuadOPBaseWidthFilter::UIState m_dampingFilter;
    };

    SampledSnapshotWriter<QuadOPBaseWidthFilter::UIState> m_responseSampler;

    void SetupResponseUIState(UIState* uiState)
    {
        m_responseSampler.Bind(&uiState->m_dampingFilter);
    }

    void PopulateUIStateSample()
    {
        m_postFeedbackFilter.m_bff.PopulateUIStateSample(m_responseSampler);
    }


    QuadFloat Process(Input& input)
    {
        m_postFeedbackFilter.SetBFFBaseWidth(input.m_bffBase, input.m_bffWidth);

        QuadFloat qInput = m_inputFilter.Process(input.m_input);
        QuadFloat retrn = m_preFeedbackFilter.Process(input.m_return);
        qInput = qInput + retrn * input.m_feedback;
        qInput = m_saturator.Process(qInput);
        m_delayLine.Write(qInput);

        QuadFloat delayTime = input.m_delayTimeSamples * input.m_widen;
        delayTime += input.m_delayOffsetSamples;
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

    // Each channel widens over its own range, so the wideners stay per channel.
    //
    PhaseUtils::ExpParam m_wideners[4];

    QuadZeroedExpParam m_feedback;

    // Double state: 1 Hz one-poles at the engine rate stall short of their
    // targets in float.
    //
    OPLowPassFilterGeneric<double, 4> m_delayTimeFilter;

    struct Input
    {
        QuadFloat m_reverbTimeKnob;
        QuadFloat m_delayOffsetKnob;
        QuadFloat m_feedbackKnob;
        QuadFloat m_dampingBaseKnob;
        QuadFloat m_dampingWidthKnob;

        Input()
            : m_reverbTimeKnob()
            , m_delayOffsetKnob(QuadFloat::Splat(0.5f))
            , m_feedbackKnob()
            , m_dampingBaseKnob()
            , m_dampingWidthKnob()
        {
        }
    };

    QuadReverbInputSetter()
      : m_delayTime(
          QuadReverb::x_minDelayHostSamples * static_cast<float>(QuadReverb::x_oversample),
          QuadReverb::x_maxDelayHostSamples * static_cast<float>(QuadReverb::x_oversample))
      , m_dampingBase(SampleTimer::HzToCycPerSamp(QuadOPBaseWidthFilter::x_minBaseHz), SampleTimer::HzToCycPerSamp(QuadOPBaseWidthFilter::x_maxBaseHz))
      , m_dampingWidth(1.0, 2048.0)
      , m_wideners
      {
        PhaseUtils::ExpParam(1.0, 0.978615137775),
        PhaseUtils::ExpParam(1.0, 1.0),
        PhaseUtils::ExpParam(1.0, 0.986803398875),
        PhaseUtils::ExpParam(1.0, 0.989701964951)
      }
    {
        m_delayTimeFilter.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        m_feedback.SetBaseByCenter(0.25);
    }

    void Process(Input& input, QuadReverb::Input& reverbInput)
    {
        reverbInput.m_delayTimeSamples = m_delayTime.Update(m_delayTimeFilter.Process(input.m_reverbTimeKnob.Convert<double>()).Convert<float>());
        reverbInput.m_bffBase = m_dampingBase.Update(input.m_dampingBaseKnob);
        reverbInput.m_bffWidth = m_dampingWidth.Update(input.m_dampingWidthKnob);
        reverbInput.m_delayOffsetSamples = (input.m_delayOffsetKnob * 2.0f - 1.0f)
            * (QuadReverb::x_modulationHostSamples * static_cast<float>(QuadReverb::x_oversample));
        reverbInput.m_feedback = m_feedback.Update(input.m_feedbackKnob) * 1.25f;
        for (int i = 0; i < 4; ++i)
        {
            reverbInput.m_widen[i] = m_wideners[i].Update(1.0 /* reverb max width */);
        }
    }
};
