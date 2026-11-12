#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "Filter.hpp"
#include "Saturator.hpp"
#include "Oversample.hpp"
#include "PhaseUtils.hpp"
#include "TheoryOfTime.hpp"
#include "TapeHead.hpp"
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
    
    // The delay line stores host samples; heads use continuous master phase.
    // Input and feedback are decimated once per host frame; grains play at
    // engine rate. TheoryOfTime owns the inverse wired into the line at startup.
    //
    static constexpr size_t x_oversample = SampleTimer::x_oversample;
    static constexpr size_t x_delayLineSize = TheoryOfTime::x_timeWarpInverterSamples / x_oversample;
    using Writer = QuadDelayLineMovableWriter<x_delayLineSize>;
    using WriteDecimator = DownsamplerGeneric<float, 4, 8, x_oversample>;
    Writer m_delayLine;
    WriteDecimator m_writeDecimator;
    size_t m_writeSubSample;
    QuadGrainManager<Writer> m_grainManager;

    ScopeWriterHolderGeneric<4> m_envelopeScopeWriter;
    QuadFloat m_envelopeValue;

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

        double m_writeHeadPhase;
        QuadDouble m_readHeadPhase;
        float m_relativeWriteHeadPosition;
        QuadFloat m_relativeReadHeadPosition;

        QuadFloat m_bffBase;
        QuadFloat m_bffWidth;

        QuadGrainManager<Writer>::Input m_grainManagerInput;

        Input()
            : m_feedback(0.5, 0.5, 0.5, 0.5)
            , m_rotate(0, 0, 0, 0)
            , m_writeHeadPhase(0)
            , m_relativeWriteHeadPosition(-1.0f)
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
        QuadOPBaseWidthFilter::UIState m_dampingFilter;
        ScopeWriterGeneric<4, 1> m_scopeWriter;
        std::atomic<double> m_readHeadPhase[4]{};
        std::atomic<double> m_writeHeadPhase{0.0};
    };

    void SetupScopeWriter(UIState* uiState)
    {
        assert(m_delayLine.m_inverter);
        uiState->m_scopeWriter.SetTimeWarp(0, m_delayLine.m_inverter);
        m_envelopeScopeWriter = ScopeWriterHolderGeneric<4>(&uiState->m_scopeWriter, 0);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            uiState->m_scopeWriter.SetPeriodic(0, channel, {0.0, 1.0});
        }

    }

    SampledSnapshotWriter<QuadOPBaseWidthFilter::UIState> m_responseSampler;

    void SetupResponseUIState(UIState* uiState)
    {
        m_responseSampler.Bind(&uiState->m_dampingFilter);
    }

    void PopulateUIStateSample()
    {
        m_postFeedbackFilter.m_bff.PopulateUIStateSample(m_responseSampler);
    }

    void PopulateUIState(UIState* uiState, Input& input)
    {
        for (size_t channel = 0; channel < 4; ++channel)
        {
            uiState->m_readHeadPhase[channel].store(input.m_readHeadPhase[channel]);
        }

        uiState->m_writeHeadPhase.store(input.m_writeHeadPhase);
        uiState->m_scopeWriter.Publish();
    }

    // The scope holds the magnitude of the latest completed host write between
    // writes. Its real-time engine grid is shared with the control scopes.
    //
    void WriteAudio(const QuadFloat& audio, size_t hostSample)
    {
        m_delayLine.Write(audio, hostSample);
        m_envelopeValue = audio.Abs();
    }

    void WriteEnvelopeScope()
    {
        m_envelopeScopeWriter.Write(m_envelopeValue);
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
            WriteAudio(m_writeDecimator.GetOutput(), SampleTimer::GetHostSample());
        }

        WriteEnvelopeScope();

        QuadFloat delayedSignal = m_grainManager.Process(input.m_readHeadPhase, QuadFloat(), input.m_grainManagerInput,
            m_delayLine.GetOldestSampleTime(), m_delayLine.GetLatestSampleTime());

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

    QuadZeroedExpParam m_feedback;

    // Double state: 1 Hz one-poles at the engine rate stall short of their
    // targets in float.
    //

    OPLowPassFilterGeneric<double, 4> m_rotateFilter;

    QuadExpParam m_unisonDetune;
    QuadZeroedExpParam m_unisonGain;
    QuadExpParam m_slewUp;

    float m_bufferFrac[4];
    ReadTapeHead m_readTapeHead[4];

    struct Input
    {
        int m_delayTimeFactorSwitchVal[4];
        int m_readHeadSpeedSwitchVal[4];
        int m_loopSelectorSwitchVal[4];
        int m_rotateSwitchVal[4];
        QuadFloat m_feedbackKnob;
        QuadFloat m_dampingBaseKnob;
        QuadFloat m_dampingWidthKnob;

        QuadFloat m_resynthShiftFadeKnob;
        QuadFloat m_resynthUnisonKnob;
        int m_resynthShiftSwitchVal[4];
        QuadFloat m_resynthSlewUpKnob;
        QuadFloat m_phaseModulationKnob;
        
        TheoryOfTime* m_theoryOfTime;

        Input()
            : m_feedbackKnob()
            , m_dampingBaseKnob()
            , m_dampingWidthKnob()
            , m_resynthShiftFadeKnob()
            , m_resynthUnisonKnob()
            , m_resynthSlewUpKnob()
            , m_phaseModulationKnob()
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
      : m_dampingBase(SampleTimer::HzToCycPerSamp(QuadOPBaseWidthFilter::x_minBaseHz), SampleTimer::HzToCycPerSamp(QuadOPBaseWidthFilter::x_maxBaseHz))
      , m_dampingWidth(1.0, 2048.0)
      , m_unisonDetune(ResynthesizerGeneric<float, 4>::GetUnisonBase())
      , m_unisonGain(20)
      , m_slewUp(0.25 / Resynthesizer::GetGrainLaunchSamples(), 0.5)
    {
        m_rotateFilter.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(1.0));
        m_feedback.SetBaseByCenter(0.25);
        for (int i = 0; i < 4; ++i)
        {
            m_bufferFrac[i] = 1.0;
        }
    }

    // TheoryOfTime has already advanced continuous phase this sample.
    //
    void Process(Input& input, QuadDelay::Input& delayInput)
    {
        double writePhase = input.m_theoryOfTime->m_phaseClock.m_phase;
        delayInput.m_writeHeadPhase = writePhase;
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
        delayInput.m_grainManagerInput.m_phaseModulation = input.m_phaseModulationKnob *
            QuadGrainManager<QuadDelay::Writer>::SynthesisOLA::x_maxPhaseModulation;

        QuadFloat unisonDetune = m_unisonDetune.Update(input.m_resynthUnisonKnob);
        QuadFloat unisonGain = m_unisonGain.Update(QuadFloat::Min(QuadFloat::Splat(1.0f), input.m_resynthUnisonKnob * 3.0f));
        QuadFloat slewUp = m_slewUp.Update(QuadFloat::Splat(1.0f) - input.m_resynthSlewUpKnob);

        for (int i = 0; i < 4; ++i)
        {
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
            readInput.m_bufferFraction = m_bufferFrac[i];
            readInput.m_readHeadSpeed = possibleReadHeadSpeeds[input.m_readHeadSpeedSwitchVal[i]];
            readInput.m_requestedLoopSelector = totLoopSelector;
            m_readTapeHead[i].Update(readInput);
            double readPhase = m_readTapeHead[i].m_phase;
            delayInput.m_readHeadPhase[i] = readPhase;
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

    }
};
