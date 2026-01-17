#pragma once

#include "VectorPhaseShaper.hpp"
#include "EncoderBankBank.hpp"
#include "ButterworthFilter.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "RandomLFO.hpp"
#include "LadderFilter.hpp"
#include "ADSP.hpp"
#include "BitCrush.hpp"
#include "Lissajous.hpp"
#include "QuadMixer.hpp"
#include "QuadDelay.hpp"
#include "QuadReverb.hpp"
#include "PolyXFader.hpp"
#include "GangedRandomLFO.hpp"
#include "ScopeWriter.hpp"
#include "Blink.hpp"
#include "RandomWaveTable.hpp"
#include "MultibandSaturator.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "VCO.hpp"
#include "SourceMixer.hpp"
#include "DeepVocoder.hpp"
#include "KMixMidi.hpp"
#include "Math.hpp"
#include "SampleTimer.hpp"
#include "StateSaver.hpp"

struct TheoryOfTime;

struct SquiggleBoyVoice
{
    struct VoiceConfig
    {
        enum class Machine : int
        {
            VCO = 0,
            Thru = 1,
        };

        VoiceConfig()
            : m_machine(Machine::VCO)
            , m_sourceIndex(0)
        {
        }

        Machine m_machine;
        int m_sourceIndex;        
    };

    struct VCOSection
    {
        static constexpr size_t x_oversample = 4;
        VectorPhaseShaperInternal m_vco[2];

        BitRateReducer m_bitRateReducer;
        TanhSaturator<true> m_saturator;


        ButterworthFilter m_antiAliasFilter;

        bool m_top;
        float m_output;

        ScopeWriterHolder m_scopeWriter[2];

        OPLowPassFilter m_wtBlendFilter[2];

        PhaseUtils::ExpParam m_morphHarmonics[2];

        VCO m_sub;

        struct Input
        {
            float m_v[2];
            float m_d[2];
            float m_baseFreq;

            float m_morphHarmonics[2];
            float m_wtBlend[2];

            PhaseUtils::ExpParam m_offsetFreqFactor;
            PhaseUtils::ExpParam m_detune;

            PhaseUtils::ZeroedExpParam m_crossModIndex[2];

            float m_fade;

            float m_bitCrushAmount;

            float m_saturationGain;

            Input()
                : m_v{0.5, 0.5}
                , m_d{0.5, 0.5}
                , m_baseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
                , m_morphHarmonics{0.0, 0.0}
                , m_wtBlend{0, 0}    
                , m_offsetFreqFactor(4)         
                , m_detune(1.03)
                , m_fade(0)
                , m_bitCrushAmount(0)
                , m_saturationGain(0.25)
            {
                m_crossModIndex[0] = PhaseUtils::ZeroedExpParam(8.0);
                m_crossModIndex[1] = PhaseUtils::ZeroedExpParam(8.0);
            }
        };

        Input m_state;

        VCOSection()
            : m_output(0)
        {
            m_antiAliasFilter.SetCyclesPerSample(0.40 / x_oversample);
            m_wtBlendFilter[0].SetAlphaFromNatFreq(1000.0 / 48000.0);
            m_wtBlendFilter[1].SetAlphaFromNatFreq(1000.0 / 48000.0);
        }

        float Process(const Input& input)
        {
            m_output = 0;

            bool top[2] = {false, false};
            float wtBlend[2] = {m_wtBlendFilter[0].Process(input.m_wtBlend[0]), m_wtBlendFilter[1].Process(input.m_wtBlend[1])}; 

            for (size_t i = 0; i < x_oversample; ++i)
            {
                float interp = (i + 1) / static_cast<float>(x_oversample);

                VectorPhaseShaperInternal::Input vcoInput[2];

                vcoInput[0].m_useVoct = false;
                vcoInput[1].m_useVoct = false;
                
                for (int j = 0; j < 2; ++j)
                {
                    vcoInput[j].m_v = m_state.m_v[j] + interp * (input.m_v[j] - m_state.m_v[j]);
                    vcoInput[j].m_d = m_state.m_d[j] + interp * (input.m_d[j] - m_state.m_d[j]);
                    vcoInput[j].m_wtBlend = m_state.m_wtBlend[j] + interp * (wtBlend[j] - m_state.m_wtBlend[j]);
                }

                vcoInput[0].m_phaseMod = (m_state.m_crossModIndex[0].m_expParam + interp * (input.m_crossModIndex[0].m_expParam - m_state.m_crossModIndex[0].m_expParam)) * m_vco[1].m_out;
                vcoInput[0].m_freq = (m_state.m_baseFreq + interp * (input.m_baseFreq - m_state.m_baseFreq)) / x_oversample;
                vcoInput[1].m_freq = vcoInput[0].m_freq * (m_state.m_offsetFreqFactor.m_expParam + interp * (input.m_offsetFreqFactor.m_expParam - m_state.m_offsetFreqFactor.m_expParam));

                vcoInput[0].m_freq *= m_state.m_detune.m_expParam;
                vcoInput[1].m_freq /= m_state.m_detune.m_expParam;

                for (int j = 0; j < 2; ++j)
                {
                    vcoInput[j].m_morphHarmonics = m_morphHarmonics[j].Update(vcoInput[j].m_freq, vcoInput[j].m_maxFreq, input.m_morphHarmonics[j]);
                }

                m_vco[0].Process(vcoInput[0], 0 /*unused*/);
                vcoInput[1].m_phaseMod = m_vco[0].m_out * (m_state.m_crossModIndex[1].m_expParam + interp * (input.m_crossModIndex[1].m_expParam - m_state.m_crossModIndex[1].m_expParam));
                m_vco[1].Process(vcoInput[1], 0 /*unused*/);

                top[0] = top[0] || m_vco[0].m_top;
                top[1] = top[1] || m_vco[1].m_top;

                float fade = m_state.m_fade + interp * (input.m_fade - m_state.m_fade);
                float mixed = m_vco[0].m_out * Math::Cos2pi(fade / 4) + m_vco[1].m_out * Math::Cos2pi(fade / 4 + 0.75);

                float bitCrushAmount = m_state.m_bitCrushAmount + interp * (input.m_bitCrushAmount - m_state.m_bitCrushAmount);
                m_bitRateReducer.SetAmount(bitCrushAmount);
                float crushed = m_bitRateReducer.Process(mixed);

                float saturationGain = m_state.m_saturationGain + interp * (input.m_saturationGain - m_state.m_saturationGain);
                m_saturator.SetInputGain(saturationGain);
                float saturated = m_saturator.Process(crushed);

                m_output += m_antiAliasFilter.Process(saturated);
            }

            m_sub.Process(input.m_baseFreq / 2);

            m_scopeWriter[0].Write(m_vco[0].m_out);
            m_scopeWriter[1].Write(m_vco[1].m_out);
            if (top[0])
            {
                m_scopeWriter[0].RecordStart();
            }

            if (top[1])
            {
                m_scopeWriter[1].RecordStart();
            }

            m_output /= x_oversample;
            m_state = input;

            m_top = top[0];

            return m_output;
        }
    };

    struct FilterSection
    {
        LadderFilterLP m_lpFilter;
        LadderFilterHP m_hpFilter;
        ADSP m_adsp;

        SampleRateReducer m_sampleRateReducer;

        float m_output;

        ScopeWriterHolder m_scopeWriter;

        FilterSection()
            : m_output(0)
        {
        }
        
        struct Input
        {
            float m_vcoBaseFreq;
            float m_ladderBaseFreq;
            PhaseUtils::ExpParam m_lpCutoffFactor;
            PhaseUtils::ExpParam m_hpCutoffFactor;
            PhaseUtils::ZeroedExpParam m_lpResonance;
            PhaseUtils::ZeroedExpParam m_hpResonance;
            float m_envDepth;

            ADSP::InputSetter m_adspInputSetter;
            ADSP::Input m_adspInput;

            VoiceConfig* m_voiceConfig;

            PhaseUtils::ExpParam m_sampleRateReducerFreq;

            void SetADSP(float attack, float decay, float sustain, float phasorMult)
            {
                m_adspInputSetter.Set(attack, decay, sustain, phasorMult, m_adspInput);
            }

            Input()
                : m_vcoBaseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
                , m_lpCutoffFactor(0.25f, 1024.0f)
                , m_hpCutoffFactor(0.25f, 256.0f)
                , m_envDepth(0)
                , m_sampleRateReducerFreq(1.0f, 2048.0f)
            {
                m_lpResonance.SetBaseByCenter(0.125);
                m_hpResonance.SetBaseByCenter(0.125);
            }
        };
        
        float Process(Input& input, float vcoOutput, bool top)
        {
            m_adsp.Process(input.m_adspInput);

            float baseFreq = input.m_vcoBaseFreq;
            if (input.m_voiceConfig->m_machine == VoiceConfig::Machine::Thru)
            {
                baseFreq = 80.0 / SampleTimer::x_sampleRate;
            }

            m_hpFilter.SetCutoff(baseFreq * input.m_hpCutoffFactor.m_expParam);
            m_hpFilter.SetResonance(input.m_hpResonance.m_expParam);
            m_lpFilter.SetCutoff(baseFreq * input.m_hpCutoffFactor.m_expParam * input.m_lpCutoffFactor.m_expParam);
            m_lpFilter.SetResonance(input.m_lpResonance.m_expParam);

            m_output = vcoOutput;
            m_output = m_lpFilter.Process(vcoOutput);
            m_output = m_hpFilter.Process(m_output);

            m_sampleRateReducer.SetFreq(input.m_vcoBaseFreq * input.m_sampleRateReducerFreq.m_expParam);
            m_output = m_sampleRateReducer.Process(m_output);

            m_scopeWriter.Write(m_output);
            if (top)
            {
                m_scopeWriter.RecordStart();
            }

            return m_output;
        }
    };

    struct AmpSection
    {
        ADSP m_adsp;

        float m_output;
        float m_subOut;

        OPLowPassFilter m_freqDependentGainSlew;
        float m_freqDependentGainTarget;

        ATanSaturator<true> m_outputSaturator;

        OPLowPassFilter m_subFilter;

        ScopeWriterHolder m_scopeWriter;
        bool m_subRunning;

        struct Input
        {
            float m_vcoTargetFreq;
            ADSP::InputSetter m_adspInputSetter;
            ADSP::Input m_adspInput;

            PhaseUtils::ZeroedExpParam m_gain;
            PhaseUtils::ZeroedExpParam m_subGain;
            PhaseUtils::ExpParam m_subTanhGain;

            ScopeWriterHolder m_scopeWriter;
            TanhSaturator<true> m_subTanhSaturator;

            VoiceConfig* m_voiceConfig;

            bool m_subTrig;

            void SetADSP(float attack, float decay, float sustain, float phasorMult)
            {
                m_adspInputSetter.Set(attack, decay, sustain, phasorMult, m_adspInput);
            }

            Input()
            {
                m_subTrig = false;
                m_subTanhGain = PhaseUtils::ExpParam(0.125, 2.0);
            }
        };

        AmpSection()
            : m_output(0)
        {
            m_freqDependentGainSlew.SetAlphaFromNatFreq(250.0f / 48000.0f);
            m_outputSaturator.SetInputGain(0.5f);
            m_subFilter.SetAlphaFromNatFreq(170.0f / 48000.0f);
            m_subRunning = false;
        }

        float Process(Input& input, float filterOutput, float sub)
        {
            if (input.m_adspInput.m_trig)
            {
                if (input.m_vcoTargetFreq * 48000 < 100 || input.m_voiceConfig->m_machine == VoiceConfig::Machine::Thru)
                {
                    m_freqDependentGainTarget = 1.0;
                }
                else
                {
                    m_freqDependentGainTarget = 1.0 / std::sqrtf(input.m_vcoTargetFreq * 48000 / 100);
                }

                if (m_adsp.m_state != ADSP::State::Idle)
                {
                    m_scopeWriter.RecordEnd();
                }

                m_subRunning = input.m_subTrig && input.m_voiceConfig->m_machine == VoiceConfig::Machine::VCO;
            }

            input.m_subTanhSaturator.SetInputGain(input.m_subTanhGain.m_expParam);
            sub = input.m_subTanhSaturator.Process(sub);

            float freqDependentGain = m_freqDependentGainSlew.Process(m_freqDependentGainTarget);
            float adspEnv = std::powf(m_adsp.Process(input.m_adspInput), 2.0f);
            float subGain = m_subFilter.Process(m_subRunning ? input.m_subGain.m_expParam * adspEnv : 0.0f);            
            float mainOut = freqDependentGain * adspEnv * filterOutput;
            float subOut = freqDependentGain * subGain * sub;
            
            m_scopeWriter.Write(mainOut + m_subOut);
            if (input.m_adspInput.m_trig)
            {
                m_scopeWriter.RecordStart();
            }

            if (m_adsp.m_changed && m_adsp.m_state == ADSP::State::Idle)
            {
                m_scopeWriter.RecordEnd();
            }

            m_subOut = subOut * input.m_gain.m_expParam;
            m_output = mainOut * input.m_gain.m_expParam;
            m_output = m_outputSaturator.Process(m_output);

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
        float m_shValue;
        OPLowPassFilter m_shSlew;
        float m_output;

        ScopeWriterHolder m_scopeWriter;

        struct Input
        {
            PolyXFaderInternal::Input m_polyXFaderInput;
            PhaseUtils::ExpParam m_mult;
            bool m_trig;
            float m_shFade;

            Input()
                : m_mult(1, 16)
            {
                m_polyXFaderInput.m_size = x_numPhasors;
            }
        };

        float Process(Input& input)
        {
            input.m_polyXFaderInput.m_mult = input.m_mult.m_expParam;
            m_polyXFader.Process(input.m_polyXFaderInput);
            float lfoOut = m_polyXFader.m_output;
            if (input.m_trig)
            {
                m_shValue = lfoOut;
            }
            
            m_output = m_shSlew.Process(m_shValue) * input.m_shFade + lfoOut * (1 - input.m_shFade);

            m_scopeWriter.Write(m_output);

            if (m_polyXFader.m_top)
            {
                m_scopeWriter.RecordStart();
            }

            return m_output;
        }

        SquiggleLFO()
            : m_output(0)
        {
            m_shSlew.SetAlphaFromNatFreq(500.0f / 48000.0f);
        }
    };

    void SetupAudioScopeWriters(ScopeWriter* scopeWriter, size_t voiceIx)
    {
        m_vco.m_scopeWriter[0] = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::VCO1));
        m_vco.m_scopeWriter[1] = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::VCO2));
        m_filter.m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::PostFilter));
        m_amp.m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp));
    }

    void SetupControlScopeWriters(ScopeWriter* scopeWriter, size_t voiceIx)
    {
        m_squiggleLFO[0].m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::ControlScopes::LFO1));
        m_squiggleLFO[1].m_scopeWriter = ScopeWriterHolder(scopeWriter, voiceIx, static_cast<size_t>(SmartGridOne::ControlScopes::LFO2));
    }

    OPLowPassFilter m_baseFreqSlew;
    OPLowPassFilter m_ladderFreqSlew;
    PhaseUtils::ExpParam m_baseFreqSlewAmount;
    VCOSection m_vco;
    FilterSection m_filter;
    AmpSection m_amp;
    PanSection m_pan;
    SquiggleLFO m_squiggleLFO[2];

    float m_output;

    struct Input
    {
        VoiceConfig m_voiceConfig;
        SourceMixer* m_sourceMixer;
        ADSP::ADSPControl m_adspControl;

        VCOSection::Input m_vcoInput;
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
            m_filterInput.m_adspInput.Set(m_adspControl);
            m_ampInput.m_adspInput.Set(m_adspControl);

            m_squiggleLFOInput[0].m_trig = m_adspControl.m_trig;
            m_squiggleLFOInput[1].m_trig = m_adspControl.m_trig;
        }
    };

    SquiggleBoyVoice()
        : m_baseFreqSlewAmount(0.125 / 48000.0, 500.0 / 48000.0)
        , m_output(0)
    {
    }

    float Processs(Input& input)
    {
        if (input.m_voiceConfig.m_machine == VoiceConfig::Machine::VCO)
        {
            m_output = m_vco.Process(input.m_vcoInput);
        }
        else
        {
            m_output = input.m_sourceMixer->m_sources[input.m_voiceConfig.m_sourceIndex].m_output;
        }

        m_output = m_filter.Process(input.m_filterInput, m_output, m_vco.m_top);
        m_output = m_amp.Process(input.m_ampInput, m_output, m_vco.m_sub.m_output);
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

    struct MixerInput : QuadMixerInternal::Input
    {
        ScopeWriterHolder m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::NumScopes)];

        MixerInput()
        {
        }
    };

    SquiggleBoyWaveTableGenerator m_waveTableGenerator[2];

    SquiggleBoyVoice m_voices[x_numVoices];
    ManyGangedRandomLFO m_gangedRandomLFO[4];
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
    ManyGangedRandomLFO::Input m_gangedRandomLFOInput[4];
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

    RGen m_rGen;

    bool m_firstFrame;

    StateSaver* m_stateSaver;

    SquiggleBoy()
        : m_stateSaver(nullptr)
    {
        m_mixerState.m_numInputs = x_numVoices + SourceMixer::x_numSources;
        m_mixerState.m_numMonoInputs = x_numVoices;
        m_delayToReverbSend.m_expParam = 0.0;
        m_reverbToDelaySend.m_expParam = 0.0;
        m_theoryOfTime = nullptr;

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_sourceMixer = &m_sourceMixer;
        }

        for (size_t i = 0; i < 4; ++i)
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
                    m_waveTableGenerator[i].SetLeft(&m_voices[j * x_voicesPerTrack + k].m_vco.m_vco[i]);
                }

                m_waveTableGenerator[i].Clear();
                m_waveTableGenerator[i].GenerateCompletely();
                for (size_t k = 0; k < x_voicesPerTrack; ++k)
                {
                    m_waveTableGenerator[i].SetRight(&m_voices[j * x_voicesPerTrack + k].m_vco.m_vco[i]);
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
                            m_waveTableGenerator[i].SetRight(&m_voices[j * x_voicesPerTrack + k].m_vco.m_vco[i]);
                        }

                        m_waveTableGenerator[i].m_rightVisible[j] = false;
                        m_waveTableGenerator[i].Clear();
                    }

                    if (needLeft)
                    {
                        INFO("New left wavetable for VCO %zu track %zu", i, j);
                        for (size_t k = 0; k < x_voicesPerTrack; ++k)
                        {
                            m_waveTableGenerator[i].SetLeft(&m_voices[j * x_voicesPerTrack + k].m_vco.m_vco[i]);
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

        m_gangedRandomLFO[0].Process(1.0 / 48000.0, m_gangedRandomLFOInput[0]);
        m_gangedRandomLFO[1].Process(1.0 / 48000.0, m_gangedRandomLFOInput[1]);
        m_gangedRandomLFO[2].Process(1.0 / 48000.0, m_gangedRandomLFOInput[2]);
        m_gangedRandomLFO[3].Process(1.0 / 48000.0, m_gangedRandomLFOInput[3]);

        m_globalGangedRandomLFO[0].Process(1.0 / 48000.0, m_globalGangedRandomLFOInput[0]);
        m_globalGangedRandomLFO[1].Process(1.0 / 48000.0, m_globalGangedRandomLFOInput[1]);
        m_quadGangedRandomLFO[0].Process(1.0 / 48000.0, m_quadGangedRandomLFOInput[0]);
        m_quadGangedRandomLFO[1].Process(1.0 / 48000.0, m_quadGangedRandomLFOInput[1]);

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_panInput.m_input = m_panPhase.m_phase;

            m_mixerState.m_input[i] = m_voices[i].Processs(m_state[i]);
            m_mixerState.m_monoIn[i] = m_voices[i].m_amp.m_subOut;
            m_mixerState.m_x[i] = m_voices[i].m_pan.m_outputX;
            m_mixerState.m_y[i] = m_voices[i].m_pan.m_outputY;
        }

        m_sourceMixer.Process(m_sourceMixerState);
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
    static constexpr size_t x_numVoiceBanks = 4;
    EncoderBankBankInternal<x_numVoiceBanks> m_voiceEncoderBank;

    static constexpr size_t x_numQuadBanks = 2;
    EncoderBankBankInternal<x_numQuadBanks> m_quadEncoderBank;

    static constexpr size_t x_numGlobalBanks = 4;
    EncoderBankBankInternal<x_numGlobalBanks> m_globalEncoderBank;

    static constexpr size_t x_totalNumBanks = x_numVoiceBanks + x_numQuadBanks + x_numGlobalBanks;

    static constexpr size_t x_numFaders = 16;

    size_t m_selectedAbsoluteEncoderBank;
    size_t m_selectedGridId;

    bool m_shift;    

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

        struct FilterParams
        {
            std::atomic<float> m_hpAlpha;
            std::atomic<float> m_lpAlpha;
            std::atomic<float> m_lpResonance;
            std::atomic<float> m_hpResonance;
        };

        std::atomic<VisualDisplayMode> m_visualDisplayMode;

        EncoderBankUIState m_encoderBankUIState;
        ScopeWriter m_audioScopeWriter;
        ScopeWriter m_controlScopeWriter;
        ScopeWriter m_quadScopeWriter;
        ScopeWriter m_sourceMixerScopeWriter;
        ScopeWriter m_monoScopeWriter;

        std::atomic<size_t> m_activeTrack;

        FilterParams m_filterParams[x_numVoices];

        std::atomic<float> m_xPos[x_numVoices];
        std::atomic<float> m_yPos[x_numVoices];

        MeterReader m_voiceMeterReader[x_numVoices];
        QuadMeterReader m_returnMeterReader[QuadMixerInternal::x_numSends];
        QuadMeterReader m_quadMasterMeterReader;
        StereoMeterReader m_stereoMasterMeterReader;

        MultibandSaturator<4, 2>::UIState m_stereoMasteringChainUIState;

        QuadDelay::UIState m_delayUIState;
        QuadReverb::UIState m_reverbUIState;

        SourceMixer::UIState m_sourceMixerUIState;
        DeepVocoder::UIState m_deepVocoderUIState;

        void ReadMeters()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_voiceMeterReader[i].Process();
            }

            for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
            {
                m_returnMeterReader[i].Process();
            }

            m_quadMasterMeterReader.Process();
            m_stereoMasterMeterReader.Process();

            m_sourceMixerUIState.ProcessMeters();
        }

        void SetMeterReaders(QuadMixerInternal* mixer)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
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


        void SetFilterParams(size_t i, float hpAlpha, float lpAlpha, float hpResonance, float lpResonance)
        {
            m_filterParams[i].m_hpAlpha.store(hpAlpha);
            m_filterParams[i].m_lpAlpha.store(lpAlpha);
            m_filterParams[i].m_lpResonance.store(lpResonance);
            m_filterParams[i].m_hpResonance.store(hpResonance);
        }

        void SetPos(size_t i, float x, float y)
        {
            m_xPos[i].store(x);
            m_yPos[i].store(y);
        }

        std::pair<float, float> GetPos(size_t i)
        {
            return std::make_pair(m_xPos[i].load(), m_yPos[i].load());
        }

        UIState()
            : m_audioScopeWriter(x_numVoices, static_cast<size_t>(SmartGridOne::AudioScopes::NumScopes))
            , m_controlScopeWriter(x_numVoices, static_cast<size_t>(SmartGridOne::ControlScopes::NumScopes))
            , m_quadScopeWriter(4, static_cast<size_t>(SmartGridOne::QuadScopes::NumScopes))
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
            m_controlScopeWriter.AdvanceIndex();
            m_quadScopeWriter.AdvanceIndex();
            m_sourceMixerScopeWriter.AdvanceIndex();
            m_monoScopeWriter.AdvanceIndex();
        }
    };

    void WriteKMixMidi(KMixMidi* kMixMidi)
    {        
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            float trim = std::max(0.0f, 2 * (m_globalEncoderBank.GetValue(2, i, 3, 0) - 0.5f));
            float ccValue = std::max(0.0f, trim);
            kMixMidi->SetTrim(i, ccValue);
        }
    }

    void SetupScopeWriters(UIState* uiState)
    {
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Delay)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Delay));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Reverb)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Reverb));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Master)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Master));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Dry)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Dry));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Stereo)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Stereo));

        m_sourceMixer.SetupScopeWriters(&uiState->m_sourceMixerScopeWriter);
        
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_voices[i].SetupAudioScopeWriters(&uiState->m_audioScopeWriter, i);
            m_voices[i].SetupControlScopeWriters(&uiState->m_controlScopeWriter, i);
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
        EncoderBankBankInternal<x_numVoiceBanks>::Input m_voiceEncoderBankInput;
        EncoderBankBankInternal<x_numGlobalBanks>::Input m_globalEncoderBankInput;
        EncoderBankBankInternal<x_numQuadBanks>::Input m_quadEncoderBankInput;

        float m_baseFreq[x_numVoices];
        float m_ladderBaseFreq[x_numVoices];

        float m_sheafyModulators[x_numVoices][3];

        float m_faders[x_numFaders];

        double m_totalPhasor[SquiggleBoyVoice::SquiggleLFO::x_numPhasors];
        bool m_totalTop[SquiggleBoyVoice::SquiggleLFO::x_numPhasors];
        bool m_top;

        PhaseUtils::ExpParam m_tempo;

        bool m_shift;
        BitSet16 m_selectedGesture;

        float GetGainFader(size_t i)
        {
            return m_faders[5 + i / x_numTracks];
        }

        Input()
            : m_tempo(1.0 / (64.0 * 48000.0), 4.0 / 48000.0)
            , m_shift(false)
        {
            m_selectedGesture.Clear();
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_baseFreq[i] = PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0);
                m_ladderBaseFreq[i] = PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0);
                m_sheafyModulators[i][0] = 0;
                m_sheafyModulators[i][1] = 0;
                m_sheafyModulators[i][2] = 0;
            }

            for (size_t i = 0; i < x_numFaders; ++i)
            {
                m_faders[i] = 0;
            }

            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_numVoices = x_numVoices / x_numTracks;
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_numTracks = x_numTracks;

            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_numVoices = 1;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_numTracks = 1;

            m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_numVoices = 4;
            m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_numTracks = 1;
        }

        void SelectTrack(int track)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_track = track;
        }

        size_t GetCurrentTrack()
        {
            return m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_track;
        }

        void SetLeftScene(int scene)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene1 = scene;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene1 = scene;
            m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene1 = scene;
        }

        void SetRightScene(int scene)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene2 = scene;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene2 = scene;
            m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene2 = scene;
        }

        void SetBlendFactor(float blendFactor)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_blendFactor = blendFactor;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_blendFactor = blendFactor;
            m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_blendFactor = blendFactor;
        }

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

            m_voiceEncoderBankInput.SelectGesture(m_selectedGesture);
            m_globalEncoderBankInput.SelectGesture(m_selectedGesture);
            m_quadEncoderBankInput.SelectGesture(m_selectedGesture);
        }    

        void SetBlink(bool blink)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_blink = blink;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_blink = blink;
            m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_blink = blink;
        }
    };

    void SelectGridId()
    {
        if (m_selectedAbsoluteEncoderBank < x_numVoiceBanks)
        {
            m_voiceEncoderBank.SelectGrid(m_selectedAbsoluteEncoderBank);
            m_quadEncoderBank.SelectGrid(-1);
            m_globalEncoderBank.SelectGrid(-1);
            m_selectedGridId = m_voiceEncoderBank.m_selectedGridId;
        }
        else if (m_selectedAbsoluteEncoderBank < x_numVoiceBanks + x_numQuadBanks)
        {
            m_quadEncoderBank.SelectGrid(m_selectedAbsoluteEncoderBank - x_numVoiceBanks);
            m_voiceEncoderBank.SelectGrid(-1);
            m_globalEncoderBank.SelectGrid(-1);
            m_selectedGridId = m_quadEncoderBank.m_selectedGridId;
        }
        else
        {
            m_globalEncoderBank.SelectGrid(m_selectedAbsoluteEncoderBank - x_numVoiceBanks - x_numQuadBanks);
            m_voiceEncoderBank.SelectGrid(-1);
            m_quadEncoderBank.SelectGrid(-1);
            m_selectedGridId = m_globalEncoderBank.m_selectedGridId;
        }
    }

    SmartGrid::Color GetSelectorColor(size_t ordinal)
    {
        if (ordinal < x_numVoiceBanks)
        {
            return m_voiceEncoderBank.GetSelectorColor(ordinal);
        }
        else if (ordinal < x_numVoiceBanks + x_numQuadBanks)
        {
            return m_quadEncoderBank.GetSelectorColor(ordinal - x_numVoiceBanks);
        }
        else
        {
            return m_globalEncoderBank.GetSelectorColor(ordinal - x_numVoiceBanks - x_numQuadBanks);
        }
    }

    SmartGrid::Color GetSelectorColorNoDim(size_t ordinal)
    {
        if (ordinal < x_numVoiceBanks)
        {
            return m_voiceEncoderBank.m_color[ordinal];
        }
        else if (ordinal < x_numVoiceBanks + x_numQuadBanks)
        {
            return m_quadEncoderBank.m_color[ordinal - x_numVoiceBanks];
        }
        else
        {
            return m_globalEncoderBank.m_color[ordinal - x_numVoiceBanks - x_numQuadBanks];
        }
    }

    void CopyToScene(int scene)
    {
        m_voiceEncoderBank.CopyToScene(scene);
        m_globalEncoderBank.CopyToScene(scene);
        m_quadEncoderBank.CopyToScene(scene);
    }

    void SelectEncoderBank(size_t encoderBank)
    {
        if (m_shift)
        {
            ResetGrid(encoderBank);
        }
        else
        {
            m_selectedAbsoluteEncoderBank = encoderBank;
            SelectGridId();
        }
    }

    bool AreVoiceEncodersActive()
    {
        return m_voiceEncoderBank.m_selectedBank >= 0;
    }

    struct SelectorCell : SmartGrid::Cell
    {
        SquiggleBoyWithEncoderBank* m_owner;
        size_t m_ordinal;
        
        SelectorCell(SquiggleBoyWithEncoderBank* owner, size_t ordinal)
            : m_owner(owner)
            , m_ordinal(ordinal)
        {
        }

        virtual SmartGrid::Color GetColor() override
        {
            return m_owner->GetSelectorColor(m_ordinal);
        }

        virtual void OnPress(uint8_t velocity) override
        {
            m_owner->SelectEncoderBank(m_ordinal);
        }
    };

    void Config(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_values = input.m_totalPhasor;
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_values = input.m_totalPhasor;
            m_state[i].m_squiggleLFOInput[0].m_polyXFaderInput.m_top = input.m_totalTop;
            m_state[i].m_squiggleLFOInput[1].m_polyXFaderInput.m_top = input.m_totalTop;
        }

        m_voiceEncoderBank.SetColor(0, SmartGrid::Color::Red, input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.SetColor(1, SmartGrid::Color::Green, input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.SetColor(2, SmartGrid::Color::Orange, input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.SetColor(3, SmartGrid::Color::Blue, input.m_voiceEncoderBankInput);
        m_quadEncoderBank.SetColor(0, SmartGrid::Color::Pink, input.m_quadEncoderBankInput);
        m_quadEncoderBank.SetColor(1, SmartGrid::Color::Fuscia, input.m_quadEncoderBankInput);
        m_globalEncoderBank.SetColor(0, SmartGrid::Color::Yellow, input.m_globalEncoderBankInput);
        m_globalEncoderBank.SetColor(1, SmartGrid::Color::SeaGreen, input.m_globalEncoderBankInput);
        m_globalEncoderBank.SetColor(2, SmartGrid::Color::White, input.m_globalEncoderBankInput);
        m_globalEncoderBank.SetColor(3, SmartGrid::Color::Ocean, input.m_globalEncoderBankInput);

        m_voiceEncoderBank.Config(0, 0, 0, 0, "VCO 1 Cos Blend", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 0, 1, 0, "VCO 2 Cos Blend", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 1, 0, 1.0 / 8.0, "VCO 1 V", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 1, 1, 1.0 / 8.0, "VCO 2 V", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 2, 0, 0.5, "VCO 1 D", input.m_voiceEncoderBankInput);  
        m_voiceEncoderBank.Config(0, 2, 1, 0.5, "VCO 2 D", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 3, 0, 0, "VCO 2->1 PM", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 3, 1, 0, "VCO 1->2 PM", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 0, 2, 0, "VCO 1 Fade", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 1, 2, 0, "Saturation", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 2, 2, 0, "Bit Reduction", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 3, 2, 0, "Sample Rate Reduction", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 0, 3, 0, "VCO 2 Pitch Offset", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 1, 3, 0, "Pitch Drift", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 2, 3, 0, "Sub Oscillator", input.m_voiceEncoderBankInput);

        m_voiceEncoderBank.Config(1, 0, 1, 0, "HP Cutoff", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 1, 0, "HP Resonance", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 1, 1, "LP Cutoff", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 1, 0, "LP Resonance", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 0, 0, 0, "Filter ADSP Attack", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 0, 0.2, "Filter ADSP Decay", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 0, 0, "Filter ADSP Sustain", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 0, 0.5, "Filter ADSP Phasor Mult", input.m_voiceEncoderBankInput);

        m_voiceEncoderBank.Config(1, 0, 2, 0, "Amp ADSP Attack", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 2, 0.3, "Amp ADSP Decay", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 2, 0.5, "Amp ADSP Sustain", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 2, 0.5, "Amp ADSP Phasor Mult", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 0, 3, 1, "Amp Gain", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 3, 0, "Delay Send", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 3, 0, "Reverb Send", input.m_voiceEncoderBankInput);

        m_voiceEncoderBank.Config(2, 0, 0, 1, "Pan Radius", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(2, 1, 0, 0, "Pan Phase Shift", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(2, 2, 0, 0, "Pan Mult X", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(2, 3, 0, 0, "Pan Mult Y", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(2, 0, 1, 0.5, "Pan Center X", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(2, 1, 1, 0.5, "Pan Center Y", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(2, 3, 1, 0, "Portamento", input.m_voiceEncoderBankInput);

        for (size_t i = 0; i < 2; ++i)
        {
            m_voiceEncoderBank.Config(3, 0, 2 * i, 0.5, "LFO " + std::to_string(i + 1) + " Skew", input.m_voiceEncoderBankInput);
            m_voiceEncoderBank.Config(3, 1, 2 * i, 0, "LFO " + std::to_string(i + 1) + " Mult", input.m_voiceEncoderBankInput);
            m_voiceEncoderBank.Config(3, 2, 2 * i, 0.5, "LFO " + std::to_string(i + 1) + " Shape", input.m_voiceEncoderBankInput);
            m_voiceEncoderBank.Config(3, 0, 2 * i + 1, i == 0 ? 0.75 : 0.25, "LFO " + std::to_string(i + 1) + " Center", input.m_voiceEncoderBankInput);
            m_voiceEncoderBank.Config(3, 1, 2 * i + 1, 0, "LFO " + std::to_string(i + 1) + " Slope", input.m_voiceEncoderBankInput);
            m_voiceEncoderBank.Config(3, 2, 2 * i + 1, 1.0, "LFO " + std::to_string(i + 1) + " Phase Shift", input.m_voiceEncoderBankInput);
            m_voiceEncoderBank.Config(3, 3, 2 * i + 1, 0, "LFO " + std::to_string(i + 1) + " Sample And Hold", input.m_voiceEncoderBankInput);
        }

        m_quadEncoderBank.Config(0, 0, 0, 0.75, "Delay Time", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 1, 0, 0.75, "Delay Time Factor", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 3, 0, 0.75, "Delay Feedback", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 0, 1, 0.4, "Delay Damp Base", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 1, 1, 0.3, "Delay Damp Width", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 2, 1, 0.5, "Resynth Slew Up", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 3, 1, 0, "Reverb Send", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 0, 2, 0, "Delay Widen", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 1, 2, 0.25, "Delay Rotate", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 2, 2, 0.1, "Resynth Unison", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 1, 3, 0.0, "Resynth Shift Fade", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 2, 3, 0.0, "Resynth Shift Interval", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(0, 3, 3, 1.0, "Delay Return", input.m_quadEncoderBankInput);

        m_quadEncoderBank.Config(1, 0, 0, 0.5, "Reverb Time", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 3, 0, 0.75, "Reverb Feedback", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 0, 1, 0.3, "Reverb Damp Base", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 1, 1, 0.5, "Reverb Damp Width", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 3, 1, 0, "Delay Send", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 1, 2, 0.1, "Delay Mod Speed", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 2, 2, 1.0, "Delay Mod Phase Shift", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 0, 3, 0, "Reverb Mod Depth", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 1, 3, 0.1, "Reverb Mod Speed", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 2, 3, 1.0, "Reverb Mod Phase Shift", input.m_quadEncoderBankInput);
        m_quadEncoderBank.Config(1, 3, 3, 1.0, "Reverb Return", input.m_quadEncoderBankInput);

        m_globalEncoderBank.Config(0, 0, 0, 0.5, "Tempo", input.m_globalEncoderBankInput);

        m_globalEncoderBank.Config(0, 0, 2, 0.5, "Tempo LFO Skew", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 1, 2, 0, "Tempo LFO Mult", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 2, 2, 0.5, "Tempo LFO Shape", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 0, 3, 0.75, "Tempo LFO Center", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 1, 3, 0, "Tempo LFO Slope", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 3, 3, 0, "Tempo LFO Index", input.m_globalEncoderBankInput);

        m_globalEncoderBank.Config(1, 0, 2, 4.0 / 5.0, "Low Eq", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 1, 2, 4.0 / 5.0, "Low Mid Eq", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 2, 2, 4.0 / 5.0, "High Mid Eq", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 3, 2, 4.0 / 5.0, "High Eq", input.m_globalEncoderBankInput);
        
        m_globalEncoderBank.Config(1, 0, 3, 0.5, "Low Band Cutoff", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 1, 3, 0.5, "Mid Band Cutoff", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 2, 3, 0.5, "High Band Cutoff", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 3, 3, 4.0 / 5.0, "Master Gain", input.m_globalEncoderBankInput);

        m_globalEncoderBank.Config(2, 0, 0, 0, "Source 1 HP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 0, 1, 1, "Source 1 LP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 0, 3, 0, "Source 1 Gain", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 1, 0, 0, "Source 2 HP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 1, 1, 1, "Source 2 LP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 1, 3, 0, "Source 2 Gain", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 2, 0, 0, "Source 3 HP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 2, 1, 1, "Source 3 LP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 2, 3, 0, "Source 3 Gain", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 3, 0, 0, "Source 4 HP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 3, 1, 1, "Source 4 LP", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(2, 3, 3, 0, "Source 4 Gain", input.m_globalEncoderBankInput);

        // UNDONE(DEEP_VOCODER)
        //
        m_globalEncoderBank.Config(3, 0, 0, 0.5, "Deep Vocoder Slew Up", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(3, 1, 0, 0.5, "Deep Vocoder Slew Down", input.m_globalEncoderBankInput);
        
        for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
        {
            size_t row = 1 + trio;
            m_globalEncoderBank.Config(3, 0, row, 0.5, "Deep Vocoder Gain Threshold Trio " + std::to_string(trio + 1), input.m_globalEncoderBankInput);
            m_globalEncoderBank.Config(3, 1, row, 0.5, "Deep Vocoder Ratio Trio " + std::to_string(trio + 1), input.m_globalEncoderBankInput);
            m_globalEncoderBank.Config(3, 2, row, 0.5, "Deep Vocoder Slope Down Trio " + std::to_string(trio + 1), input.m_globalEncoderBankInput);
            m_globalEncoderBank.Config(3, 3, row, 0.5, "Deep Vocoder Slope Up Trio " + std::to_string(trio + 1), input.m_globalEncoderBankInput);
        }
    }

    SquiggleBoyWithEncoderBank()
    {
        SelectEncoderBank(0);
    }

    void SetVoiceModulators(Input& input)
    {
        SmartGrid::BankedEncoderCell::ModulatorValues& modulatorValues = input.m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_modulatorValues;
        for (size_t i = 0; i < x_numVoices; ++i)
        {            
            modulatorValues.m_value[2][i] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[0].m_lfos[i / x_numTracks].m_pos[i % x_numTracks]));
            modulatorValues.m_value[3][i] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[1].m_lfos[i / x_numTracks].m_pos[i % x_numTracks]));

            modulatorValues.m_value[4][i] = m_voices[i].m_filter.m_adsp.m_output;
            modulatorValues.m_value[5][i] = m_voices[i].m_amp.m_adsp.m_output;

            modulatorValues.m_value[6][i] = m_voices[i].m_squiggleLFO[0].m_output;
            modulatorValues.m_value[7][i] = m_voices[i].m_squiggleLFO[1].m_output;

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
        SmartGrid::BankedEncoderCell::ModulatorValues& modulatorValues = input.m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_modulatorValues;
    
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
        SmartGrid::BankedEncoderCell::ModulatorValues& modulatorValues = input.m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_modulatorValues;

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
            float freqSlewAmount = m_voices[i].m_baseFreqSlewAmount.Update(1.0 - m_voiceEncoderBank.GetValue(2, 3, 1, i));
            m_voices[i].m_baseFreqSlew.SetAlphaFromNatFreq(freqSlewAmount);
            m_voices[i].m_ladderFreqSlew.SetAlphaFromNatFreq(freqSlewAmount);
            m_state[i].m_vcoInput.m_baseFreq = m_voices[i].m_baseFreqSlew.Process(input.m_baseFreq[i]);
            m_state[i].m_vcoInput.m_morphHarmonics[0] = m_voiceEncoderBank.GetValue(0, 0, 0, i);
            m_state[i].m_vcoInput.m_morphHarmonics[1] = m_voiceEncoderBank.GetValue(0, 0, 1, i);
            m_state[i].m_vcoInput.m_wtBlend[0] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[2].m_lfos[i / x_voicesPerTrack].m_pos[i % x_voicesPerTrack]));
            m_state[i].m_vcoInput.m_wtBlend[1] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[3].m_lfos[i / x_voicesPerTrack].m_pos[i % x_voicesPerTrack]));

            m_state[i].m_vcoInput.m_v[0] = m_voiceEncoderBank.GetValue(0, 1, 0, i);
            m_state[i].m_vcoInput.m_v[1] = m_voiceEncoderBank.GetValue(0, 1, 1, i);
            m_state[i].m_vcoInput.m_d[0] = m_voiceEncoderBank.GetValue(0, 2, 0, i);
            m_state[i].m_vcoInput.m_d[1] = m_voiceEncoderBank.GetValue(0, 2, 1, i);
            m_state[i].m_vcoInput.m_crossModIndex[0].Update(m_voiceEncoderBank.GetValue(0, 3, 0, i));
            m_state[i].m_vcoInput.m_crossModIndex[1].Update(m_voiceEncoderBank.GetValue(0, 3, 1, i));

            m_state[i].m_vcoInput.m_fade = m_voiceEncoderBank.GetValue(0, 0, 2, i);
            m_state[i].m_vcoInput.m_saturationGain = m_voiceEncoderBank.GetValue(0, 1, 2, i) * 4 + 0.25;
            m_state[i].m_vcoInput.m_bitCrushAmount = m_voiceEncoderBank.GetValue(0, 2, 2, i);
            m_state[i].m_filterInput.m_sampleRateReducerFreq.Update(1 - m_voiceEncoderBank.GetValue(0, 3, 2, i));
            m_state[i].m_vcoInput.m_offsetFreqFactor.Update(m_voiceEncoderBank.GetValue(0, 0, 3, i));
            m_state[i].m_vcoInput.m_detune.Update(m_voiceEncoderBank.GetValue(0, 1, 3, i));
            m_state[i].m_ampInput.m_subGain.Update(std::min<float>(1.0, m_voiceEncoderBank.GetValue(0, 2, 3, i) * 2));
            m_state[i].m_ampInput.m_subTanhGain.Update(std::max<float>(0.0, m_voiceEncoderBank.GetValue(0, 2, 3, i) * 2 - 1.0));
            
            m_state[i].m_filterInput.m_vcoBaseFreq = m_voices[i].m_baseFreqSlew.m_output;            
            
            m_state[i].m_filterInput.m_lpCutoffFactor.Update(m_voiceEncoderBank.GetValue(1, 2, 1, i));
            m_state[i].m_filterInput.m_hpCutoffFactor.Update(m_voiceEncoderBank.GetValue(1, 0, 1, i));
            m_state[i].m_filterInput.m_lpResonance.Update(m_voiceEncoderBank.GetValue(1, 3, 1, i));
            m_state[i].m_filterInput.m_hpResonance.Update(m_voiceEncoderBank.GetValue(1, 1, 1, i));

            if (m_state[i].m_adspControl.m_trig)
            {
                input.m_ladderBaseFreq[i] = std::powf(input.m_baseFreq[i], 0.707f);     
            }
            
            m_state[i].m_filterInput.m_ladderBaseFreq = m_voices[i].m_ladderFreqSlew.Process(input.m_ladderBaseFreq[i]);
            

            m_state[i].m_filterInput.SetADSP(
                m_voiceEncoderBank.GetValue(1, 0, 0, i), 
                m_voiceEncoderBank.GetValue(1, 1, 0, i), 
                m_voiceEncoderBank.GetValue(1, 2, 0, i), 
                m_voiceEncoderBank.GetValue(1, 3, 0, i));

            m_state[i].m_ampInput.m_vcoTargetFreq = input.m_baseFreq[i];
            m_state[i].m_ampInput.SetADSP(
                m_voiceEncoderBank.GetValue(1, 0, 2, i), 
                m_voiceEncoderBank.GetValue(1, 1, 2, i), 
                m_voiceEncoderBank.GetValue(1, 2, 2, i), 
                m_voiceEncoderBank.GetValue(1, 3, 2, i));

            m_state[i].m_ampInput.m_gain.Update(input.GetGainFader(i));

            m_state[i].m_panInput.m_radius = m_voiceEncoderBank.GetValue(2, 0, 0, i);
            float staticOffset = static_cast<float>(i % x_numTracks) / x_numTracks + static_cast<float>(i / x_numTracks) / (x_numTracks * x_numTracks);
            m_state[i].m_panInput.m_phaseShift = staticOffset + m_voiceEncoderBank.GetValue(2, 1, 0, i);
            m_state[i].m_panInput.m_multX = m_voiceEncoderBank.GetValue(2, 2, 0, i) * 4 + 1;
            m_state[i].m_panInput.m_multY = m_voiceEncoderBank.GetValue(2, 3, 0, i) * 4 + 1;
            m_state[i].m_panInput.m_centerX = m_voiceEncoderBank.GetValue(2, 0, 1, i) * 2 - 1;
            m_state[i].m_panInput.m_centerY = m_voiceEncoderBank.GetValue(2, 1, 1, i) * 2 - 1;

            for (size_t j = 0; j < 2; ++j)
            {
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_attackFrac = m_voiceEncoderBank.GetValue(3, 0, 2 * j, i);
                m_state[i].m_squiggleLFOInput[j].m_mult.Update(m_voiceEncoderBank.GetValue(3, 1, 2 * j, i));
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_shape = m_voiceEncoderBank.GetValue(3, 2, 2 * j, i);
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_center = 1 - m_voiceEncoderBank.GetValue(3, 0, 2 * j + 1, i);
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_slope = m_voiceEncoderBank.GetValue(3, 1, 2 * j + 1, i);
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_phaseShift = m_voiceEncoderBank.GetValue(3, 2, 2 * j + 1, i) * (static_cast<float>(i % x_numTracks) / x_numTracks);
                m_state[i].m_squiggleLFOInput[j].m_shFade = m_voiceEncoderBank.GetValue(3, 3, 2 * j + 1, i);
            }

            m_mixerState.m_gain[i].Update(m_voiceEncoderBank.GetValue(1, 0, 3, i));
            m_mixerState.m_sendGain[i][0].Update(m_voiceEncoderBank.GetValue(1, 1, 3, i));
            m_mixerState.m_sendGain[i][1].Update(m_voiceEncoderBank.GetValue(1, 2, 3, i));

            m_state[i].SetGates();
        }

        QuadDelayInputSetter::Input delayInputSetterInput;
        for (int i = 0; i < 4; ++i)
        {
            delayInputSetterInput.m_loopSelectorKnob[i] = m_quadEncoderBank.GetValue(0, 0, 0, i);
            delayInputSetterInput.m_delayTimeFactorKnob[i] = m_quadEncoderBank.GetValue(0, 1, 0, i);
            delayInputSetterInput.m_feedbackKnob[i] = m_quadEncoderBank.GetValue(0, 3, 0, i);

            delayInputSetterInput.m_dampingBaseKnob[i] = m_quadEncoderBank.GetValue(0, 0, 1, i);
            delayInputSetterInput.m_dampingWidthKnob[i] = m_quadEncoderBank.GetValue(0, 1, 1, i);
            delayInputSetterInput.m_resynthSlewUpKnob[i] = m_quadEncoderBank.GetValue(0, 2, 1, i);

            delayInputSetterInput.m_widenKnob[i] = m_quadEncoderBank.GetValue(0, 0, 2, i);
            delayInputSetterInput.m_rotateKnob[i] = m_quadEncoderBank.GetValue(0, 1, 2, i);
            delayInputSetterInput.m_resynthUnisonKnob[i] = m_quadEncoderBank.GetValue(0, 2, 2, i);

            delayInputSetterInput.m_resynthShiftFadeKnob[i] = m_quadEncoderBank.GetValue(0, 1, 3, i);
            delayInputSetterInput.m_resynthShiftKnob[i] = m_quadEncoderBank.GetValue(0, 2, 3, i);

            // For whatever reason, the delay's LFO is on the reverb page... And really not owned by the delay.
            //
            delayInputSetterInput.m_modFreqKnob[i] = m_quadEncoderBank.GetValue(1, 1, 2, i);
            delayInputSetterInput.m_lfoPhaseKnob[i] = m_quadEncoderBank.GetValue(1, 2, 2, i);
        }

        delayInputSetterInput.m_theoryOfTime = m_theoryOfTime;
        m_delayInputSetter.Process(delayInputSetterInput, m_delayState);
        m_delayToReverbSend.Update(m_quadEncoderBank.GetValue(0, 3, 1, 0));
        m_mixerState.m_returnGain[0].Update(m_quadEncoderBank.GetValue(0, 3, 3, 0));

        QuadReverbInputSetter::Input reverbInputSetterInput;
        for (int i = 0; i < 4; ++i)
        {
            reverbInputSetterInput.m_reverbTimeKnob[i] = m_quadEncoderBank.GetValue(1, 0, 0, i);
            reverbInputSetterInput.m_feedbackKnob[i] = m_quadEncoderBank.GetValue(1, 3, 0, i);

            reverbInputSetterInput.m_dampingBaseKnob[i] = m_quadEncoderBank.GetValue(1, 0, 1, i);
            reverbInputSetterInput.m_dampingWidthKnob[i] = m_quadEncoderBank.GetValue(1, 1, 1, i);

            reverbInputSetterInput.m_modDepthKnob[i] = m_quadEncoderBank.GetValue(1, 0, 3, i);
            reverbInputSetterInput.m_modFreqKnob[i] = m_quadEncoderBank.GetValue(1, 1, 3, i);
            reverbInputSetterInput.m_lfoPhaseKnob[i] = m_quadEncoderBank.GetValue(1, 2, 3, i);
        }
        
        m_reverbInputSetter.Process(reverbInputSetterInput, m_reverbState);
        m_reverbToDelaySend.Update(m_quadEncoderBank.GetValue(1, 3, 1, 0));
        m_mixerState.m_returnGain[1].Update(m_quadEncoderBank.GetValue(1, 3, 3, 0) / 2);

        input.m_tempo.Update(m_globalEncoderBank.GetValue(0, 0, 0, 0));

        m_mixerState.m_masterChainInput.SetGain(0, m_globalEncoderBank.GetValue(1, 0, 2, 0));
        m_mixerState.m_masterChainInput.SetGain(1, m_globalEncoderBank.GetValue(1, 1, 2, 0));
        m_mixerState.m_masterChainInput.SetGain(2, m_globalEncoderBank.GetValue(1, 2, 2, 0));
        m_mixerState.m_masterChainInput.SetGain(3, m_globalEncoderBank.GetValue(1, 3, 2, 0));
        m_mixerState.m_masterChainInput.SetBassFreq(m_globalEncoderBank.GetValue(1, 0, 3, 0));
        m_mixerState.m_masterChainInput.SetCrossoverFreq(0, m_globalEncoderBank.GetValue(1, 1, 3, 0));
        m_mixerState.m_masterChainInput.SetCrossoverFreq(1, m_globalEncoderBank.GetValue(1, 2, 3, 0));
        m_mixerState.m_masterChainInput.SetMasterGain(m_globalEncoderBank.GetValue(1, 3, 3, 0));        

        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            m_sourceMixerState.m_sources[i].m_hpCutoff.Update(m_globalEncoderBank.GetValue(2, i, 0, 0));
            m_sourceMixerState.m_sources[i].m_lpFactor.Update(m_globalEncoderBank.GetValue(2, i, 1, 0));
            m_sourceMixerState.m_sources[i].m_gain.Update(std::min(1.0f, 2 * m_globalEncoderBank.GetValue(2, i, 3, 0)));
        }

        // UNDONE(DEEP_VOCODER)
        //
        m_deepVocoderState.SetSlew(m_globalEncoderBank.GetValue(3, 0, 0, 0), m_globalEncoderBank.GetValue(3, 1, 0, 0));
        
        for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
        {
            size_t row = 1 + trio;
            for (size_t voiceInTrio = 0; voiceInTrio < TheNonagonInternal::x_voicesPerTrio; ++voiceInTrio)
            {
                size_t i = trio * 3 + voiceInTrio;

                m_deepVocoderState.m_voiceInput[i].m_gainThreshold.Update(m_globalEncoderBank.GetValue(3, 0, row, 0));
                m_deepVocoderState.m_voiceInput[i].m_pitchRatioPre.Update(m_globalEncoderBank.GetValue(3, 1, row, 0));
                m_deepVocoderState.m_voiceInput[i].m_slopeDown.Update(m_globalEncoderBank.GetValue(3, 2, row, 0));
                m_deepVocoderState.m_voiceInput[i].m_slopeUp.Update(1.0 - m_globalEncoderBank.GetValue(3, 3, row, 0));
            }
        }
    }   

    JSON ToJSON()
    {
        JSON rootJ = JSON::Object();
        rootJ.SetNew("voiceEncoderBank", m_voiceEncoderBank.ToJSON());
        rootJ.SetNew("globalEncoderBank", m_globalEncoderBank.ToJSON());
        rootJ.SetNew("quadEncoderBank", m_quadEncoderBank.ToJSON());
        if (this->GetRecordingDirectory().length() > 0)
        {
            rootJ.SetNew("recordingDirectory", JSON::String(this->GetRecordingDirectory().c_str()));
        }

        return rootJ;
    }

    void FromJSON(JSON rootJ)
    {
        m_voiceEncoderBank.FromJSON(rootJ.Get("voiceEncoderBank"));
        m_quadEncoderBank.FromJSON(rootJ.Get("quadEncoderBank"));
        m_globalEncoderBank.FromJSON(rootJ.Get("globalEncoderBank"));
        JSON recordingDirectoryJ = rootJ.Get("recordingDirectory");
        if (!recordingDirectoryJ.IsNull() && strlen(recordingDirectoryJ.StringValue()) > 0)
        {
            this->SetRecordingDirectory(recordingDirectoryJ.StringValue());
        }
    }

    void RevertToDefault()
    {
        m_voiceEncoderBank.RevertToDefault();
    }

    void ClearGesture(int gesture)
    {
        m_voiceEncoderBank.ClearGesture(gesture);
        m_quadEncoderBank.ClearGesture(gesture);
        m_globalEncoderBank.ClearGesture(gesture);
    }

    // Returns the gestures affecting the specified ordinal bank for the specified track
    //
    BitSet16 GetGesturesAffectingBankForTrack(size_t ordinal, size_t track)
    {
        if (ordinal < x_numVoiceBanks)
        {
            return m_voiceEncoderBank.GetGesturesAffectingBankForTrack(ordinal, track);
        }
        else if (ordinal < x_numVoiceBanks + x_numQuadBanks)
        {
            return m_quadEncoderBank.GetGesturesAffectingBankForTrack(ordinal - x_numVoiceBanks, track);
        }
        else
        {
            return m_globalEncoderBank.GetGesturesAffectingBankForTrack(ordinal - x_numVoiceBanks - x_numQuadBanks, track);
        }
    }

    // Returns true if the gesture affects the specified ordinal bank for the specified track
    //
    bool IsGestureAffectingBank(int gesture, size_t ordinal, size_t track)
    {
        if (ordinal < x_numVoiceBanks)
        {
            return m_voiceEncoderBank.IsGestureAffectingBank(gesture, ordinal, track);
        }
        else if (ordinal < x_numVoiceBanks + x_numQuadBanks)
        {
            return m_quadEncoderBank.IsGestureAffectingBank(gesture, ordinal - x_numVoiceBanks, track);
        }
        else
        {
            return m_globalEncoderBank.IsGestureAffectingBank(gesture, ordinal - x_numVoiceBanks - x_numQuadBanks, track);
        }
    }

    // Returns true if the gesture affects any bank for any track
    //
    bool IsGestureAffectingAnyBank(int gesture)
    {
        return m_voiceEncoderBank.GetGesturesAffecting().Get(gesture) ||
               m_quadEncoderBank.GetGesturesAffecting().Get(gesture) ||
               m_globalEncoderBank.GetGesturesAffecting().Get(gesture);
    }

    SmartGrid::Color GetGestureColor(int gesture)
    {
        SmartGrid::Color voiceResult = m_voiceEncoderBank.GetGestureColor(gesture);        
        SmartGrid::Color quadResult = m_quadEncoderBank.GetGestureColor(gesture);        
        SmartGrid::Color globalResult = m_globalEncoderBank.GetGestureColor(gesture);        
        int found = (voiceResult != SmartGrid::Color::Off) + (quadResult != SmartGrid::Color::Off) + (globalResult != SmartGrid::Color::Off);
        if (found > 1)
        {
            return SmartGrid::Color::White;
        }
        else if (found == 1)
        {
            return voiceResult != SmartGrid::Color::Off ? voiceResult : quadResult != SmartGrid::Color::Off ? quadResult : globalResult;
        }
        else
        {
            return SmartGrid::Color::Grey.Dim();
        }
    }
    void ResetGrid(uint64_t ix)
    {
        if (ix < SquiggleBoyWithEncoderBank::x_numVoiceBanks)
        {
            m_voiceEncoderBank.ResetGrid(ix);
        }
        else if (ix < SquiggleBoyWithEncoderBank::x_numVoiceBanks + SquiggleBoyWithEncoderBank::x_numQuadBanks)
        {
            m_quadEncoderBank.ResetGrid(ix - SquiggleBoyWithEncoderBank::x_numVoiceBanks);
        }
        else
        {
            m_globalEncoderBank.ResetGrid(ix - SquiggleBoyWithEncoderBank::x_numVoiceBanks - SquiggleBoyWithEncoderBank::x_numQuadBanks);
        }
    }

    void Apply(SmartGrid::MessageIn msg)
    {
        m_voiceEncoderBank.Apply(msg);
        m_globalEncoderBank.Apply(msg);
        m_quadEncoderBank.Apply(msg);
    }

    void PopulateUIState(UIState* uiState)
    {
        m_voiceEncoderBank.PopulateUIState(&uiState->m_encoderBankUIState);
        m_quadEncoderBank.PopulateUIState(&uiState->m_encoderBankUIState);
        m_globalEncoderBank.PopulateUIState(&uiState->m_encoderBankUIState);

        SmartGrid::Color colors[3] = 
        { 
            SmartGrid::Color::Cyan.AdjustBrightness(0.5), 
            SmartGrid::Color::Indigo.AdjustBrightness(0.5), 
            SmartGrid::Color::SeaGreen.AdjustBrightness(0.5) 
        };

        uiState->m_encoderBankUIState.SetModulationGlyph(2, SmartGridOne::ModulationGlyphs::SmoothRandom, colors[0]);
        uiState->m_encoderBankUIState.SetModulationGlyph(3, SmartGridOne::ModulationGlyphs::SmoothRandom, colors[1]);
        uiState->m_encoderBankUIState.SetModulationGlyph(6, SmartGridOne::ModulationGlyphs::LFO, colors[2]);
        uiState->m_encoderBankUIState.SetModulationGlyph(7, SmartGridOne::ModulationGlyphs::LFO, colors[0]);
        uiState->m_encoderBankUIState.SetModulationGlyph(11, SmartGridOne::ModulationGlyphs::Spread, colors[1]);
        uiState->m_encoderBankUIState.SetModulationGlyph(14, SmartGridOne::ModulationGlyphs::Noise, colors[2]);
        if (m_selectedAbsoluteEncoderBank < x_numVoiceBanks)
        {
            uiState->m_encoderBankUIState.SetModulationGlyph(4, SmartGridOne::ModulationGlyphs::ADSR, colors[0]);
            uiState->m_encoderBankUIState.SetModulationGlyph(5, SmartGridOne::ModulationGlyphs::ADSR, colors[1]);
            uiState->m_encoderBankUIState.SetModulationGlyph(8, SmartGridOne::ModulationGlyphs::Sheaf, colors[0]);
            uiState->m_encoderBankUIState.SetModulationGlyph(9, SmartGridOne::ModulationGlyphs::Sheaf, colors[1]);
            uiState->m_encoderBankUIState.SetModulationGlyph(10, SmartGridOne::ModulationGlyphs::Sheaf, colors[2]);
        }
        else if (m_selectedAbsoluteEncoderBank < x_numVoiceBanks + x_numQuadBanks)
        {
            uiState->m_encoderBankUIState.SetModulationGlyph(4, SmartGridOne::ModulationGlyphs::Quadrature, SmartGrid::Color::Pink);
            uiState->m_encoderBankUIState.SetModulationGlyph(5, SmartGridOne::ModulationGlyphs::Quadrature, SmartGrid::Color::Purple);
            uiState->m_encoderBankUIState.SetModulationGlyph(8, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            uiState->m_encoderBankUIState.SetModulationGlyph(9, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            uiState->m_encoderBankUIState.SetModulationGlyph(10, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
        }
        else
        {
            uiState->m_encoderBankUIState.SetModulationGlyph(4, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            uiState->m_encoderBankUIState.SetModulationGlyph(5, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            uiState->m_encoderBankUIState.SetModulationGlyph(8, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            uiState->m_encoderBankUIState.SetModulationGlyph(9, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
            uiState->m_encoderBankUIState.SetModulationGlyph(10, SmartGridOne::ModulationGlyphs::None, SmartGrid::Color::Off);
        }
        
        uiState->m_activeTrack.store(m_voiceEncoderBank.GetCurrentTrack());

        uiState->m_audioScopeWriter.Publish();
        uiState->m_controlScopeWriter.Publish();
        uiState->m_quadScopeWriter.Publish();
        uiState->m_sourceMixerScopeWriter.Publish();
        uiState->m_monoScopeWriter.Publish();
        
        uiState->ReadMeters();

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            uiState->SetPos(i, m_voices[i].m_pan.m_outputX, m_voices[i].m_pan.m_outputY);

            float hpAlpha = m_voices[i].m_filter.m_hpFilter.m_stage4.m_alpha;
            float lpAlpha = m_voices[i].m_filter.m_lpFilter.m_stage4.m_alpha;
            float lpResonance = m_voices[i].m_filter.m_lpFilter.m_kEff;
            float hpResonance = m_voices[i].m_filter.m_hpFilter.m_kEff;
            uiState->SetFilterParams(i, hpAlpha, lpAlpha, hpResonance, lpResonance);
        }

        m_mixerState.m_masterChainInput.PopulateUIState(&uiState->m_stereoMasteringChainUIState);
        m_delay.PopulateUIState(&uiState->m_delayUIState, m_delayState);
        m_reverb.PopulateUIState(&uiState->m_reverbUIState);

        m_sourceMixer.PopulateUIState(&uiState->m_sourceMixerUIState);
        m_deepVocoder.PopulateUIState(&uiState->m_deepVocoderUIState);

        if (m_selectedAbsoluteEncoderBank == 0)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Voice);
        }
        else if (m_selectedAbsoluteEncoderBank == 1)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Filter);
        }
        else if (m_selectedAbsoluteEncoderBank == 2)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::PanAndMelody);
        }
        else if (m_selectedAbsoluteEncoderBank == 3)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Control);
        }
        else if (m_selectedAbsoluteEncoderBank == x_numVoiceBanks + x_numQuadBanks)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::QuadMaster);
        }
        else if (m_selectedAbsoluteEncoderBank == x_numVoiceBanks + x_numQuadBanks + 1 ||
                 m_selectedAbsoluteEncoderBank == x_numVoiceBanks + x_numQuadBanks + 2 ||
                 m_selectedAbsoluteEncoderBank == x_numVoiceBanks + x_numQuadBanks + 3)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::StereoMaster);
        }
        else if (m_selectedAbsoluteEncoderBank == x_numVoiceBanks)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Delay);
        }
        else if (m_selectedAbsoluteEncoderBank == x_numVoiceBanks + 1)
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Reverb);
        }
        else
        {
            uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Voice);
        }
    }

    void ProcessSample(Input& input, float deltaT, const AudioInputBuffer& audioInputBuffer)
    {
        m_shift = input.m_shift;
        input.m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_shift = m_shift;
        input.m_quadEncoderBankInput.m_bankedEncoderInternalInput.m_shift = m_shift;
        input.m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_shift = m_shift;

        SetVoiceModulators(input);
        SetGlobalModulators(input);
        SetQuadModulators(input);

        m_voiceEncoderBank.Process(input.m_voiceEncoderBankInput, deltaT);
        m_quadEncoderBank.Process(input.m_quadEncoderBankInput, deltaT);
        m_globalEncoderBank.Process(input.m_globalEncoderBankInput, deltaT);

        SetEncoderParameters(input);

        SquiggleBoy::ProcessSample(audioInputBuffer);
    }
};