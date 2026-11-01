#pragma once

#include "SquiggleBoySource.hpp"
#include "VoiceMachineEnums.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "Filter.hpp"
#include "Slew.hpp"
#include "LadderFilter.hpp"
#include "StateVariableFilter.hpp"
#include "AHD.hpp"
#include "BitCrush.hpp"
#include "Lissajous.hpp"
#include "PolyXFader.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SampleTimer.hpp"
#include "RecordingBuffer.hpp"
#include "SquiggleBoyVoiceConfig.hpp"
#include "RecordingManager.hpp"
#include "Saturator.hpp"
#include "Oversample.hpp"
#include "VCO.hpp"

#include <algorithm>
#include <cstring>

struct TheoryOfTime;
struct AudioBufferBank;

struct SquiggleBoyTrack
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;
    static constexpr size_t x_recordingBufferWriterSize = RecordingManager::x_numVoices;

    using VoiceConfig = SquiggleBoyVoiceConfig;

    struct FilterSection
    {
        static constexpr float x_lpPitchTracking = 0.707f;
        static constexpr float x_lpTrackingPivot = SampleTimer::HzToCycPerSamp(261.6256f);

        // Ladder filters (4-pole)
        //
        LadderFilterLP m_lpLadder;
        LinearSVF4PoleHighPass m_hp4Pole;

        // SVF filters (2-pole)
        //
        LinearStateVariableFilter m_lpSVF;
        LinearStateVariableFilter m_hpSVF;

        TanhSaturator<true> m_saturator;

        // DC blocker before SVF (5 Hz at oversample rate)
        //
        OPHighPassFilter m_svfDCBlocker;

        SampleRateReducer m_sampleRateReducer;

        float m_output;
        float m_uBlockOutput[x_uBlockSize];

        ScopeWriterHolder m_scopeWriter;

        // Slewed parameters
        //
        ParamSlew m_vcoBaseFreqSlew;
        ParamSlew m_lpCutoffSlew;
        ParamSlew m_hpCutoffSlew;
        ParamSlew m_lpResonanceSlew;
        ParamSlew m_hpResonanceSlew;
        ParamSlew m_saturationGainSlew;
        ParamSlew m_sampleRateReducerFreqSlew;

        FilterSection()
            : m_output(0)
            , m_vcoBaseFreqSlew(x_oversample)
            , m_lpCutoffSlew(x_oversample)
            , m_hpCutoffSlew(x_oversample)
            , m_lpResonanceSlew(x_oversample)
            , m_hpResonanceSlew(x_oversample)
            , m_saturationGainSlew(x_oversample)
            , m_sampleRateReducerFreqSlew(x_oversample)
        {
            // 5 Hz at the oversampled rate
            //
            m_svfDCBlocker.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(5.0) / x_oversample);
            m_saturator.SetInputGain(0.25f);
        }

        void ProcessLadder4Pole(float input, float hpFreq, float lpFreq, float hpResonance, float lpResonance, float saturationGain)
        {
            m_hp4Pole.SetCutoff(hpFreq);
            m_hp4Pole.SetResonance(hpResonance);
            m_lpLadder.SetCutoff(lpFreq);
            m_lpLadder.SetResonance(lpResonance);
            m_lpLadder.SetSaturationGain(saturationGain);

            m_output = m_lpLadder.Process(input);
            m_hp4Pole.Process(m_output);
            m_output = m_hp4Pole.GetOutput();
        }

        void ProcessSVF2Pole(float input, float hpFreq, float lpFreq, float hpResonance, float lpResonance, float saturationGain)
        {
            m_hpSVF.SetCutoff(hpFreq);
            m_hpSVF.SetResonance(hpResonance);
            m_lpSVF.SetCutoff(lpFreq);
            m_lpSVF.SetResonance(lpResonance);
            m_saturator.SetInputGain(saturationGain);
            
            m_lpSVF.Process(m_saturator.Process(input));
            m_output = m_saturator.Process(m_lpSVF.GetLowPass());
            m_hpSVF.Process(m_output);
            m_output = m_saturator.Process(m_hpSVF.GetHighPass());
        }

        struct Input
        {
            float m_vcoBaseFreq;
            PhaseUtils::ExpParam m_lpCutoffFactor;
            PhaseUtils::ExpParam m_hpCutoffFactor;
            PhaseUtils::ZeroedExpParam m_lpResonance;
            PhaseUtils::ZeroedExpParam m_hpResonance;
            PhaseUtils::ExpParam m_saturationGain;

            VoiceConfig* m_voiceConfig;

            PhaseUtils::ExpParam m_sampleRateReducerFreq;

            Input()
                : m_vcoBaseFreq(PhaseUtils::VOctToNatural(0.0, SampleTimer::x_samplePeriod))
                , m_lpCutoffFactor(0.25f, 1024.0f)
                , m_hpCutoffFactor(0.25f, 256.0f)
                , m_saturationGain(0.25f, 5.0f)
                , m_sampleRateReducerFreq(1.0f, 2048.0f)
            {
                m_lpResonance.SetBaseByCenter(0.125);
                m_hpResonance.SetBaseByCenter(0.125);
            }
        };

        void ProcessUBlock(Input& input, const float* vcoOutput, const SampleTop* top)
        {
            float baseFreq = input.m_vcoBaseFreq;
            if (input.m_voiceConfig->m_sourceMachine == VoiceConfig::SourceMachine::Thru)
            {
                baseFreq = SampleTimer::HzToCycPerSamp(80.0);
            }

            // Update slew targets
            //
            m_vcoBaseFreqSlew.Update(baseFreq);
            m_lpCutoffSlew.Update(input.m_lpCutoffFactor.m_expParam);
            m_hpCutoffSlew.Update(input.m_hpCutoffFactor.m_expParam);
            m_lpResonanceSlew.Update(input.m_lpResonance.m_expParam);
            m_hpResonanceSlew.Update(input.m_hpResonance.m_expParam);
            m_saturationGainSlew.Update(input.m_saturationGain.m_expParam);
            m_sampleRateReducerFreqSlew.Update(input.m_sampleRateReducerFreq.m_expParam);

            for (size_t i = 0; i < x_uBlockSize; ++i)
            {
                float vcoBaseFreq = m_vcoBaseFreqSlew.Process();
                float lpCutoff = m_lpCutoffSlew.Process();
                float hpCutoff = m_hpCutoffSlew.Process();
                float lpResonance = m_lpResonanceSlew.Process();
                float hpResonance = m_hpResonanceSlew.Process();
                float sampleRateReducerFreq = m_sampleRateReducerFreqSlew.Process();
                float saturationGain = m_saturationGainSlew.Process();

                // Compute filter frequencies adjusted for oversampled rate
                //
                float hpFreq = std::min<float>(0.5 / x_oversample, vcoBaseFreq * hpCutoff / x_oversample);
                float lpBaseFreq = x_lpTrackingPivot * std::pow(vcoBaseFreq / x_lpTrackingPivot, x_lpPitchTracking);
                float lpFreq = std::min<float>(0.5 / x_oversample, lpBaseFreq * hpCutoff * lpCutoff / x_oversample);

                float dcBlocked = m_svfDCBlocker.Process(vcoOutput[i]);

                if (input.m_voiceConfig->m_filterMachine == VoiceConfig::FilterMachine::Ladder4Pole)
                {
                    ProcessLadder4Pole(dcBlocked, hpFreq, lpFreq, hpResonance, lpResonance, saturationGain);
                }
                else
                {
                    ProcessSVF2Pole(dcBlocked, hpFreq, lpFreq, hpResonance, lpResonance, saturationGain);
                }

                m_sampleRateReducer.SetFreq(vcoBaseFreq * sampleRateReducerFreq / x_oversample);
                m_output = m_sampleRateReducer.Process(m_output);

                m_uBlockOutput[i] = m_output;

                // Base-rate operations every x_oversample samples
                //
                if ((i + 1) % x_oversample == 0)
                {
                    size_t baseIndex = i / x_oversample;

                    m_scopeWriter.Write(baseIndex, m_output);
                    if (top[baseIndex])
                    {
                        m_scopeWriter.RecordStart(top[baseIndex].GetPosition(baseIndex));
                    }
                }
            }
        }

        void DebugPrint()
        {
            m_lpLadder.DebugPrint();
        }
    };

    struct AmpSection
    {
        static constexpr float x_subSlewMinSeconds = 0.001f;
        static constexpr float x_subRiseMaxSeconds = 1.0f;
        static constexpr float x_subFallMaxSeconds = 5.0f;

        AHD m_ahd;
        AHD m_modulationAHD;

        float m_output;
        float m_subOut;

        OPLowPassFilter m_freqDependentGainSlew;
        float m_freqDependentGainTarget;

        BiDirectionalSlew m_subSlew;

        ScopeWriterHolder m_scopeWriter;
        ScopeWriterHolder m_subScopeWriter;
        ScopeWriterHolder m_modulationEnvelopeScopeWriter;
        ScopeWriterHolder m_ampEnvelopeScopeWriter;
        bool m_subRunning;

        struct Input
        {
            float m_vcoTargetFreq;
            AHD::InputSetter m_ahdInputSetter;
            AHD::Input m_ahdInput;
            AHD::InputSetter m_modulationAHDInputSetter;
            AHD::Input m_modulationAHDInput;

            PhaseUtils::ZeroedExpParam m_gain;
            PhaseUtils::ZeroedExpParam m_subGain;
            PhaseUtils::ExpParam m_subTanhGain;
            PhaseUtils::ExpParam m_subRiseSeconds;
            PhaseUtils::ExpParam m_subFallSeconds;

            ScopeWriterHolder m_scopeWriter;
            TanhSaturator<true> m_subTanhSaturator;

            VoiceConfig* m_voiceConfig;

            bool m_subTrig;

            void SetAHD(float attack, float hold, float decay, float amplitude)
            {
                m_ahdInputSetter.Set(attack, hold, decay, amplitude, false, m_ahdInput);
            }

            void SetModulationAHD(float attack, float hold, float decay, float amplitude)
            {
                m_modulationAHDInputSetter.Set(attack, hold, decay, amplitude, true, m_modulationAHDInput);
            }

            Input()
                : m_subTanhGain(0.125f, 32.0f)
                , m_subRiseSeconds(x_subSlewMinSeconds, x_subRiseMaxSeconds)
                , m_subFallSeconds(x_subSlewMinSeconds, x_subFallMaxSeconds)
            {
                m_subTrig = false;
            }
        };

        AmpSection()
            : m_output(0)
            , m_subOut(0)
        {
            m_freqDependentGainSlew.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(250.0));
            m_subRunning = false;
        }

        void WriteEnvelopeScopes()
        {
            if (SampleTimer::IsControlFrame())
            {
                m_modulationEnvelopeScopeWriter.Write(m_modulationAHD.m_output);
                m_ampEnvelopeScopeWriter.Write(m_ahd.m_output);
            }

            if (m_modulationAHD.m_triggered)
            {
                m_modulationEnvelopeScopeWriter.RecordStart();
            }

            if (m_modulationAHD.m_changed && m_modulationAHD.m_state == AHD::State::Idle)
            {
                m_modulationEnvelopeScopeWriter.RecordEnd();
            }

            if (m_ahd.m_triggered)
            {
                m_ampEnvelopeScopeWriter.RecordStart();
            }

            if (m_ahd.m_changed && m_ahd.m_state == AHD::State::Idle)
            {
                m_ampEnvelopeScopeWriter.RecordEnd();
            }
        }

        float Process(Input& input, float filterOutput, float sub)
        {
            if (input.m_ahdInput.m_trig)
            {
                float vcoTargetHz = static_cast<float>(SampleTimer::CycPerSampToHz(input.m_vcoTargetFreq));
                if (vcoTargetHz < 100 || !VoiceMachine::IsSynthLike(input.m_voiceConfig->m_sourceMachine))
                {
                    m_freqDependentGainTarget = 1.0;
                }
                else
                {
                    m_freqDependentGainTarget = 1.0 / std::sqrtf(vcoTargetHz / 100);
                }

                if (m_ahd.m_state != AHD::State::Idle)
                {
                    m_scopeWriter.RecordEnd();
                }

                m_subRunning = input.m_subTrig;
            }

            input.m_subTanhSaturator.SetInputGain(input.m_subTanhGain.m_expParam);
            sub = input.m_subTanhSaturator.Process(sub);

            float freqDependentGain = m_freqDependentGainSlew.Process(m_freqDependentGainTarget);

            // AHD::Process now applies amplitude and polarity internally
            //
            input.m_modulationAHDInput.m_samplePosition = static_cast<float>(SampleTimer::GetUBlockIndex());
            m_modulationAHD.Process(input.m_modulationAHDInput);
            input.m_ahdInput.m_samplePosition = static_cast<float>(SampleTimer::GetUBlockIndex());
            float ahdEnv = PhaseUtils::ZeroedExpParam::Compute(10.0f, m_ahd.Process(input.m_ahdInput));

            constexpr float x_slewTimeToCycles = static_cast<float>(SampleTimer::HzToCycPerSamp(1.0 / (2.0 * M_PI)));
            m_subSlew.SetSlewUp(x_slewTimeToCycles / input.m_subRiseSeconds.m_expParam);
            m_subSlew.SetSlewDown(x_slewTimeToCycles / input.m_subFallSeconds.m_expParam);
            float subEnvelope = m_subSlew.Process(m_subRunning ? static_cast<float>(input.m_subGain.m_expParam * ahdEnv) : 0.0f);
            float mainOut = freqDependentGain * ahdEnv * filterOutput;
            float subOut = freqDependentGain * subEnvelope * sub;
            
            m_scopeWriter.Write(mainOut);
            m_subScopeWriter.Write(subOut);
            if (m_ahd.m_triggered)
            {
                m_scopeWriter.RecordStart();
            }

            if (m_ahd.m_changed && m_ahd.m_state == AHD::State::Idle)
            {
                m_scopeWriter.RecordEnd();
            }

            WriteEnvelopeScopes();

            m_subOut = subOut * input.m_gain.m_expParam;
            m_output = mainOut * input.m_gain.m_expParam;

            return m_output;
        }
    };

    struct PanSection
    {
        LissajousLFOInternal m_lissajousLFO;
        float m_outputX;
        float m_outputY;

        PanSection()
            : m_outputX(0)
            , m_outputY(0)
        {
        }

        struct Input : LissajousLFOInternal::Input
        {
            float m_input;

            Input()
                : m_input(0)
            {
            }
        };

        void Process(Input& input)
        {
            m_lissajousLFO.Process(input.m_input, input);
            m_outputX = m_lissajousLFO.m_outputX * 0.5 + 0.5;
            m_outputY = m_lissajousLFO.m_outputY * 0.5 + 0.5;
        }
    };

    struct SquiggleLFO
    {
        static constexpr size_t x_numPhasors = 6;

        PolyXFaderInternal m_polyXFader;
        float m_output;

        ScopeWriterHolder m_scopeWriter;

        struct Input
        {
            PolyXFaderInternal::Input m_polyXFaderInput;
            PhaseUtils::ExpParam m_mult;

            Input()
                : m_mult(1, 16)
            {
                m_polyXFaderInput.m_size = x_numPhasors;
            }
        };

        float Process(Input& input)
        {
            input.m_polyXFaderInput.m_mult = input.m_mult.m_expParam;
            input.m_polyXFaderInput.m_samplePosition = static_cast<float>(SampleTimer::GetUBlockIndex());
            m_polyXFader.Process(input.m_polyXFaderInput);
            m_output = m_polyXFader.m_rawOutput;

            if (SampleTimer::IsControlFrame())
            {
                m_scopeWriter.Write(m_polyXFader.m_output);
            }

            if (m_polyXFader.m_top[0])
            {
                m_scopeWriter.RecordStart(m_polyXFader.m_top[0].GetControlPosition(SampleTimer::GetUBlockIndex()));
            }

            return m_output;
        }

        SquiggleLFO()
            : m_output(0)
        {
        }
    };

    void SetupAudioScopeWriters(ScopeWriter* scopeWriter, size_t voiceIx)
    {
        m_source.m_dualWaveShapingVCO.m_scopeWriter[0] = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::VCO1));
        m_source.m_dualWaveShapingVCO.m_scopeWriter[1] = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::VCO2));
        m_filter.m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::PostFilter));
        m_amp.m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp));
        m_amp.m_subScopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::Sub));
    }

    void SetupControlScopeWriters(ScopeWriter* scopeWriter, size_t voiceIx)
    {
        m_squiggleLFO[0].m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::ControlScopes::LFO1));
        m_squiggleLFO[1].m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::ControlScopes::LFO2));
        m_amp.m_modulationEnvelopeScopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::ControlScopes::ModulationEnvelope));
        m_amp.m_ampEnvelopeScopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::ControlScopes::AmpEnvelope));
    }

    OPLowPassFilter m_baseFreqSlew;
    PhaseUtils::ExpParam m_baseFreqSlewAmount;
    SquiggleBoySource m_source;
    VCO m_sub;
    FilterSection m_filter;
    AmpSection m_amp;
    PanSection m_pan;
    SquiggleLFO m_squiggleLFO[2];

    Downsampler m_downsampler;
    float m_uBlockFilterOut[SampleTimer::x_controlFrameRate];

    float m_output;

    RecordingBuffer m_recordingBuffer;
    RecordingBufferWriter<x_recordingBufferWriterSize> m_recordingBufferWriter;

    struct Input
    {
        struct SubInput
        {
            float m_baseFreq;
        };

        VoiceConfig m_voiceConfig;
        AHD::AHDControl m_ahdControl;

        SubInput m_subInput;
        SquiggleBoySource::Input m_sourceInput;
        FilterSection::Input m_filterInput;
        AmpSection::Input m_ampInput;
        PanSection::Input m_panInput;
        SquiggleLFO::Input m_squiggleLFOInput[2];

        Input()
        {
            m_filterInput.m_voiceConfig = &m_voiceConfig;
            m_ampInput.m_voiceConfig = &m_voiceConfig;
        }

        void SetGates()
        {            
            m_ampInput.m_ahdInput.Set(m_ahdControl);
            m_ampInput.m_modulationAHDInput.Set(m_ahdControl);
            m_sourceInput.m_physicalModelingInput.m_ahdInput.Set(m_ahdControl);

            m_squiggleLFOInput[0].m_polyXFaderInput.m_trig[0] = m_ahdControl.m_trig;
            m_squiggleLFOInput[1].m_polyXFaderInput.m_trig[0] = m_ahdControl.m_trig;
        }

        void SetTheoryOfTimeInputs(TheoryOfTime* theoryOfTime)
        {
            m_sourceInput.m_theoryOfTime = theoryOfTime;
        }
    };

    SquiggleBoyTrack()
        : m_baseFreqSlewAmount(SampleTimer::HzToCycPerSamp(0.125), SampleTimer::HzToCycPerSamp(500.0))
        , m_downsampler(x_oversample)
        , m_output(0)
    {
    }

    void ProcessUBlock(Input& input)
    {
        input.m_sourceInput.m_sourceMachine = input.m_voiceConfig.m_sourceMachine;
        input.m_sourceInput.m_sourceAssignment = input.m_voiceConfig.m_sourceAssignment;
        input.m_sourceInput.m_audioBufferBank = input.m_voiceConfig.m_audioBufferBank;

        m_source.ProcessUBlock(input.m_sourceInput);

        m_filter.ProcessUBlock(input.m_filterInput, m_source.m_uBlockOutput, m_source.m_uBlockTop);
        m_downsampler.Process(m_filter.m_uBlockOutput, m_uBlockFilterOut);
    }

    void DebugPrint()
    {
        m_filter.DebugPrint();
    }

    float Processs(Input& input)
    {
        if (SampleTimer::IsControlFrame())
        {
            ProcessUBlock(input);
        }

        size_t uBlockIndex = SampleTimer::GetUBlockIndex();
        float sub = m_sub.Process(input.m_subInput.m_baseFreq / 2);
        m_output = m_amp.Process(input.m_ampInput, m_uBlockFilterOut[uBlockIndex], sub);
        m_pan.Process(input.m_panInput);
        m_squiggleLFO[0].Process(input.m_squiggleLFOInput[0]);
        m_squiggleLFO[1].Process(input.m_squiggleLFOInput[1]);

        m_recordingBufferWriter.Process(m_output);
        return m_output;
    }
};
