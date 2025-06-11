#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "Filter.hpp"

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

        void SetBFFBaseWidth(float base, float width)
        {
            m_bff.SetBaseWidth(base, width);
        }
    };
    
    QuadDelayLine m_delayLine;
    QuadLFO m_lfo;

    QuadParallelAllPassFilter<8> m_inputFilter;
    
    QuadAllPassFilter m_preFeedbackFilter;
    PostFeedbackFilter m_postFeedbackFilter;

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
    {
        SetAPFGain(0.6);
        SetAPFDelayTimes();
    }
    
    struct Input
    {
        QuadFloat m_input;
        QuadFloat m_return;
        float m_delayTimeSamples;
        float m_feedback;
        float m_rotate;
        float m_widen;
        float m_modDepth;

        float m_bffBase;
        float m_bffWidth;

        QuadLFO::Input m_lfoInput; 

        Input()
            : m_delayTimeSamples(64)
            , m_feedback(0.5)
            , m_rotate(0)
            , m_widen(0)
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
                float nearest = std::round(m_rotate * 4) / 4.0f;
                float distance  = std::fabs(m_rotate - nearest);
                float factor = exp( -distance * 10);

                float rotate = m_rotate + (nearest - m_rotate) * factor / 5;
                return output.Rotate(rotate);
            }
        }

        QuadFloat GetBaseDelayTime()
        {
            QuadFloat widenOffset(0.78615137775, 1.0, 0.86803398875, 0.89701964951);
            return QuadFloat(m_delayTimeSamples, m_delayTimeSamples, m_delayTimeSamples, m_delayTimeSamples) + widenOffset * m_widen;
        }
    };

    void Process(Input& input)
    {
        m_lfo.Process(input.m_lfoInput);
        QuadFloat delayTime = input.GetBaseDelayTime();
        delayTime += m_lfo.m_output * delayTime * (input.m_modDepth / 2);

        m_postFeedbackFilter.SetBFFBaseWidth(input.m_bffBase, input.m_bffWidth);

        QuadFloat qInput = IsReverb ? m_inputFilter.Process(input.m_input) : input.m_input;
        qInput = qInput + m_preFeedbackFilter.Process(input.m_return) * input.m_feedback;
        m_delayLine.Write(qInput);
        m_output = m_postFeedbackFilter.Process(input.TransformOutput(m_delayLine.Read(delayTime)));
    }           
};

#ifndef IOS_BUILD
template<bool IsReverb>
struct QuadDelay : Module
{
    QuadDelayInternal<IsReverb> m_internal;
    typename QuadDelayInternal<IsReverb>::Input m_state;

    float m_logDelayTimeVal;
    float m_modulationVOctVal;
    OPSlew m_delayTimeSlew;

    float m_logDampingBFFBase;
    float m_logDampingBFFWidth;

    IOMgr m_ioMgr;
    IOMgr::Input* m_input;
    IOMgr::Input* m_return;
    IOMgr::Input* m_logDelayTime;
    IOMgr::Input* m_feedback;
    IOMgr::Input* m_rotate;
    IOMgr::Input* m_widen;

    IOMgr::Input* m_modulationVOct;
    IOMgr::Input* m_modulationDepth;
    IOMgr::Input* m_modulationPhaseOffset;
    IOMgr::Input* m_modulationCrossDepth;

    IOMgr::Input* m_dampingBFFBase;
    IOMgr::Input* m_dampingBFFWidth;
    
    IOMgr::Output* m_output;
    IOMgr::Output* m_lfoOut;

    QuadDelay()
        : m_ioMgr(this)
    {
        m_input = m_ioMgr.AddInput("Input", true);
        m_return = m_ioMgr.AddInput("Return", true);

        m_logDelayTime = m_ioMgr.AddInput("Log Delay Time", true);
        m_logDelayTime->SetTarget(0, &m_logDelayTimeVal);

        m_feedback = m_ioMgr.AddInput("Feedback", true);
        m_feedback->m_scale = 0.1;
        m_feedback->SetTarget(0, &m_state.m_feedback);

        if (!IsReverb)
        {            
            m_rotate = m_ioMgr.AddInput("Rotate", true);
            m_rotate->m_scale = 0.1;
            m_rotate->SetTarget(0, &m_state.m_rotate);
        }
        
        m_widen = m_ioMgr.AddInput("Widen", true);
        m_widen->m_scale = 0.1;
        m_widen->SetTarget(0, &m_state.m_widen);

        m_modulationVOct = m_ioMgr.AddInput("Modulation VOct", true);
        m_modulationVOct->m_scale = 0.5;
        m_modulationVOct->SetTarget(0, &m_modulationVOctVal);

        m_modulationDepth = m_ioMgr.AddInput("Modulation Depth", true);
        m_modulationDepth->m_scale = 0.1;
        m_modulationDepth->SetTarget(0, &m_state.m_modDepth);
        
        m_modulationPhaseOffset = m_ioMgr.AddInput("Modulation Phase Offset", true);
        m_modulationPhaseOffset->m_scale = 0.1;
        m_modulationPhaseOffset->SetTarget(0, &m_state.m_lfoInput.m_phaseOffset);

        m_modulationCrossDepth = m_ioMgr.AddInput("Modulation Cross Depth", true);
        m_modulationCrossDepth->m_scale = 0.1;
        m_modulationCrossDepth->SetTarget(0, &m_state.m_lfoInput.m_crossDepth);

        m_dampingBFFBase = m_ioMgr.AddInput("Damping BFF Base", true);
        m_dampingBFFWidth = m_ioMgr.AddInput("Damping BFF Width", true);
        m_dampingBFFWidth->m_scale = 11.0 / 10;

        m_dampingBFFBase->SetTarget(0, &m_logDampingBFFBase);
        m_dampingBFFWidth->SetTarget(0, &m_logDampingBFFWidth);

        m_output = m_ioMgr.AddOutput("Output", true);

        m_lfoOut = m_ioMgr.AddOutput("LFO Output", true);
        m_lfoOut->m_scale = 10;

        for (int i = 0; i < 4; ++i)
        {
            m_input->SetTarget(i, &m_state.m_input[i]);
            m_return->SetTarget(i, &m_state.m_return[i]);
            m_output->SetSource(i, &m_internal.m_output[i]);
            m_lfoOut->SetSource(i, &m_internal.m_lfo.m_output[i]);
        }

        m_ioMgr.Config();
        
        m_output->SetChannels(4);
        m_lfoOut->SetChannels(4);
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        m_state.m_delayTimeSamples = powf(2, m_logDelayTimeVal) * 60;
        m_state.m_lfoInput.m_freq = powf(2, m_modulationVOctVal) * 0.05 * args.sampleTime;
        m_internal.m_lfo.SetSlew(1.0 / (64 * 0.05), args.sampleTime);

        m_state.m_bffBase = powf(2, m_logDampingBFFBase) / 2048;
        m_state.m_bffWidth = powf(2, m_logDampingBFFWidth);
        

        m_internal.Process(m_state);
        m_ioMgr.SetOutputs();
    }
};

template<bool IsReverb>
struct QuadDelayWidget : public ModuleWidget
{
    QuadDelayWidget(QuadDelay<IsReverb>* module)
    {
        setModule(module);
        
        if (IsReverb)
        {
            setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadReverb.svg")));
        }
        else
        {
            setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/QuadDelay.svg")));
        }

        if (module)
        {
            module->m_input->Widget(this, 1, 1);
            module->m_return->Widget(this, 1, 2);
            module->m_logDelayTime->Widget(this, 2, 1);
            module->m_feedback->Widget(this, 2, 2);
            
            if (!IsReverb)
            {
                module->m_rotate->Widget(this, 2, 3);
            }
            
            module->m_widen->Widget(this, 2, 4);
            module->m_modulationVOct->Widget(this, 3, 1);
            module->m_modulationDepth->Widget(this, 3, 2);
            module->m_modulationPhaseOffset->Widget(this, 3, 3);
            module->m_modulationCrossDepth->Widget(this, 3, 4);
            module->m_dampingBFFBase->Widget(this, 4, 1);
            module->m_dampingBFFWidth->Widget(this, 4, 2);
            module->m_output->Widget(this, 5, 1);
            module->m_lfoOut->Widget(this, 5, 2);
        }
    }
};
#endif
            


    
    
