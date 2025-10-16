#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "Filter.hpp"
#include "PhaseUtils.hpp"

template<bool IsReverb>
struct QuadDelayInternal
{
    struct PostFeedbackFilter
    {
        QuadAllPassFilter m_apf1;
        QuadOPBaseWidthFilter m_bff;
        QuadAllPassFilter m_apf2;

        QuadFloat Process(QuadFloat input)
        {
            QuadFloat output = IsReverb ? m_apf1.Process(input) : input;
            output = m_bff.Process(output);
            output = IsReverb ? m_apf2.Process(output) : output;
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

    QuadDelayInternal()
        : m_saturator(0.5f)
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
        QuadFloat m_rotate;
        QuadFloat m_widen;
        QuadFloat m_modDepth;

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        QuadLFO::Input m_lfoInput; 

        QuadDelayLine<x_delayLineSize>::QuadXFader m_delayTimeFader;

        Input()
            : m_delayTimeSamples(64, 64, 64, 64)
            , m_feedback(0.5, 0.5, 0.5, 0.5)
            , m_rotate(0, 0, 0, 0)
            , m_widen(1.0, 1.0, 1.0, 1.0)
        {
        }

        QuadFloat TransformOutput(QuadFloat output)
        {
            if (IsReverb)
            {
                return output.Hadamard();
            }
            else
            {
                return output.RotateLinear(m_rotate);
            }
        }
    };

    QuadFloat Process(Input& input)
    {
        m_lfo.Process(input.m_lfoInput);
        m_postFeedbackFilter.SetBFFBaseWidth(input.m_bffBase, input.m_bffWidth);

        QuadFloat qInput = IsReverb ? m_inputFilter.Process(input.m_input) : input.m_input;
        qInput = qInput + m_preFeedbackFilter.Process(input.m_return) * input.m_feedback;
        qInput = m_saturator.Process(qInput);
        m_delayLine.Write(qInput);

        QuadFloat delayedSignal;
        if (IsReverb)
        {
            QuadFloat delayTime = input.m_delayTimeSamples * input.m_widen;
            delayTime += m_lfo.m_output * 1000 * input.m_modDepth;
            delayedSignal = m_delayLine.Read(delayTime);
        }
        else
        {
            input.m_delayTimeFader.Process();
            QuadDelayLine<x_delayLineSize>::QuadXFader delayTimeFader = input.m_delayTimeFader.ScaleOffset(input.m_widen, m_lfo.m_output * 1000 * input.m_modDepth);
            delayedSignal = m_delayLine.Read(delayTimeFader);
        }

        m_output = m_postFeedbackFilter.Process(input.TransformOutput(delayedSignal));

        return m_output;
    }           
};

template<bool IsReverb>
struct QuadDelayInputSetter
{
    PhaseUtils::ExpParam m_delayTime[4];
    PhaseUtils::ExpParam m_dampingBase[4];
    PhaseUtils::ExpParam m_dampingWidth[4];
    PhaseUtils::ExpParam m_modFreq[4];

    PhaseUtils::ExpParam m_wideners[4];
    PhaseUtils::ExpParam m_modDepth[4];

    PhaseUtils::ZeroedExpParam m_feedback[4];

    DelayTimeSynchronizer m_delayTimeSynchronizer[4];

    OPLowPassFilter m_delayTimeFilter[4];
    OPLowPassFilter m_modDepthFilter[4];

    OPLowPassFilter m_rotateFilter[4];

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
            m_delayTime[i] = PhaseUtils::ExpParam(60.0, 1024.0 * 60.0);
            m_dampingBase[i] = PhaseUtils::ExpParam(1.0 / 2048.0, 0.5);
            m_dampingWidth[i] = PhaseUtils::ExpParam(1.0, 2048.0);
            m_modFreq[i] = PhaseUtils::ExpParam(0.05 / 48000.0, 1024.0 * 0.05 / 48000.0);
            m_delayTimeFilter[i].SetAlphaFromNatFreq(1.0 / 48000.0);
            m_modDepthFilter[i].SetAlphaFromNatFreq(1.0 / 48000.0);
            m_rotateFilter[i].SetAlphaFromNatFreq(0.25 / 48000.0);
        }
    }

    void SetDelayTime(int i, float delayTime, float tempoFreq, float delayTimeFactorKnob, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        delayTime = m_delayTime[i].Update(delayTime);
        m_delayTimeSynchronizer[i].Update(input.m_delayTimeFader.m_fader[i], tempoFreq, delayTime, delayTimeFactorKnob);
    }

    void SetReverbTime(int i, float reverbTime, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        input.m_delayTimeSamples[i] = m_delayTime[i].Update(m_delayTimeFilter[i].Process(reverbTime));
    }

    void SetDamping(int i, float dampingBase, float dampingWidth, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        input.m_bffBase[i] = m_dampingBase[i].Update(dampingBase);
        input.m_bffWidth[i] = m_dampingWidth[i].Update(dampingWidth);
    }
    
    void SetModulation(int i, float modFreq, float modDepth, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        input.m_modDepth[i] = m_modDepthFilter[i].Process(modDepth);
        input.m_lfoInput.m_freq[i] = m_modFreq[i].Update(modFreq);
    }

    void SetWiden(int i, float widen, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        input.m_widen[i] = m_wideners[i].Update(widen);
    }

    void SetRotate(int i, float rotate, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        rotate = std::round(rotate * 4) / 4.0f;
        input.m_rotate[i] = m_rotateFilter[i].Process(rotate);
    }

    void SetFeedback(int i, float feedback, typename QuadDelayInternal<IsReverb>::Input& input)
    {
        input.m_feedback[i] = m_feedback[i].Update(feedback);
    }
};
