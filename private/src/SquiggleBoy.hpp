#pragma once

#include "SquiggleBoySource.hpp"
#include "VoiceMachineEnums.hpp"
#include "SmartGridOneEncoders.hpp"
#include "ButterworthFilter.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "RandomLFO.hpp"
#include "LadderFilter.hpp"
#include "StateVariableFilter.hpp"
#include "AHD.hpp"
#include "BitCrush.hpp"
#include "Lissajous.hpp"
#include "QuadMixer.hpp"
#include "QuadDelay.hpp"
#include "QuadReverb.hpp"
#include "PolyXFader.hpp"
#include "GangedRandomLFO.hpp"
#include "ScopeWriter.hpp"
#include "RandomWaveTable.hpp"
#include "MultibandSaturator.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SourceMixer.hpp"
#include "VCO.hpp"
#include "DeepVocoder.hpp"
#include "KMixMidi.hpp"
#include "Math.hpp"
#include "SampleTimer.hpp"
#include "StateSaver.hpp"
#include "AudioBuffer.hpp"
#include "DirectoryExplorer.hpp"
#include "Oversample.hpp"
#include "TransferFunction.hpp"

#include <cstring>

struct TheoryOfTime;
struct IoTaskThread;
struct AudioBufferBank;

struct SquiggleBoyVoice
{
    static constexpr size_t x_oversample = 4;
    static constexpr size_t x_uBlockSize = SampleTimer::x_controlFrameRate * x_oversample;

    struct VoiceConfig
    {
        using SourceMachine = VoiceMachine::SourceMachine;
        using FilterMachine = VoiceMachine::FilterMachine;

        SourceMachine m_sourceMachine;
        FilterMachine m_filterMachine;
        int m_sourceIndex;

        AudioBufferBank* m_audioBufferBank0;
        AudioBufferBank* m_audioBufferBank1;

        VoiceConfig()
            : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
            , m_filterMachine(FilterMachine::Ladder4Pole)
            , m_sourceIndex(0)
            , m_audioBufferBank0(nullptr)
            , m_audioBufferBank1(nullptr)
        {
        }
    };

    struct FilterSection
    {
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
            // 5 Hz at 192kHz (48kHz * 4x oversample)
            //
            m_svfDCBlocker.SetAlphaFromNatFreq(5.0f / 192000.0f);
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
                : m_vcoBaseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
                , m_lpCutoffFactor(0.25f, 1024.0f)
                , m_hpCutoffFactor(0.25f, 256.0f)
                , m_saturationGain(0.25f, 5.0f)
                , m_sampleRateReducerFreq(1.0f, 2048.0f)
            {
                m_lpResonance.SetBaseByCenter(0.125);
                m_hpResonance.SetBaseByCenter(0.125);
            }
        };

        void ProcessUBlock(Input& input, const float* vcoOutput, const bool* top)
        {
            float baseFreq = input.m_vcoBaseFreq;
            if (input.m_voiceConfig->m_sourceMachine == VoiceConfig::SourceMachine::Thru)
            {
                baseFreq = 80.0 / SampleTimer::x_sampleRate;
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
                float lpFreq = std::min<float>(0.5 / x_oversample, vcoBaseFreq * hpCutoff * lpCutoff / x_oversample);

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
                        m_scopeWriter.RecordStart(baseIndex);
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
        AHD m_ahd;
        AHD m_modulationAHD;

        float m_output;
        float m_subOut;

        OPLowPassFilter m_freqDependentGainSlew;
        float m_freqDependentGainTarget;

        OPLowPassFilter m_subFilter;

        ScopeWriterHolder m_scopeWriter;
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

            OPLowPassFilter m_gainSlew;
            PhaseUtils::ZeroedExpParam m_gain;
            PhaseUtils::ZeroedExpParam m_subGain;
            PhaseUtils::ExpParam m_subTanhGain;

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
            {
                m_subTrig = false;
                m_subTanhGain = PhaseUtils::ExpParam(0.125, 8.0);
                m_gainSlew.SetAlphaFromNatFreq(500.0f / 48000.0f);
            }
        };

        AmpSection()
            : m_output(0)
        {
            m_freqDependentGainSlew.SetAlphaFromNatFreq(250.0f / 48000.0f);
            m_subFilter.SetAlphaFromNatFreq(170.0f / 48000.0f);
            m_subRunning = false;
        }

        void WriteEnvelopeScopes(Input& input)
        {
            if (SampleTimer::IsControlFrame())
            {
                m_modulationEnvelopeScopeWriter.Write(m_modulationAHD.m_output);
                m_ampEnvelopeScopeWriter.Write(m_ahd.m_output);
            }

            if (input.m_modulationAHDInput.m_trig)
            {
                m_modulationEnvelopeScopeWriter.RecordStart();
            }

            if (m_modulationAHD.m_changed && m_modulationAHD.m_state == AHD::State::Idle)
            {
                m_modulationEnvelopeScopeWriter.RecordEnd();
            }

            if (input.m_ahdInput.m_trig)
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
                if (input.m_vcoTargetFreq * 48000 < 100 || input.m_voiceConfig->m_sourceMachine == VoiceConfig::SourceMachine::Thru)
                {
                    m_freqDependentGainTarget = 1.0;
                }
                else
                {
                    m_freqDependentGainTarget = 1.0 / std::sqrtf(input.m_vcoTargetFreq * 48000 / 100);
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

            float subGain = m_subFilter.Process(m_subRunning ? input.m_subGain.m_expParam * ahdEnv : 0.0f);            
            float mainOut = freqDependentGain * ahdEnv * filterOutput;
            float subOut = freqDependentGain * subGain * sub;
            
            m_scopeWriter.Write(mainOut + m_subOut);
            if (input.m_ahdInput.m_trig)
            {
                m_scopeWriter.RecordStart();
            }

            if (m_ahd.m_changed && m_ahd.m_state == AHD::State::Idle)
            {
                m_scopeWriter.RecordEnd();
            }

            WriteEnvelopeScopes(input);

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

            if (m_polyXFader.m_top)
            {
                m_scopeWriter.RecordStart();
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

            m_squiggleLFOInput[0].m_polyXFaderInput.m_trig = m_ahdControl.m_trig;
            m_squiggleLFOInput[1].m_polyXFaderInput.m_trig = m_ahdControl.m_trig;
        }

        void SetTheoryOfTimeInputs(TheoryOfTime* theoryOfTime)
        {
            m_sourceInput.m_theoryOfTime = theoryOfTime;
        }
    };

    SquiggleBoyVoice()
        : m_baseFreqSlewAmount(0.125 / 48000.0, 500.0 / 48000.0)
        , m_downsampler(x_oversample)
        , m_output(0)
    {
    }

    void ProcessUBlock(Input& input)
    {
        input.m_sourceInput.m_sourceMachine = input.m_voiceConfig.m_sourceMachine;
        input.m_sourceInput.m_sourceIndex = input.m_voiceConfig.m_sourceIndex;
        input.m_sourceInput.m_audioBufferBank0 = input.m_voiceConfig.m_audioBufferBank0;
        input.m_sourceInput.m_audioBufferBank1 = input.m_voiceConfig.m_audioBufferBank1;

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

        return m_output;
    }
};

struct SquiggleBoy
{
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_numTracks = 3;
    static constexpr size_t x_voicesPerTrack = x_numVoices / x_numTracks;
    static constexpr size_t x_numGangedRandomLFOs = 4;

    struct MixerInput : QuadMixerInternal::Input
    {
        ScopeWriterHolder m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::NumScopes)];

        MixerInput()
        {
        }
    };

    SquiggleBoyWaveTableGenerator m_waveTableGenerator[2];

    SquiggleBoyVoice m_voices[x_numVoices];
    ManyGangedRandomLFO m_gangedRandomLFO[x_numGangedRandomLFOs];
    ManyGangedRandomLFO m_quadGangedRandomLFO[2];
    ManyGangedRandomLFO m_globalGangedRandomLFO[2];

    SourceMixer m_sourceMixer;
    SourceMixer::Input m_sourceMixerState;

    DeepVocoder m_deepVocoder;
    DeepVocoder::Input m_deepVocoderState;

    QuadMixerInternal m_mixer;

    QuadDelay m_delay;
    QuadReverb m_reverb;


    QuadFloatWithStereoAndSub m_output;

    SquiggleBoyVoice::Input m_state[x_numVoices];
    ManyGangedRandomLFO::Input m_gangedRandomLFOInput[x_numGangedRandomLFOs];
    ManyGangedRandomLFO::Input m_quadGangedRandomLFOInput[2];
    ManyGangedRandomLFO::Input m_globalGangedRandomLFOInput[2];
    MixerInput m_mixerState;

    QuadDelayInputSetter m_delayInputSetter;
    QuadReverbInputSetter m_reverbInputSetter;

    QuadDelay::Input m_delayState;
    QuadReverb::Input m_reverbState;
    TheoryOfTime* m_theoryOfTime;
    PhaseUtils::ZeroedExpParam m_delayToReverbSend;
    PhaseUtils::ZeroedExpParam m_reverbToDelaySend;

    PhaseUtils::SimpleOsc m_panPhase;

    PhaseUtils::ZeroedExpParam m_masterVolume;

    RGen m_rGen;

    bool m_firstFrame;
    bool m_topIndependent;

    StateSaver* m_stateSaver;

    IoTaskThread* m_ioTaskThread;

    SquiggleBoy()
        : m_topIndependent(false)
        , m_stateSaver(nullptr)
        , m_ioTaskThread(nullptr)
    {
        m_mixerState.m_numInputs = x_numVoices + SourceMixer::x_numSources;
        m_mixerState.m_numMonoInputs = x_numVoices;
        m_delayToReverbSend.m_expParam = 0.0;
        m_reverbToDelaySend.m_expParam = 0.0;
        m_theoryOfTime = nullptr;

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_sourceInput.m_sourceMixer = &m_sourceMixer;
        }

        for (size_t i = 0; i < x_numGangedRandomLFOs; ++i)
        {
            m_gangedRandomLFOInput[i].m_gangSize = x_numVoices / x_numTracks;
            m_gangedRandomLFOInput[i].m_time = 6.0;
            m_gangedRandomLFOInput[i].m_sigma = 0.2;
            m_gangedRandomLFOInput[i].m_numGangs = x_numTracks;

            if (i < 2)
            {
                m_globalGangedRandomLFOInput[i].m_gangSize = 1;
                m_globalGangedRandomLFOInput[i].m_time = 6.0;
                m_globalGangedRandomLFOInput[i].m_sigma = 0.2;
                m_globalGangedRandomLFOInput[i].m_numGangs = 1;
                m_quadGangedRandomLFOInput[i].m_gangSize = 4;
                m_quadGangedRandomLFOInput[i].m_time = 6.0;
                m_quadGangedRandomLFOInput[i].m_sigma = 0.2;
                m_quadGangedRandomLFOInput[i].m_numGangs = 1;
            }
        }

        m_firstFrame = true;
    }

    const std::string& GetRecordingDirectory() const
    {
        return m_mixer.m_recordingDirectory;
    }

    bool IsRecording() const
    {
        return m_mixer.m_isRecording;
    }

    void ToggleRecording()
    {
        m_mixer.ToggleRecording(x_numVoices + SourceMixer::x_numSources, 48000);
    }

    void SetRecordingDirectory(const std::string& directory)
    {
        m_mixer.m_recordingDirectory = directory;
    }

    void ProcessSends()
    {
        m_delayState.m_input = m_mixer.m_send[0] + m_mixerState.m_return[1] * m_reverbToDelaySend.m_expParam;
        m_mixerState.m_return[0] = m_delay.Process(m_delayState);
        m_delayState.m_return = m_mixerState.m_return[0];

        m_reverbState.m_input = m_mixer.m_send[1] + m_mixerState.m_return[0] * m_delayToReverbSend.m_expParam;
        m_mixerState.m_return[1] = m_reverb.Process(m_reverbState);
        m_reverbState.m_return = m_mixerState.m_return[1];
    }

    void InitRandomWaveTables()
    {
        for (size_t i = 0; i < 2; ++i)
        {
            for (size_t j = 0; j < x_numTracks; ++j)
            {
                m_waveTableGenerator[i].Clear();
                m_waveTableGenerator[i].GenerateCompletely();
                for (size_t k = 0; k < x_voicesPerTrack; ++k)
                {
                    m_waveTableGenerator[i].SetLeft(&m_voices[j * x_voicesPerTrack + k].m_source.m_dualWaveShapingVCO.m_vco[i]);
                }

                m_waveTableGenerator[i].Clear();
                m_waveTableGenerator[i].GenerateCompletely();
                for (size_t k = 0; k < x_voicesPerTrack; ++k)
                {
                    m_waveTableGenerator[i].SetRight(&m_voices[j * x_voicesPerTrack + k].m_source.m_dualWaveShapingVCO.m_vco[i]);
                }
            }

            m_waveTableGenerator[i].Clear();
        }
    }

    void ProcessWaveTableGenerators()
    {
        for (size_t i = 0; i < 2; ++i)
        {
            m_waveTableGenerator[i].ProcessFrame();
            if (m_waveTableGenerator[i].IsReady())
            {
                for (size_t j = 0; j < x_numTracks; ++j)
                {
                    bool needRight = m_waveTableGenerator[i].m_rightVisible[j];
                    bool needLeft = m_waveTableGenerator[i].m_leftVisible[j];
                    for (size_t k = 0; k < x_voicesPerTrack; ++k)
                    {
                        if (m_gangedRandomLFO[2 + i].m_lfos[j].m_pos[k] < 1)
                        {
                            needLeft = false;
                            m_waveTableGenerator[i].m_leftVisible[j] = true;
                        }

                        if (m_gangedRandomLFO[2 + i].m_lfos[j].m_pos[k] > 0)
                        {
                            needRight = false;
                            m_waveTableGenerator[i].m_rightVisible[j] = true;
                        }
                    }

                    if (needRight)
                    {
                        INFO("New right wavetable for VCO %zu track %zu", i, j);
                        for (size_t k = 0; k < x_voicesPerTrack; ++k)
                        {
                            m_waveTableGenerator[i].SetRight(&m_voices[j * x_voicesPerTrack + k].m_source.m_dualWaveShapingVCO.m_vco[i]);
                        }

                        m_waveTableGenerator[i].m_rightVisible[j] = false;
                        m_waveTableGenerator[i].Clear();
                    }

                    if (needLeft)
                    {
                        INFO("New left wavetable for VCO %zu track %zu", i, j);
                        for (size_t k = 0; k < x_voicesPerTrack; ++k)
                        {
                            m_waveTableGenerator[i].SetLeft(&m_voices[j * x_voicesPerTrack + k].m_source.m_dualWaveShapingVCO.m_vco[i]);
                        }

                        m_waveTableGenerator[i].m_leftVisible[j] = false;
                        m_waveTableGenerator[i].Clear();
                    }
                }                
            }
        }
    }

    void ProcessFrame()
    {
        ProcessWaveTableGenerators();
    }

    void ProcessSample(const AudioInputBuffer& audioInputBuffer)
    {
        m_sourceMixerState.SetInputs(audioInputBuffer);

        if (m_firstFrame)
        {
            m_firstFrame = false;
            InitRandomWaveTables();
        }

        m_panPhase.Process();

        for (size_t j = 0; j < x_numGangedRandomLFOs; ++j)
        {
            m_gangedRandomLFOInput[j].m_topIndependent = m_topIndependent;
            m_gangedRandomLFO[j].Process(1.0 / 48000.0, m_gangedRandomLFOInput[j]);
        }

        m_globalGangedRandomLFOInput[0].m_topIndependent = m_topIndependent;
        m_globalGangedRandomLFOInput[1].m_topIndependent = m_topIndependent;
        m_globalGangedRandomLFO[0].Process(1.0 / 48000.0, m_globalGangedRandomLFOInput[0]);
        m_globalGangedRandomLFO[1].Process(1.0 / 48000.0, m_globalGangedRandomLFOInput[1]);

        m_quadGangedRandomLFOInput[0].m_topIndependent = m_topIndependent;
        m_quadGangedRandomLFOInput[1].m_topIndependent = m_topIndependent;
        m_quadGangedRandomLFO[0].Process(1.0 / 48000.0, m_quadGangedRandomLFOInput[0]);
        m_quadGangedRandomLFO[1].Process(1.0 / 48000.0, m_quadGangedRandomLFOInput[1]);

        if (SampleTimer::IsControlFrame())
        {
            m_sourceMixer.ProcessUBlock(m_sourceMixerState);
        }

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_panInput.m_input = m_panPhase.m_phase;

            m_mixerState.m_input[i] = m_voices[i].Processs(m_state[i]);
            m_mixerState.m_monoIn[i] = m_voices[i].m_amp.m_subOut;
            m_mixerState.m_x[i] = m_voices[i].m_pan.m_outputX;
            m_mixerState.m_y[i] = m_voices[i].m_pan.m_outputY;
        }

        if (SampleTimer::GetSample() % 48000 == 0)
        {
            m_voices[0].DebugPrint();
        }

        m_deepVocoderState.m_enabled = m_sourceMixer.IsVocoderEnabled(m_sourceMixerState);
        float deepVocoderInput = m_sourceMixer.GetDeepVocoderInput(m_sourceMixerState);
        m_deepVocoder.Process(deepVocoderInput, m_deepVocoderState);
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_mixerState.m_input[i + x_numVoices] = m_sourceMixer.m_sources[i].m_output;
            m_mixerState.m_x[i + x_numVoices] = 0.5f;
            m_mixerState.m_y[i + x_numVoices] = 0.5f;
            m_mixerState.m_gain[i + x_numVoices].m_expParam = 1.0; 
            m_mixerState.m_sendGain[i + x_numVoices][0].m_expParam = 0;
            m_mixerState.m_sendGain[i + x_numVoices][1].m_expParam = 0;
        }

        m_mixer.ProcessInputs(m_mixerState);

        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Dry)].Write(m_mixer.m_output.m_output);

        ProcessSends();

        m_output = m_mixer.ProcessReturns(m_mixerState);

        WriteQuadScopes();
        
        // Apply master volume with exponential curve for proper gain response
        //
        float masterVolumeGain = m_masterVolume.m_expParam;
        m_output.m_output = m_output.m_output * masterVolumeGain;
        m_output.m_stereoOutput = m_output.m_stereoOutput * masterVolumeGain;
        m_output.m_sub = m_output.m_sub * masterVolumeGain;
    }

    void WriteQuadScopes()
    {
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Delay)].Write(m_mixerState.m_return[0]);
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Reverb)].Write(m_mixerState.m_return[1]);
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Master)].Write(m_output.m_output);
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Stereo)].Write(
            m_output.m_stereoOutput.CombineToQuad(m_mixer.m_quadToStereoMixdown.m_output));
    }
};

struct SquiggleBoyWithEncoderBank : SquiggleBoy
{
    using BankMode = SmartGridOneEncoders::BankMode;
    using Bank = SmartGridOneEncoders::Bank;
    using Param = SmartGridOneEncoders::Param;

    static constexpr size_t x_numFaders = 16;

    SmartGridOneEncoders m_encoders;
    BitSet16 m_selectedGesture;
    SmartGrid::SceneManager* m_sceneManager;

    struct UIState
    {
        enum class VisualDisplayMode
        {
            Voice,
            Filter,
            PanAndMelody,
            Control,
            Delay,
            Reverb,
            QuadMaster,
            StereoMaster,
            SourceMixer
        };

        struct VoiceFilterUIState : TransferFunction
        {
            // Ladder filters (4-pole)
            //
            LadderFilterLP::UIState m_lpLadder;
            LinearSVF4PoleHighPass::UIState m_hp4Pole;

            // SVF filters (2-pole)
            //
            LinearStateVariableFilter::UIState m_lpSVF;
            LinearStateVariableFilter::UIState m_hpSVF;

            std::atomic<SquiggleBoyVoice::VoiceConfig::FilterMachine> m_filterMachine;

            VoiceFilterUIState()
                : m_filterMachine(SquiggleBoyVoice::VoiceConfig::FilterMachine::Ladder4Pole)
            {
            }

            float FrequencyResponse(float freq) const override
            {
                if (m_filterMachine.load() == SquiggleBoyVoice::VoiceConfig::FilterMachine::Ladder4Pole)
                {
                    float lp = LadderFilterLP::FrequencyResponse(
                        m_lpLadder.m_alpha.load(),
                        m_lpLadder.m_feedback.load(),
                        freq);
                    float hp = LinearSVF4PoleHighPass::FrequencyResponse(
                        m_hp4Pole.m_g.load(),
                        m_hp4Pole.m_k.load(),
                        freq);
                    return lp * hp;
                }
                else
                {
                    float lp = LinearStateVariableFilter::LowPassFrequencyResponse(
                        m_lpSVF.m_g.load(),
                        m_lpSVF.m_k.load(),
                        freq);
                    float hp = LinearStateVariableFilter::HighPassFrequencyResponse(
                        m_hpSVF.m_g.load(),
                        m_hpSVF.m_k.load(),
                        freq);
                    return lp * hp;
                }
            }

            std::complex<float> TransferFunctionValue(float freq) const override
            {
                if (m_filterMachine.load() == SquiggleBoyVoice::VoiceConfig::FilterMachine::Ladder4Pole)
                {
                    std::complex<float> lp = LadderFilterLP::TransferFunction(
                        m_lpLadder.m_alpha.load(),
                        m_lpLadder.m_feedback.load(),
                        freq);
                    std::complex<float> hp = LinearSVF4PoleHighPass::TransferFunction(
                        m_hp4Pole.m_g.load(),
                        m_hp4Pole.m_k.load(),
                        freq);
                    return lp * hp;
                }

                std::complex<float> lp = LinearStateVariableFilter::LowPassTransferFunction(
                    m_lpSVF.m_g.load(),
                    m_lpSVF.m_k.load(),
                    freq);
                std::complex<float> hp = LinearStateVariableFilter::HighPassTransferFunction(
                    m_hpSVF.m_g.load(),
                    m_hpSVF.m_k.load(),
                    freq);
                return lp * hp;
            }
        };

        struct VoiceSourceUIState
        {
            std::atomic<SquiggleBoyVoice::VoiceConfig::SourceMachine> m_sourceMachine;
            PhysicalModelingSource::UIState m_physicalModelingUIState;

            VoiceSourceUIState()
                : m_sourceMachine(SquiggleBoyVoice::VoiceConfig::SourceMachine::DualWaveShapingVCO)
            {
            }
        };

        struct VoiceConfigUIState
        {
            static constexpr size_t x_uiBufferSize = DirectoryExplorer::UIState::x_uiBufferSize;
            static constexpr size_t x_directoryPathCapacity = 512;

            std::atomic<size_t> m_which;
            char m_sampleDirectorySlot1[x_uiBufferSize][x_numTracks][x_voicesPerTrack][x_directoryPathCapacity];
            char m_sampleDirectorySlot2[x_uiBufferSize][x_numTracks][x_voicesPerTrack][x_directoryPathCapacity];

            VoiceConfigUIState()
                : m_which(0)
                , m_sampleDirectorySlot1{}
                , m_sampleDirectorySlot2{}
            {
            }

            size_t Which() const
            {
                return m_which.load(std::memory_order_acquire);
            }

            const char* GetSampleDirectorySlot1(size_t which, size_t trackIx, size_t voiceInTrack) const
            {
                return m_sampleDirectorySlot1[which][trackIx][voiceInTrack];
            }

            const char* GetSampleDirectorySlot2(size_t which, size_t trackIx, size_t voiceInTrack) const
            {
                return m_sampleDirectorySlot2[which][trackIx][voiceInTrack];
            }

            static void CopyDirectory(char* destination, const char* source)
            {
                if (!destination)
                {
                    return;
                }

                if (!source)
                {
                    destination[0] = '\0';
                    return;
                }

                std::strncpy(destination, source, x_directoryPathCapacity - 1);
                destination[x_directoryPathCapacity - 1] = '\0';
            }

            void SetSampleDirectories(size_t which, size_t trackIx, size_t voiceInTrack, const char* slot1, const char* slot2)
            {
                CopyDirectory(m_sampleDirectorySlot1[which][trackIx][voiceInTrack], slot1);
                CopyDirectory(m_sampleDirectorySlot2[which][trackIx][voiceInTrack], slot2);
            }
        };

        std::atomic<VisualDisplayMode> m_visualDisplayMode;

        EncoderBankUIState m_encoderBankUIState;
        ScopeWriter m_audioScopeWriter;
        ScopeWriter m_controlScopeWriter;
        ScopeWriter m_quadScopeWriter;
        ScopeWriter m_quadControlScopeWriter;
        ScopeWriter m_globalControlScopeWriter;
        ScopeWriter m_sourceMixerScopeWriter;
        ScopeWriter m_monoScopeWriter;

        std::atomic<size_t> m_activeTrack;

        VoiceFilterUIState m_voiceFilterUIState[x_numVoices];
        VoiceSourceUIState m_voiceSourceUIState[x_numVoices];

        std::atomic<float> m_xPos[x_numVoices];
        std::atomic<float> m_yPos[x_numVoices];

        MeterReader m_voiceMeterReader[x_numVoices + SourceMixer::x_numSources];
        QuadMeterReader m_returnMeterReader[QuadMixerInternal::x_numSends];
        QuadMeterReader m_quadMasterMeterReader;
        StereoMeterReader m_stereoMasterMeterReader;

        MultibandSaturator<4, 2>::UIState m_stereoMasteringChainUIState;

        QuadDelay::UIState m_delayUIState;
        QuadReverb::UIState m_reverbUIState;

        SourceMixer::UIState m_sourceMixerUIState;
        DeepVocoder::UIState m_deepVocoderUIState;

        VoiceConfigUIState m_voiceConfigUIState;
        DirectoryExplorer::UIState m_directoryExplorerUIState;

        void ReadMeters()
        {
            for (size_t i = 0; i < x_numVoices + SourceMixer::x_numSources; ++i)
            {
                m_voiceMeterReader[i].Process();
            }

            for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
            {
                m_returnMeterReader[i].Process();
            }

            m_quadMasterMeterReader.Process();
            m_stereoMasterMeterReader.Process();
        }

        void SetMeterReaders(QuadMixerInternal* mixer)
        {
            for (size_t i = 0; i < x_numVoices + SourceMixer::x_numSources; ++i)
            {
                m_voiceMeterReader[i].m_meter = &mixer->m_voiceMeters[i];
            }
            
            for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
            {
                m_returnMeterReader[i].SetMeterReaders(&mixer->m_returnMeters[i]);
            }

            m_quadMasterMeterReader.SetMeterReaders(&mixer->m_masterMeter);
            m_stereoMasterMeterReader.SetMeterReaders(&mixer->m_stereoMeter);

            mixer->m_masterChain.m_stereoMasteringChain.m_saturator.SetupUIState(&m_stereoMasteringChainUIState);
        }


        void PopulateVoiceFilterUIState(
            size_t i,
            LadderFilterLP* lpLadder,
            LinearSVF4PoleHighPass* hp4Pole,
            LinearStateVariableFilter* lpSVF,
            LinearStateVariableFilter* hpSVF,
            SquiggleBoyVoice::VoiceConfig::FilterMachine filterMachine)
        {
            lpLadder->PopulateUIState(&m_voiceFilterUIState[i].m_lpLadder);
            hp4Pole->PopulateUIState(&m_voiceFilterUIState[i].m_hp4Pole);
            lpSVF->PopulateUIState(&m_voiceFilterUIState[i].m_lpSVF);
            hpSVF->PopulateUIState(&m_voiceFilterUIState[i].m_hpSVF);
            m_voiceFilterUIState[i].m_filterMachine.store(filterMachine);
        }

        void PopulateVoiceSourceUIState(
            size_t i,
            SquiggleBoyVoice::VoiceConfig::SourceMachine sourceMachine,
            PhysicalModelingSource* physicalModeling)
        {
            m_voiceSourceUIState[i].m_sourceMachine.store(sourceMachine);
            if (sourceMachine == SquiggleBoyVoice::VoiceConfig::SourceMachine::PhysicalModeling && physicalModeling)
            {
                physicalModeling->PopulateUIState(&m_voiceSourceUIState[i].m_physicalModelingUIState);
            }
        }

        void SetPos(size_t i, float x, float y)
        {
            m_xPos[i].store(x);
            m_yPos[i].store(y);
        }

        void PopulateVoiceConfigUIState(const SquiggleBoyVoice::Input* voiceState)
        {
            size_t which = (m_voiceConfigUIState.m_which.load(std::memory_order_relaxed) + 1) % VoiceConfigUIState::x_uiBufferSize;

            for (size_t trackIx = 0; trackIx < x_numTracks; ++trackIx)
            {
                for (size_t voiceInTrack = 0; voiceInTrack < x_voicesPerTrack; ++voiceInTrack)
                {
                    size_t voiceIx = trackIx * x_voicesPerTrack + voiceInTrack;
                    AudioBufferBank* slot1Bank = voiceState[voiceIx].m_voiceConfig.m_audioBufferBank0;
                    AudioBufferBank* slot2Bank = voiceState[voiceIx].m_voiceConfig.m_audioBufferBank1;

                    const char* slot1Path = (slot1Bank && !slot1Bank->m_directoryName.empty()) ? slot1Bank->m_directoryName.c_str() : "";
                    const char* slot2Path = (slot2Bank && !slot2Bank->m_directoryName.empty()) ? slot2Bank->m_directoryName.c_str() : "";
                    m_voiceConfigUIState.SetSampleDirectories(which, trackIx, voiceInTrack, slot1Path, slot2Path);
                }
            }

            m_voiceConfigUIState.m_which.store(which, std::memory_order_release);
        }

        std::pair<float, float> GetPos(size_t i)
        {
            return std::make_pair(m_xPos[i].load(), m_yPos[i].load());
        }

        UIState()
            : m_audioScopeWriter(x_numVoices, static_cast<size_t>(SmartGridOne::AudioScopes::NumScopes))
            , m_controlScopeWriter(x_numVoices, static_cast<size_t>(SmartGridOne::ControlScopes::NumScopes))
            , m_quadScopeWriter(4, static_cast<size_t>(SmartGridOne::QuadScopes::NumScopes))
            , m_quadControlScopeWriter(4, static_cast<size_t>(SmartGridOne::QuadControlScopes::NumScopes))
            , m_globalControlScopeWriter(1, static_cast<size_t>(SmartGridOne::GlobalControlScopes::NumScopes))
            , m_sourceMixerScopeWriter(SourceMixer::x_numSources, static_cast<size_t>(SmartGridOne::SourceScopes::NumScopes))
            , m_monoScopeWriter(1, static_cast<size_t>(SmartGridOne::MonoScopes::NumScopes))
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_xPos[i] = 0;
                m_yPos[i] = 0;
            }
        }

        void AdvanceScopeIndices()
        {
            m_audioScopeWriter.AdvanceIndex();
            m_quadScopeWriter.AdvanceIndex();
            m_sourceMixerScopeWriter.AdvanceIndex();

            if (SampleTimer::IsControlFrame())
            {
                m_controlScopeWriter.AdvanceIndex();
                m_quadControlScopeWriter.AdvanceIndex();
                m_globalControlScopeWriter.AdvanceIndex();
                m_monoScopeWriter.AdvanceIndex();
            }
        }
    };

    void WriteKMixMidi(KMixMidi* kMixMidi)
    {
        static const Param x_sourceGains[4] =
        {
            Param::Source1Gain,
            Param::Source2Gain,
            Param::Source3Gain,
            Param::Source4Gain
        };

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            float trim = std::max(0.0f, 2 * (m_encoders.GetValue(x_sourceGains[i]) - 0.5f));
            float ccValue = std::max(0.0f, trim);
            kMixMidi->SetTrim(i, ccValue);
        }
    }

    void SetupScopeWriters(UIState* uiState)
    {
        m_delay.m_lfo.ConfigureScopeWriter(&uiState->m_quadControlScopeWriter, static_cast<size_t>(SmartGridOne::QuadControlScopes::DelayLFO));
        m_reverb.m_lfo.ConfigureScopeWriter(&uiState->m_quadControlScopeWriter, static_cast<size_t>(SmartGridOne::QuadControlScopes::ReverbLFO));
        m_quadGangedRandomLFO[0].ConfigureScopeWriter(&uiState->m_quadControlScopeWriter, static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadGangedRandom1));
        m_quadGangedRandomLFO[1].ConfigureScopeWriter(&uiState->m_quadControlScopeWriter, static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadGangedRandom2));
        m_globalGangedRandomLFO[0].ConfigureScopeWriter(&uiState->m_globalControlScopeWriter, static_cast<size_t>(SmartGridOne::GlobalControlScopes::GlobalGangedRandom1));
        m_globalGangedRandomLFO[1].ConfigureScopeWriter(&uiState->m_globalControlScopeWriter, static_cast<size_t>(SmartGridOne::GlobalControlScopes::GlobalGangedRandom2));

        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Delay)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Delay));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Reverb)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Reverb));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Master)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Master));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Dry)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Dry));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Stereo)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Stereo));

        m_sourceMixer.SetupScopeWriters(&uiState->m_sourceMixerScopeWriter);
        SetupGangedRandomScopeWriters(&uiState->m_controlScopeWriter);
        
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_voices[i].SetupAudioScopeWriters(&uiState->m_audioScopeWriter, i);
            m_voices[i].SetupControlScopeWriters(&uiState->m_controlScopeWriter, i);
        }
    }

    void SetupGangedRandomScopeWriters(ScopeWriter* scopeWriter)
    {
        for (size_t j = 0; j < x_numGangedRandomLFOs; ++j)
        {
            m_gangedRandomLFO[j].ConfigureScopeWriter(scopeWriter, static_cast<size_t>(SmartGridOne::ControlScopes::GangedRandom1) + j);
        }
    }

    void SetupUIState(UIState* uiState)
    {
        uiState->SetMeterReaders(&m_mixer);
        m_sourceMixer.SetupUIState(&uiState->m_sourceMixerUIState);
        SetupScopeWriters(uiState);
    }

    struct Input
    {
        float m_baseFreq[x_numVoices];
        
        float m_sheafyModulators[x_numVoices][3];

        float m_faders[x_numFaders];
        bool m_top;
        bool m_topIndependent;

        PhaseUtils::ExpParam m_tempo;

        float GetGainFader(size_t i)
        {
            return m_faders[5 + i / x_numTracks];
        }

        Input()
            : m_tempo(1.0 / (64.0 * 48000.0), 4.0 / 48000.0)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_baseFreq[i] = PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0);                
                m_sheafyModulators[i][0] = 0;
                m_sheafyModulators[i][1] = 0;
                m_sheafyModulators[i][2] = 0;
            }

            for (size_t i = 0; i < x_numFaders; ++i)
            {
                m_faders[i] = 0;
            }

            m_top = false;
            m_topIndependent = false;
        }
    };

    void SelectGesture(int gesture, bool select)
    {
        if (gesture == -1)
        {
            m_selectedGesture.Clear();
        }
        else
        {
            m_selectedGesture.Set(gesture, select);
        }

        m_encoders.SelectGesture(m_selectedGesture);
    }

    SmartGrid::Color GetSelectorColor(Bank bank)
    {
        return m_encoders.GetSelectorColor(bank);
    }

    SmartGrid::Color GetBankColor(Bank bank)
    {
        return m_encoders.GetBankColor(bank);
    }

    void CopyToScene(int scene)
    {
        m_encoders.CopyToScene(scene);
    }

    void SelectEncoderBank(Bank bank)
    {
        if (m_sceneManager && m_sceneManager->m_shift)
        {
            ResetBank(bank);
        }
        else
        {
            m_encoders.SelectBank(bank);
        }
    }

    bool AreVoiceEncodersActive()
    {
        return m_encoders.IsVoiceBankSelected();
    }

    struct SelectorCell : SmartGrid::Cell
    {
        SquiggleBoyWithEncoderBank* m_owner;
        Bank m_bank;
        
        SelectorCell(SquiggleBoyWithEncoderBank* owner, Bank bank)
            : m_owner(owner)
            , m_bank(bank)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->GetSelectorColor(m_bank);
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->SelectEncoderBank(m_bank);
        }
    };

    void Config(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_useIndirectPhasor = true;
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_useIndirectPhasor = true;

            m_state[i].SetTheoryOfTimeInputs(m_theoryOfTime);
        }

    }

    SquiggleBoyWithEncoderBank()
        : m_sceneManager(nullptr)
    {
    }

    void Init(SmartGrid::SceneManager* sceneManager)
    {
        m_sceneManager = sceneManager;
        m_encoders.Init(sceneManager, TheNonagonInternal::x_numTrios, TheNonagonInternal::x_voicesPerTrio);
    }

    void SetTrack(size_t track)
    {
        m_encoders.SetTrack(track);
        UpdateEncodersForMachine();
    }

    void UpdateEncodersForMachine()
    {
        int track = m_encoders.GetCurrentTrack();
        size_t voiceIx = static_cast<size_t>(track) * x_voicesPerTrack;
        m_encoders.UpdateEncodersForMachine(
            m_state[voiceIx].m_voiceConfig.m_sourceMachine,
            m_state[voiceIx].m_voiceConfig.m_filterMachine);
    }

    void ResetBank(Bank bank)
    {
        m_encoders.ResetBank(bank, false, false);
    }

    void SetVoiceModulators(Input& input)
    {
        auto& modulatorValues = m_encoders.GetModulatorValues(BankMode::Voice);
        for (size_t i = 0; i < x_numVoices; ++i)
        {            
            for (size_t j = 0; j < x_numGangedRandomLFOs; ++j)
            {
                modulatorValues.m_value[j][i] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[j].m_lfos[i / x_numTracks].m_pos[i % x_numTracks]));
            }

            modulatorValues.m_value[4][i] = m_voices[i].m_amp.m_modulationAHD.m_rawOutput;
            modulatorValues.m_value[5][i] = m_voices[i].m_amp.m_ahd.m_rawOutput;
            modulatorValues.m_amplitude[4][i] = m_voices[i].m_amp.m_modulationAHD.m_amplitude;
            modulatorValues.m_amplitude[5][i] = m_voices[i].m_amp.m_ahd.m_amplitude;

            modulatorValues.m_value[6][i] = m_voices[i].m_squiggleLFO[0].m_output;
            modulatorValues.m_value[7][i] = m_voices[i].m_squiggleLFO[1].m_output;
            modulatorValues.m_amplitude[6][i] = m_voices[i].m_squiggleLFO[0].m_polyXFader.m_amplitude;
            modulatorValues.m_amplitude[7][i] = m_voices[i].m_squiggleLFO[1].m_polyXFader.m_amplitude;

            modulatorValues.m_value[8][i] = input.m_sheafyModulators[i][0];
            modulatorValues.m_value[9][i] = input.m_sheafyModulators[i][1];
            modulatorValues.m_value[10][i] = input.m_sheafyModulators[i][2];

            modulatorValues.m_value[11][i] = static_cast<float>(i % x_numTracks) / (x_numTracks - 1);

            modulatorValues.m_value[14][i] = m_rGen.UniGen();
        }

        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numGestureParams; ++i)
        {
            modulatorValues.m_gestureWeights[i] = input.m_faders[i];
        }
    }

    void SetQuadModulators(Input& input)
    {
        auto& modulatorValues = m_encoders.GetModulatorValues(BankMode::Quad);
    
        for (size_t i = 0; i < 4; ++i)
        {
            modulatorValues.m_value[2][i] = std::min(1.0f, std::max(0.0f, m_quadGangedRandomLFO[0].m_lfos[0].m_pos[i]));
            modulatorValues.m_value[3][i] = std::min(1.0f, std::max(0.0f, m_quadGangedRandomLFO[1].m_lfos[0].m_pos[i]));

            modulatorValues.m_value[4][i] = m_delay.m_lfo.m_output[i] / 2.0 + 0.5;
            modulatorValues.m_value[5][i] = m_reverb.m_lfo.m_output[i] / 2.0 + 0.5;

            modulatorValues.m_value[11][i] = static_cast<float>(i % 4) / 3;

            modulatorValues.m_value[14][i] = m_rGen.UniGen();
        }

        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numGestureParams; ++i)
        {
            modulatorValues.m_gestureWeights[i] = input.m_faders[i];
        }    
    }

    void SetGlobalModulators(Input& input)
    {
        auto& modulatorValues = m_encoders.GetModulatorValues(BankMode::Global);

        modulatorValues.m_value[2][0] = std::min(1.0f, std::max(0.0f, m_globalGangedRandomLFO[0].m_lfos[0].m_pos[0]));
        modulatorValues.m_value[3][0] = std::min(1.0f, std::max(0.0f, m_globalGangedRandomLFO[1].m_lfos[0].m_pos[0]));

        modulatorValues.m_value[14][0] = m_rGen.UniGen();

        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numGestureParams; ++i)
        {
            modulatorValues.m_gestureWeights[i] = input.m_faders[i];
        }
    }

    void SetEncoderParameters(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            float freqSlewAmount = m_voices[i].m_baseFreqSlewAmount.Update(1.0 - m_encoders.GetValue(Param::Portamento, i));
            m_voices[i].m_baseFreqSlew.SetAlphaFromNatFreq(freqSlewAmount);
            float baseFreq = m_voices[i].m_baseFreqSlew.Process(input.m_baseFreq[i]);

            // Set source machine for the source dispatch
            // (m_sourceMixer is already set in the constructor)
            //
            m_state[i].m_sourceInput.m_sourceMachine = m_state[i].m_voiceConfig.m_sourceMachine;

            // Dispatch encoder params to the appropriate source machine
            //
            float wtBlend0 = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[2].m_lfos[i / x_voicesPerTrack].m_pos[i % x_voicesPerTrack]));
            float wtBlend1 = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[3].m_lfos[i / x_voicesPerTrack].m_pos[i % x_voicesPerTrack]));
            m_voices[i].m_source.SetEncoderParams(
                m_encoders, m_state[i].m_sourceInput, i, baseFreq, wtBlend0, wtBlend1);

            m_state[i].m_subInput.m_baseFreq = baseFreq;

            m_state[i].m_filterInput.m_sampleRateReducerFreq.Update(1 - m_encoders.GetValueNoSlew(Param::SampleRateReduction, i));
            m_state[i].m_ampInput.m_subGain.Update(std::min<float>(1.0, m_encoders.GetValue(Param::SubOscillator, i) * 2));
            m_state[i].m_ampInput.m_subTanhGain.Update(std::max<float>(0.0, m_encoders.GetValue(Param::SubOscillator, i) * 2 - 1.0));
            
            m_state[i].m_filterInput.m_vcoBaseFreq = m_voices[i].m_baseFreqSlew.m_output;            
            
            m_state[i].m_filterInput.m_lpCutoffFactor.Update(m_encoders.GetValueNoSlew(Param::LPCutoff, i));
            m_state[i].m_filterInput.m_hpCutoffFactor.Update(m_encoders.GetValueNoSlew(Param::HPCutoff, i));
            m_state[i].m_filterInput.m_lpResonance.Update(m_encoders.GetValueNoSlew(Param::LPResonance, i));
            m_state[i].m_filterInput.m_hpResonance.Update(m_encoders.GetValueNoSlew(Param::HPResonance, i));
            m_state[i].m_filterInput.m_saturationGain.Update(m_encoders.GetValueNoSlew(Param::FilterDrive, i));

            m_state[i].m_ampInput.SetModulationAHD(
                m_encoders.GetValueNoSlew(Param::ModAHDAttack, i), 
                m_encoders.GetValueNoSlew(Param::ModAHDHold, i), 
                m_encoders.GetValueNoSlew(Param::ModAHDDecay, i), 
                m_encoders.GetValueNoSlew(Param::ModAmplitude, i));

            m_state[i].m_ampInput.m_vcoTargetFreq = input.m_baseFreq[i];
            m_state[i].m_ampInput.SetAHD(
                m_encoders.GetValue(Param::AmpAHDAttack, i), 
                m_encoders.GetValue(Param::AmpAHDHold, i), 
                m_encoders.GetValue(Param::AmpAHDDecay, i), 
                m_encoders.GetValue(Param::AmpAmplitude, i));

            m_state[i].m_ampInput.m_gain.Update(m_state[i].m_ampInput.m_gainSlew.Process(input.GetGainFader(i)));

            m_state[i].m_panInput.m_radius = m_encoders.GetValue(Param::PanRadius, i);
            float staticOffset = static_cast<float>(i % x_numTracks) / x_numTracks + static_cast<float>(i / x_numTracks) / (x_numTracks * x_numTracks);
            m_state[i].m_panInput.m_phaseShift = staticOffset + m_encoders.GetValue(Param::PanPhaseShift, i);
            m_state[i].m_panInput.m_multX = m_encoders.GetValue(Param::PanMultX, i) * 4 + 1;
            m_state[i].m_panInput.m_multY = m_encoders.GetValue(Param::PanMultY, i) * 4 + 1;
            m_state[i].m_panInput.m_centerX = m_encoders.GetValue(Param::PanCenterX, i) * 2 - 1;
            m_state[i].m_panInput.m_centerY = m_encoders.GetValue(Param::PanCenterY, i) * 2 - 1;

            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_attackFrac = m_encoders.GetValue(Param::LFO1Skew, i);
            m_state[i].m_squiggleLFOInput[0].m_mult.Update(m_encoders.GetValue(Param::LFO1Mult, i));
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_shape = m_encoders.GetValue(Param::LFO1Shape, i);
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_amplitude = m_encoders.GetValue(Param::LFO1Amplitude, i);
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_center = 1 - m_encoders.GetValue(Param::LFO1Center, i);
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_slope = m_encoders.GetValue(Param::LFO1Slope, i);
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_phaseShift = m_encoders.GetValue(Param::LFO1PhaseShift, i) * (static_cast<float>(i % x_numTracks) / x_numTracks);
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_useIndirectPhasor = true;
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_shFade = m_encoders.GetValue(Param::LFO1SampleAndHold, i);

            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_attackFrac = m_encoders.GetValue(Param::LFO2Skew, i);
            m_state[i].m_squiggleLFOInput[1].m_mult.Update(m_encoders.GetValue(Param::LFO2Mult, i));
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_shape = m_encoders.GetValue(Param::LFO2Shape, i);
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_amplitude = m_encoders.GetValue(Param::LFO2Amplitude, i);
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_center = 1 - m_encoders.GetValue(Param::LFO2Center, i);
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_slope = m_encoders.GetValue(Param::LFO2Slope, i);
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_phaseShift = m_encoders.GetValue(Param::LFO2PhaseShift, i) * (static_cast<float>(i % x_numTracks) / x_numTracks);
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_useIndirectPhasor = true;
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_shFade = m_encoders.GetValue(Param::LFO2SampleAndHold, i);

            m_mixerState.m_gain[i].Update(m_encoders.GetValue(Param::AmpGain, i));
            m_mixerState.m_sendGain[i][0].Update(m_encoders.GetValue(Param::DelaySend, i));
            m_mixerState.m_sendGain[i][1].Update(m_encoders.GetValue(Param::ReverbSend, i));

            m_state[i].SetGates();
        }

        QuadDelayInputSetter::Input delayInputSetterInput;
        for (int i = 0; i < 4; ++i)
        {
            delayInputSetterInput.m_loopSelectorKnob[i] = m_encoders.GetValue(Param::DelayTime, i);
            delayInputSetterInput.m_delayTimeFactorSwitchVal[i] = m_encoders.GetSwitchVal(Param::DelayTimeFactor, i);
            delayInputSetterInput.m_readHeadSpeedSwitchVal[i] = m_encoders.GetSwitchVal(Param::DelayReadSpeed, i);
            delayInputSetterInput.m_feedbackKnob[i] = m_encoders.GetValue(Param::DelayFeedback, i);

            delayInputSetterInput.m_dampingBaseKnob[i] = m_encoders.GetValue(Param::DelayDampBase, i);
            delayInputSetterInput.m_dampingWidthKnob[i] = m_encoders.GetValue(Param::DelayDampWidth, i);
            delayInputSetterInput.m_resynthSlewUpKnob[i] = m_encoders.GetValue(Param::ResynthSlewUp, i);

            delayInputSetterInput.m_widenKnob[i] = m_encoders.GetValue(Param::DelayWiden, i);
            delayInputSetterInput.m_rotateSwitchVal[i] = m_encoders.GetSwitchVal(Param::DelayRotate, i);
            delayInputSetterInput.m_resynthUnisonKnob[i] = m_encoders.GetValue(Param::ResynthUnison, i);

            delayInputSetterInput.m_resynthShiftFadeKnob[i] = m_encoders.GetValue(Param::ResynthShiftFade, i);
            delayInputSetterInput.m_resynthShiftSwitchVal[i] = m_encoders.GetSwitchVal(Param::ResynthShiftInterval, i);

            delayInputSetterInput.m_modFreqKnob[i] = m_encoders.GetValue(Param::DelayModSpeed, i);
            delayInputSetterInput.m_lfoPhaseKnob[i] = m_encoders.GetValue(Param::DelayModPhaseShift, i);
        }

        delayInputSetterInput.m_theoryOfTime = m_theoryOfTime;
        m_delayInputSetter.Process(delayInputSetterInput, m_delayState, &m_delay);
        m_delayToReverbSend.Update(m_encoders.GetValue(Param::DelayReverbSend));
        m_mixerState.m_returnGain[0].Update(m_encoders.GetValue(Param::DelayReturn));

        QuadReverbInputSetter::Input reverbInputSetterInput;
        for (int i = 0; i < 4; ++i)
        {
            reverbInputSetterInput.m_reverbTimeKnob[i] = m_encoders.GetValue(Param::ReverbTime, i);
            reverbInputSetterInput.m_feedbackKnob[i] = m_encoders.GetValue(Param::ReverbFeedback, i);

            reverbInputSetterInput.m_dampingBaseKnob[i] = m_encoders.GetValue(Param::ReverbDampBase, i);
            reverbInputSetterInput.m_dampingWidthKnob[i] = m_encoders.GetValue(Param::ReverbDampWidth, i);

            reverbInputSetterInput.m_modDepthKnob[i] = m_encoders.GetValue(Param::ReverbModDepth, i);
            reverbInputSetterInput.m_modFreqKnob[i] = m_encoders.GetValue(Param::ReverbModSpeed, i);
            reverbInputSetterInput.m_lfoPhaseKnob[i] = m_encoders.GetValue(Param::ReverbModPhaseShift, i);
        }
        
        m_reverbInputSetter.Process(reverbInputSetterInput, m_reverbState);
        m_reverbToDelaySend.Update(m_encoders.GetValue(Param::ReverbDelaySend));
        m_mixerState.m_returnGain[1].Update(m_encoders.GetValue(Param::ReverbReturn) / 2);

        input.m_tempo.Update(m_encoders.GetValue(Param::Tempo));

        m_mixerState.m_masterChainInput.SetGain(0, m_encoders.GetValue(Param::LowEq));
        m_mixerState.m_masterChainInput.SetGain(1, m_encoders.GetValue(Param::LowMidEq));
        m_mixerState.m_masterChainInput.SetGain(2, m_encoders.GetValue(Param::HighMidEq));
        m_mixerState.m_masterChainInput.SetGain(3, m_encoders.GetValue(Param::HighEq));
        m_mixerState.m_masterChainInput.SetBassFreq(m_encoders.GetValue(Param::LowBandCutoff));
        m_mixerState.m_masterChainInput.SetCrossoverFreq(0, m_encoders.GetValue(Param::MidBandCutoff));
        m_mixerState.m_masterChainInput.SetCrossoverFreq(1, m_encoders.GetValue(Param::HighBandCutoff));
        m_mixerState.m_masterChainInput.SetMasterGain(m_encoders.GetValue(Param::MasterGain));
        m_masterVolume.Update(m_encoders.GetValue(Param::MasterVolume));

        m_sourceMixerState.m_sources[0].m_hpCutoff.Update(m_encoders.GetValueNoSlew(Param::Source1HP));
        m_sourceMixerState.m_sources[0].m_lpFactor.Update(m_encoders.GetValueNoSlew(Param::Source1LP));
        m_sourceMixerState.m_sources[0].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValueNoSlew(Param::Source1Gain)));
        m_sourceMixerState.m_sources[1].m_hpCutoff.Update(m_encoders.GetValueNoSlew(Param::Source2HP));
        m_sourceMixerState.m_sources[1].m_lpFactor.Update(m_encoders.GetValueNoSlew(Param::Source2LP));
        m_sourceMixerState.m_sources[1].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValueNoSlew(Param::Source2Gain)));
        m_sourceMixerState.m_sources[2].m_hpCutoff.Update(m_encoders.GetValueNoSlew(Param::Source3HP));
        m_sourceMixerState.m_sources[2].m_lpFactor.Update(m_encoders.GetValueNoSlew(Param::Source3LP));
        m_sourceMixerState.m_sources[2].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValueNoSlew(Param::Source3Gain)));
        m_sourceMixerState.m_sources[3].m_hpCutoff.Update(m_encoders.GetValueNoSlew(Param::Source4HP));
        m_sourceMixerState.m_sources[3].m_lpFactor.Update(m_encoders.GetValueNoSlew(Param::Source4LP));
        m_sourceMixerState.m_sources[3].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValueNoSlew(Param::Source4Gain)));

        m_deepVocoderState.SetSlew(m_encoders.GetValue(Param::DeepVocoderSlewUp), m_encoders.GetValue(Param::DeepVocoderSlewDown));
        
        static const Param x_dvGainThresh[3] = {Param::DVGainThresh1, Param::DVGainThresh2, Param::DVGainThresh3};
        static const Param x_dvRatio[3] = {Param::DVRatio1, Param::DVRatio2, Param::DVRatio3};
        static const Param x_dvSlopeDown[3] = {Param::DVSlopeDown1, Param::DVSlopeDown2, Param::DVSlopeDown3};
        static const Param x_dvSlopeUp[3] = {Param::DVSlopeUp1, Param::DVSlopeUp2, Param::DVSlopeUp3};

        for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
        {
            for (size_t voiceInTrio = 0; voiceInTrio < TheNonagonInternal::x_voicesPerTrio; ++voiceInTrio)
            {
                size_t i = trio * 3 + voiceInTrio;

                m_deepVocoderState.m_voiceInput[i].m_gainThreshold.Update(m_encoders.GetValue(x_dvGainThresh[trio]));
                m_deepVocoderState.m_voiceInput[i].m_pitchRatioPre.Update(m_encoders.GetValue(x_dvRatio[trio]));
                m_deepVocoderState.m_voiceInput[i].m_slopeDown.Update(m_encoders.GetValue(x_dvSlopeDown[trio]));
                m_deepVocoderState.m_voiceInput[i].m_slopeUp.Update(1.0 - m_encoders.GetValue(x_dvSlopeUp[trio]));
            }
        }
    }   

    JSON ToJSON()
    {
        return m_encoders.ToJSON();
    }

    void FromJSON(JSON rootJ)
    {
        m_encoders.FromJSON(rootJ);
    }

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        m_encoders.RevertToDefault(allScenes, allTracks);
    }

    void ClearGesture(int gesture)
    {
        m_encoders.ClearGesture(gesture);
    }

    BitSet16 GetGesturesAffectingBankForTrack(Bank bank, size_t track)
    {
        BankMode mode = m_encoders.GetModeForBank(bank);
        size_t effectiveTrack = (mode == BankMode::Voice) ? track : 0;
        return m_encoders.GetGesturesAffectingBankForTrack(bank, effectiveTrack);
    }

    bool IsGestureAffectingBank(int gesture, Bank bank, size_t track)
    {
        BankMode mode = m_encoders.GetModeForBank(bank);
        size_t effectiveTrack = (mode == BankMode::Voice) ? track : 0;
        return m_encoders.IsGestureAffectingBank(gesture, bank, effectiveTrack);
    }

    bool IsGestureAffectingAnyBank(int gesture)
    {
        return m_encoders.IsGestureAffecting(gesture);
    }

    SmartGrid::Color GetGestureColor(int gesture)
    {
        SmartGrid::Color result = m_encoders.GetGestureColor(gesture);
        if (result == SmartGrid::Color::Off)
        {
            return SmartGrid::Color::Grey.Dim();
        }

        return result;
    }

    void Apply(SmartGrid::MessageIn msg)
    {
        m_encoders.Apply(msg);
    }

    void PopulateUIState(UIState* uiState)
    {
        m_encoders.PopulateUIState(&uiState->m_encoderBankUIState);
        uiState->m_activeTrack.store(m_encoders.GetCurrentTrack());

        uiState->m_audioScopeWriter.Publish();
        uiState->m_controlScopeWriter.Publish();
        uiState->m_quadScopeWriter.Publish();
        uiState->m_quadControlScopeWriter.Publish();
        uiState->m_globalControlScopeWriter.Publish();
        uiState->m_sourceMixerScopeWriter.Publish();
        uiState->m_monoScopeWriter.Publish();
        
        uiState->ReadMeters();

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            uiState->SetPos(i, m_voices[i].m_pan.m_outputX, m_voices[i].m_pan.m_outputY);
            uiState->PopulateVoiceFilterUIState(
                i,
                &m_voices[i].m_filter.m_lpLadder,
                &m_voices[i].m_filter.m_hp4Pole,
                &m_voices[i].m_filter.m_lpSVF,
                &m_voices[i].m_filter.m_hpSVF,
                m_state[i].m_voiceConfig.m_filterMachine);
            uiState->PopulateVoiceSourceUIState(
                i,
                m_state[i].m_voiceConfig.m_sourceMachine,
                &m_voices[i].m_source.m_physicalModeling);
        }

        m_mixerState.m_masterChainInput.PopulateUIState(&uiState->m_stereoMasteringChainUIState);
        m_delay.PopulateUIState(&uiState->m_delayUIState, m_delayState);
        m_reverb.PopulateUIState(&uiState->m_reverbUIState);

        m_sourceMixer.PopulateUIState(&uiState->m_sourceMixerUIState);
        m_deepVocoder.PopulateUIState(&uiState->m_deepVocoderUIState);
        uiState->PopulateVoiceConfigUIState(m_state);

        Bank selectedBank = m_encoders.m_selectedBank;
        switch (selectedBank)
        {
            case Bank::Source:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Voice);
                break;
            }
            case Bank::FilterAndAmp:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Filter);
                break;
            }
            case Bank::PanningAndSequencing:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::PanAndMelody);
                break;
            }
            case Bank::VoiceLFOs:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Control);
                break;
            }
            case Bank::Delay:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Delay);
                break;
            }
            case Bank::Reverb:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Reverb);
                break;
            }
            case Bank::TheoryOfTime:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::QuadMaster);
                break;
            }
            case Bank::Mastering:
            case Bank::Inputs:
            case Bank::DeepVocoder:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::StereoMaster);
                break;
            }
            default:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Voice);
                break;
            }
        }
    }

    void ProcessSample(Input& input, float deltaT, const AudioInputBuffer& audioInputBuffer)
    {
        m_topIndependent = input.m_topIndependent;

        SetVoiceModulators(input);
        SetGlobalModulators(input);
        SetQuadModulators(input);

        m_encoders.Process();

        SetEncoderParameters(input);

        SquiggleBoy::ProcessSample(audioInputBuffer);
    }
};