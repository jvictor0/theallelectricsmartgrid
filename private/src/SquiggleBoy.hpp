#pragma once

#include "VectorPhaseShaper.hpp"
#include "EncoderBankBank.hpp"
#include "ButterworthFilter.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "RandomLFO.hpp"
#include "LadderFilter.hpp"
#include "ADSR.hpp"
#include "BitCrush.hpp"

struct SquiggleBoyVoice
{
    struct VCOSection
    {
        static constexpr size_t x_oversample = 4;
        VectorPhaseShaperInternal m_vco[2];

        RandomLFO m_driftLFO[2];

        BitRateReducer m_bitRateReducer;
        TanhSaturator<true> m_saturator;

        ButterworthFilter m_antiAliasFilter;

        float m_output;

        struct Input
        {
            float m_v[2];
            float m_d[2];
            float m_baseFreq;
            PhaseUtils::ExpParam m_offsetFreqFactor;
            float m_driftFreqFactor;

            float m_crossModIndex[2];

            float m_fade;

            float m_bitCrushAmount;

            float m_saturationGain;

            Input()
                : m_baseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
                , m_offsetFreqFactor(4)
                , m_driftFreqFactor(0)
                , m_crossModIndex{0, 0}
                , m_fade(0)
                , m_bitCrushAmount(0)
                , m_saturationGain(0.25)
            {
            }
        };

        Input m_state;

        VCOSection()
            : m_output(0)
        {
            m_antiAliasFilter.SetCyclesPerSample(0.40 / x_oversample);
        }

        float Process(const Input& input)
        {
            m_output = 0;

            float driftFactor[2] = 
            {
                std::powf(2, (2 * m_driftLFO[0].Process() - 1) * m_state.m_driftFreqFactor), 
                std::powf(2, (2 * m_driftLFO[1].Process() - 1) * m_state.m_driftFreqFactor)
            };

            for (size_t i = 0; i < x_oversample; ++i)
            {
                float interp = (i + 1) / static_cast<float>(x_oversample);

                VectorPhaseShaperInternal::Input vcoInput[2];

                vcoInput[0].m_useVoct = false;
                vcoInput[1].m_useVoct = false;
                
                vcoInput[0].m_v = m_state.m_v[0] + interp * (input.m_v[0] - m_state.m_v[0]);
                vcoInput[1].m_v = m_state.m_v[1] + interp * (input.m_v[1] - m_state.m_v[1]);
                vcoInput[0].m_d = m_state.m_d[0] + interp * (input.m_d[0] - m_state.m_d[0]);
                vcoInput[1].m_d = m_state.m_d[1] + interp * (input.m_d[1] - m_state.m_d[1]);
                vcoInput[0].m_phaseMod = (m_state.m_crossModIndex[0] + interp * (input.m_crossModIndex[0] - m_state.m_crossModIndex[0])) * m_vco[1].m_out;
                vcoInput[0].m_freq = (m_state.m_baseFreq + interp * (input.m_baseFreq - m_state.m_baseFreq)) / x_oversample;
                vcoInput[1].m_freq = vcoInput[0].m_freq * (m_state.m_offsetFreqFactor.m_expParam + interp * (input.m_offsetFreqFactor.m_expParam - m_state.m_offsetFreqFactor.m_expParam));

                vcoInput[0].m_freq *= driftFactor[0];
                vcoInput[1].m_freq *= driftFactor[1];

                m_vco[0].Process(vcoInput[0], 0 /*unused*/);
                vcoInput[1].m_phaseMod = m_vco[0].m_out * (m_state.m_crossModIndex[1] + interp * (input.m_crossModIndex[1] - m_state.m_crossModIndex[1]));
                m_vco[1].Process(vcoInput[1], 0 /*unused*/);

                float fade = m_state.m_fade + interp * (input.m_fade - m_state.m_fade);
                float mixed = m_vco[0].m_out * std::cos(fade * M_PI / 2) + m_vco[1].m_out * std::sin(fade * M_PI / 2);

                float bitCrushAmount = m_state.m_bitCrushAmount + interp * (input.m_bitCrushAmount - m_state.m_bitCrushAmount);
                m_bitRateReducer.SetAmount(bitCrushAmount);
                float crushed = m_bitRateReducer.Process(mixed);

                float saturationGain = m_state.m_saturationGain + interp * (input.m_saturationGain - m_state.m_saturationGain);
                m_saturator.SetInputGain(saturationGain);
                float saturated = m_saturator.Process(crushed);

                m_output += m_antiAliasFilter.Process(saturated);
            }

            m_output /= x_oversample;
            m_state = input;

            return m_output;
        }
    };

    struct FilterSection
    {
        OPLowPassFilter m_lowPassFilter;
        OPHighPassFilter m_highPassFilter;
        LadderFilter m_ladderFilter;
        ADSR m_adsr;

        SampleRateReducer m_sampleRateReducer;

        float m_output;

        FilterSection()
            : m_output(0)
        {
        }
        
        struct Input
        {
            PhaseUtils::ExpParam m_bwBase;
            PhaseUtils::ExpParam m_bwWidth;
            float m_vcoBaseFreq;
            PhaseUtils::ExpParam m_ladderCutoffFactor;
            float m_ladderResonance;
            float m_envDepth;

            ADSR::InputSetter m_adsrInputSetter;
            ADSR::Input m_adsrInput;

            PhaseUtils::ExpParam m_sampleRateReducerFreq;

            void SetADSR(float attack, float decay, float sustain, float release)
            {
                m_adsrInputSetter.Set(attack, decay, sustain, release, m_adsrInput);
            }

            Input()
                : m_bwBase(0.25f, 1024.0f)
                , m_bwWidth(1.0f, 1024.0f)
                , m_vcoBaseFreq(PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0))
                , m_ladderCutoffFactor(0.25f, 1024.0f)
                , m_ladderResonance(0)
                , m_envDepth(0)
                , m_sampleRateReducerFreq(0.25f, 1024.0f)
            {
            }
        };
        
        float Process(Input& input, float vcoOutput)
        {
            m_highPassFilter.SetAlphaFromNatFreq(input.m_vcoBaseFreq * input.m_bwBase.m_expParam);
            m_lowPassFilter.SetAlphaFromNatFreq(input.m_vcoBaseFreq * input.m_bwBase.m_expParam * input.m_bwWidth.m_expParam);

            float adsrEnv = m_adsr.Process(input.m_adsrInput);
            m_ladderFilter.SetCutoff(input.m_vcoBaseFreq * input.m_ladderCutoffFactor.m_expParam * std::powf(2.0f, input.m_envDepth * adsrEnv));
            m_ladderFilter.SetResonance(input.m_ladderResonance);

            m_output = m_lowPassFilter.Process(vcoOutput);
            m_output = m_highPassFilter.Process(m_output);
            m_output = m_ladderFilter.Process(m_output);

            m_sampleRateReducer.SetFreq(input.m_vcoBaseFreq * input.m_sampleRateReducerFreq.m_expParam);
            m_output = m_sampleRateReducer.Process(m_output);

            return m_output;
        }
    };

    struct AmpSection
    {
        ADSR m_adsr;

        float m_output;

        struct Input
        {
            ADSR::InputSetter m_adsrInputSetter;
            ADSR::Input m_adsrInput;

            PhaseUtils::ZeroedExpParam m_gain;

            void SetADSR(float attack, float decay, float sustain, float release)
            {
                m_adsrInputSetter.Set(attack, decay, sustain, release, m_adsrInput);
            }

            Input()
            {
            }
        };

        AmpSection()
            : m_output(0)
        {
        }

        float Process(Input& input, float filterOutput)
        {
            float adsrEnv = m_adsr.Process(input.m_adsrInput);
            m_output = input.m_gain.m_expParam * adsrEnv * filterOutput;

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

            return m_output;
        }

        SquiggleLFO()
            : m_output(0)
        {
            m_shSlew.SetAlphaFromNatFreq(500.0f / 48000.0f);
        }
    };

    OPLowPassFilter m_baseFreqSlew;
    PhaseUtils::ExpParam m_baseFreqSlewAmount;
    VCOSection m_vco;
    FilterSection m_filter;
    AmpSection m_amp;
    PanSection m_pan;
    SquiggleLFO m_squiggleLFO[2];

    float m_output;

    struct Input
    {
        bool m_gate;
        bool m_trig;

        VCOSection::Input m_vcoInput;
        FilterSection::Input m_filterInput;
        AmpSection::Input m_ampInput;
        PanSection::Input m_panInput;
        SquiggleLFO::Input m_squiggleLFOInput[2];

        Input()
            : m_gate(false)
            , m_trig(false)
        {
        }

        void SetGates()
        {
            m_filterInput.m_adsrInput.m_gate = m_gate;
            m_filterInput.m_adsrInput.m_trig = m_trig;

            m_ampInput.m_adsrInput.m_gate = m_gate;
            m_ampInput.m_adsrInput.m_trig = m_trig;

            m_squiggleLFOInput[0].m_trig = m_trig;
            m_squiggleLFOInput[1].m_trig = m_trig;
        }
    };

    SquiggleBoyVoice()
        : m_baseFreqSlewAmount(0.125 / 48000.0, 500.0 / 48000.0)
        , m_output(0)
    {
    }

    float Processs(Input& input)
    {
        m_output = m_vco.Process(input.m_vcoInput);
        m_output = m_filter.Process(input.m_filterInput, m_output);
        m_output = m_amp.Process(input.m_ampInput, m_output);
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

    SquiggleBoyVoice m_voices[x_numVoices];
    ManyGangedRandomLFO m_gangedRandomLFO[2];
    ManyGangedRandomLFO m_globalGangedRandomLFO[2];

    QuadMixerInternal m_mixer;

    QuadDelayInternal<false> m_delay;
    QuadDelayInternal<true> m_reverb;
 
    QuadFloat m_output;

    SquiggleBoyVoice::Input m_state[x_numVoices];
    ManyGangedRandomLFO::Input m_gangedRandomLFOInput[2];
    ManyGangedRandomLFO::Input m_globalGangedRandomLFOInput[2];
    QuadMixerInternal::Input m_mixerState;

    QuadDelayInputSetter<false> m_delayInputSetter;
    QuadDelayInputSetter<true> m_reverbInputSetter;

    QuadDelayInternal<false>::Input m_delayState;
    QuadDelayInternal<true>::Input m_reverbState;
    float m_delayToReverbSend;
    float m_reverbToDelaySend;

    PhaseUtils::SimpleOsc m_panPhase;

    SquiggleBoy()
    {
        m_mixerState.m_numInputs = x_numVoices;
        m_delayToReverbSend = 0.0;
        m_reverbToDelaySend = 0.0;

        for (size_t i = 0; i < 2; ++i)
        {
            m_gangedRandomLFOInput[i].m_gangSize = x_numVoices / x_numTracks;
            m_gangedRandomLFOInput[i].m_time = 6.0;
            m_gangedRandomLFOInput[i].m_sigma = 0.2;
            m_gangedRandomLFOInput[i].m_numGangs = x_numTracks;

            m_globalGangedRandomLFOInput[i].m_gangSize = 1;
            m_globalGangedRandomLFOInput[i].m_time = 6.0;
            m_globalGangedRandomLFOInput[i].m_sigma = 0.2;
            m_globalGangedRandomLFOInput[i].m_numGangs = 1;
        }
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
        m_mixer.ToggleRecording(x_numVoices, 48000);
    }

    void SetRecordingDirectory(const std::string& directory)
    {
        m_mixer.m_recordingDirectory = directory;
    }

    void ProcessSends()
    {
        m_delayState.m_input = m_mixer.m_send[0] + m_mixerState.m_return[1] * m_reverbToDelaySend;
        m_mixerState.m_return[0] = m_delay.Process(m_delayState);
        m_delayState.m_return = m_mixerState.m_return[0];

        m_reverbState.m_input = m_mixer.m_send[1] + m_mixerState.m_return[0] * m_delayToReverbSend;
        m_mixerState.m_return[1] = m_reverb.Process(m_reverbState);
    }

    void Process()
    {
        m_panPhase.Process();

        m_gangedRandomLFO[0].Process(1.0 / 48000.0, m_gangedRandomLFOInput[0]);
        m_gangedRandomLFO[1].Process(1.0 / 48000.0, m_gangedRandomLFOInput[1]);

        m_globalGangedRandomLFO[0].Process(1.0 / 48000.0, m_globalGangedRandomLFOInput[0]);
        m_globalGangedRandomLFO[1].Process(1.0 / 48000.0, m_globalGangedRandomLFOInput[1]);

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_state[i].m_panInput.m_input = m_panPhase.m_phase;

            m_mixerState.m_input[i] = m_voices[i].Processs(m_state[i]);
            m_mixerState.m_x[i] = m_voices[i].m_pan.m_outputX;
            m_mixerState.m_y[i] = m_voices[i].m_pan.m_outputY;
        }

        m_mixer.ProcessInputs(m_mixerState);

        ProcessSends();

        m_output = m_mixer.ProcessReturns(m_mixerState);
    }
};

struct SquiggleBoyWithEncoderBank : SquiggleBoy
{
    static constexpr size_t x_numVoiceBanks = 4;
    EncoderBankBankInternal<x_numVoiceBanks> m_voiceEncoderBank;

    static constexpr size_t x_numGlobalBanks = 3;
    EncoderBankBankInternal<x_numGlobalBanks> m_globalEncoderBank;

    size_t m_selectedAbsoluteEncoderBank;
    size_t m_selectedGridId;

    bool m_shift;

    struct Input
    {
        EncoderBankBankInternal<x_numVoiceBanks>::Input m_voiceEncoderBankInput;
        EncoderBankBankInternal<x_numGlobalBanks>::Input m_globalEncoderBankInput;

        float m_baseFreq[x_numVoices];

        float m_sheafyModulators[x_numVoices][3];

        float m_faders[8];

        float m_totalPhasor[SquiggleBoyVoice::SquiggleLFO::x_numPhasors];

        PhaseUtils::ExpParam m_tempo;

        bool m_shift;

        float GetGainFader(size_t i)
        {
            return m_faders[5 + i / x_numTracks];
        }

        Input()
            : m_tempo(1.0 / 64.0, 4.0)
            , m_shift(false)
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_baseFreq[i] = PhaseUtils::VOctToNatural(0.0, 1.0 / 48000.0);
                m_sheafyModulators[i][0] = 0;
                m_sheafyModulators[i][1] = 0;
                m_sheafyModulators[i][2] = 0;
            }

            for (size_t i = 0; i < 8; ++i)
            {
                m_faders[i] = 0;
            }

            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_numVoices = x_numVoices / x_numTracks;
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_numTracks = x_numTracks;

            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_numVoices = 1;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_numTracks = 1;
        }

        void SelectTrack(int track)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_track = track;
        }

        void SetLeftScene(int scene)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene1 = scene;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene1 = scene;
        }

        void SetRightScene(int scene)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene2 = scene;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_scene2 = scene;
        }

        void SetBlendFactor(float blendFactor)
        {
            m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_blendFactor = blendFactor;
            m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_sceneManagerInput.m_blendFactor = blendFactor;
        }
    };

    void SelectGridId()
    {
        if (m_selectedAbsoluteEncoderBank < x_numVoiceBanks)
        {
            m_voiceEncoderBank.SelectGrid(m_selectedAbsoluteEncoderBank);
            m_globalEncoderBank.SelectGrid(-1);
            m_selectedGridId = m_voiceEncoderBank.m_selectedGridId;
        }
        else
        {
            m_globalEncoderBank.SelectGrid(m_selectedAbsoluteEncoderBank - x_numVoiceBanks);
            m_voiceEncoderBank.SelectGrid(-1);
            m_selectedGridId = m_globalEncoderBank.m_selectedGridId;
        }
    }

    SmartGrid::Color GetSelectorColor(size_t ordinal)
    {
        if (ordinal < x_numVoiceBanks)
        {
            return m_voiceEncoderBank.GetSelectorColor(ordinal);
        }
        else
        {
            return m_globalEncoderBank.GetSelectorColor(ordinal - x_numVoiceBanks);
        }
    }

    void CopyToScene(int scene)
    {
        m_voiceEncoderBank.CopyToScene(scene);
        m_globalEncoderBank.CopyToScene(scene);
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
        }

        m_voiceEncoderBank.SetColor(0, SmartGrid::Color::Red, input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.SetColor(1, SmartGrid::Color::Green, input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.SetColor(2, SmartGrid::Color::Orange, input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.SetColor(3, SmartGrid::Color::Blue, input.m_voiceEncoderBankInput);
        m_globalEncoderBank.SetColor(0, SmartGrid::Color::Pink, input.m_globalEncoderBankInput);
        m_globalEncoderBank.SetColor(1, SmartGrid::Color::Purple, input.m_globalEncoderBankInput);
        m_globalEncoderBank.SetColor(2, SmartGrid::Color::Yellow, input.m_globalEncoderBankInput);

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

        m_voiceEncoderBank.Config(0, 2, 3, 0, "BW Base", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(0, 3, 3, 1, "BW Width", input.m_voiceEncoderBankInput);

        m_voiceEncoderBank.Config(1, 0, 1, 1, "Filter Cutoff", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 1, 0, "Filter Resonance", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 1, 0, "Filter Env Depth", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 0, 0, 0, "Filter ADSR Attack", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 0, 0.2, "Filter ADSR Decay", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 0, 0, "Filter ADSR Sustain", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 0, 0.5, "Filter ADSR Release", input.m_voiceEncoderBankInput);

        m_voiceEncoderBank.Config(1, 0, 2, 0, "Amp ADSR Attack", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 2, 0.3, "Amp ADSR Decay", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 2, 0.5, "Amp ADSR Sustain", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 2, 0.5, "Amp ADSR Release", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 0, 3, 1, "Amp Gain", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 1, 3, 0, "Delay Send", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 2, 3, 0, "Reverb Send", input.m_voiceEncoderBankInput);
        m_voiceEncoderBank.Config(1, 3, 3, 0.5, "Gate Length", input.m_voiceEncoderBankInput);

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

        m_globalEncoderBank.Config(0, 0, 0, 0.75, "Delay Time", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 1, 0, 0.25, "Delay Rotate", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 2, 0, 0, "Delay Mod Speed", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 3, 0, 0.75, "Delay Feedback", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 0, 1, 0.4, "Delay Damp Base", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 1, 1, 0.3, "Delay Damp Width", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 2, 1, 0.1, "Delay Mod Depth", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 3, 1, 0, "Reverb Send", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 0, 2, 0, "Delay Time Skew", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 1, 2, 1.0, "Delay Mod Phase Skew", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 2, 2, 0, "Delay Cross Mod", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(0, 3, 2, 1.0, "Delay Return", input.m_globalEncoderBankInput);

        m_globalEncoderBank.Config(1, 0, 0, 0.5, "Reverb Time", input.m_globalEncoderBankInput);
        // No rotate for reverb
        //
        m_globalEncoderBank.Config(1, 2, 0, 0.1, "Reverb Mod Speed", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 3, 0, 0.99, "Reverb Feedback", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 0, 1, 0.3, "Reverb Damp Base", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 1, 1, 0.5, "Reverb Damp Width", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 2, 1, 0, "Reverb Mod Depth", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 3, 1, 0, "Delay Send", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 0, 2, 1.0, "Reverb Time Skew", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 1, 2, 1.0, "Reverb Mod Phase Skew", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 2, 2, 0, "Reverb Cross Mod", input.m_globalEncoderBankInput);
        m_globalEncoderBank.Config(1, 3, 2, 1.0, "Reverb Return", input.m_globalEncoderBankInput);

        m_globalEncoderBank.Config(2, 0, 0, 0.5, "Tempo", input.m_globalEncoderBankInput);

        m_voiceEncoderBank.SetModulatorType(0, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_voiceEncoderBank.SetModulatorType(1, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_voiceEncoderBank.SetModulatorType(12, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_voiceEncoderBank.SetModulatorType(13, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_voiceEncoderBank.SetModulatorType(14, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);

        m_globalEncoderBank.SetModulatorType(0, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_globalEncoderBank.SetModulatorType(1, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_globalEncoderBank.SetModulatorType(12, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_globalEncoderBank.SetModulatorType(13, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
        m_globalEncoderBank.SetModulatorType(14, SmartGrid::BankedEncoderCell::EncoderType::GestureParam);
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
            modulatorValues.m_value[0][i] = input.m_faders[0];
            modulatorValues.m_value[1][i] = input.m_faders[1];

            modulatorValues.m_value[2][i] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[0].m_lfos[i / x_numTracks].m_pos[i % x_numTracks]));
            modulatorValues.m_value[3][i] = std::min(1.0f, std::max(0.0f, m_gangedRandomLFO[1].m_lfos[i / x_numTracks].m_pos[i % x_numTracks]));

            modulatorValues.m_value[4][i] = m_voices[i].m_filter.m_adsr.m_output;
            modulatorValues.m_value[5][i] = m_voices[i].m_amp.m_adsr.m_output;

            modulatorValues.m_value[6][i] = m_voices[i].m_squiggleLFO[0].m_output;
            modulatorValues.m_value[7][i] = m_voices[i].m_squiggleLFO[1].m_output;

            modulatorValues.m_value[8][i] = input.m_sheafyModulators[i][0];
            modulatorValues.m_value[9][i] = input.m_sheafyModulators[i][1];
            modulatorValues.m_value[10][i] = input.m_sheafyModulators[i][2];

            modulatorValues.m_value[11][i] = static_cast<float>(i % x_numTracks) / (x_numTracks - 1);
            
            modulatorValues.m_value[12][i] = input.m_faders[2];
            modulatorValues.m_value[13][i] = input.m_faders[3];
            modulatorValues.m_value[14][i] = input.m_faders[4];
        }
    }

    void SetGlobalModulators(Input& input)
    {
        SmartGrid::BankedEncoderCell::ModulatorValues& modulatorValues = input.m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_modulatorValues;

        modulatorValues.m_value[0][0] = input.m_faders[0];
        modulatorValues.m_value[1][0] = input.m_faders[1];

        modulatorValues.m_value[2][0] = std::min(1.0f, std::max(0.0f, m_globalGangedRandomLFO[0].m_lfos[0].m_pos[0]));
        modulatorValues.m_value[3][0] = std::min(1.0f, std::max(0.0f, m_globalGangedRandomLFO[1].m_lfos[0].m_pos[0]));

        modulatorValues.m_value[12][0] = input.m_faders[2];
        modulatorValues.m_value[13][0] = input.m_faders[3];
        modulatorValues.m_value[14][0] = input.m_faders[4];    
    }

    void SetEncoderParameters(Input& input)
    {
        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_voices[i].m_baseFreqSlew.SetAlphaFromNatFreq(m_voices[i].m_baseFreqSlewAmount.Update(1.0 - m_voiceEncoderBank.GetValue(2, 3, 1, i)));
            m_state[i].m_vcoInput.m_baseFreq = m_voices[i].m_baseFreqSlew.Process(input.m_baseFreq[i]);
            m_state[i].m_vcoInput.m_v[0] = m_voiceEncoderBank.GetValue(0, 1, 0, i) * 4.0;
            m_state[i].m_vcoInput.m_v[1] = m_voiceEncoderBank.GetValue(0, 1, 1, i) * 4.0;
            m_state[i].m_vcoInput.m_d[0] = m_voiceEncoderBank.GetValue(0, 2, 0, i);
            m_state[i].m_vcoInput.m_d[1] = m_voiceEncoderBank.GetValue(0, 2, 1, i);
            m_state[i].m_vcoInput.m_crossModIndex[0] = m_voiceEncoderBank.GetValue(0, 3, 0, i);
            m_state[i].m_vcoInput.m_crossModIndex[1] = m_voiceEncoderBank.GetValue(0, 3, 1, i);
            m_state[i].m_vcoInput.m_fade = m_voiceEncoderBank.GetValue(0, 0, 2, i);
            m_state[i].m_vcoInput.m_saturationGain = m_voiceEncoderBank.GetValue(0, 1, 2, i) * 4 + 0.25;
            m_state[i].m_vcoInput.m_bitCrushAmount = m_voiceEncoderBank.GetValue(0, 2, 2, i);
            m_state[i].m_filterInput.m_sampleRateReducerFreq.Update(1 - m_voiceEncoderBank.GetValue(0, 3, 2, i));
            m_state[i].m_vcoInput.m_offsetFreqFactor.Update(m_voiceEncoderBank.GetValue(0, 0, 3, i));
            m_state[i].m_vcoInput.m_driftFreqFactor = m_voiceEncoderBank.GetValue(0, 1, 3, i) / 24;
            
            m_state[i].m_filterInput.m_vcoBaseFreq = m_voices[i].m_baseFreqSlew.m_output;            
            m_state[i].m_filterInput.m_bwBase.Update(m_voiceEncoderBank.GetValue(0, 2, 3, i));
            m_state[i].m_filterInput.m_bwWidth.Update(m_voiceEncoderBank.GetValue(0, 3, 3, i));

            m_state[i].m_filterInput.m_ladderCutoffFactor.Update(m_voiceEncoderBank.GetValue(1, 0, 1, i));
            m_state[i].m_filterInput.m_ladderResonance = m_voiceEncoderBank.GetValue(1, 1, 1, i);
            m_state[i].m_filterInput.m_envDepth = m_voiceEncoderBank.GetValue(1, 3, 1, i) * 10;

            m_state[i].m_filterInput.SetADSR(
                m_voiceEncoderBank.GetValue(1, 0, 0, i), 
                m_voiceEncoderBank.GetValue(1, 1, 0, i), 
                m_voiceEncoderBank.GetValue(1, 2, 0, i), 
                m_voiceEncoderBank.GetValue(1, 3, 0, i));

            m_state[i].m_ampInput.SetADSR(
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
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_center = m_voiceEncoderBank.GetValue(3, 0, 2 * j + 1, i);
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_slope = m_voiceEncoderBank.GetValue(3, 1, 2 * j + 1, i);
                m_state[i].m_squiggleLFOInput[j].m_polyXFaderInput.m_phaseShift = m_voiceEncoderBank.GetValue(3, 2, 2 * j + 1, i) * (static_cast<float>(i % x_numTracks) / x_numTracks);
                m_state[i].m_squiggleLFOInput[j].m_shFade = m_voiceEncoderBank.GetValue(3, 3, 2 * j + 1, i);
            }

            m_mixerState.m_gain[i] = m_voiceEncoderBank.GetValue(1, 0, 3, i);
            m_mixerState.m_sendGain[i][0] = m_voiceEncoderBank.GetValue(1, 1, 3, i);
            m_mixerState.m_sendGain[i][1] = m_voiceEncoderBank.GetValue(1, 2, 3, i);

            m_state[i].SetGates();
        }

        m_delayInputSetter.SetDelayTime(m_globalEncoderBank.GetValue(0, 0, 0, 0), input.m_tempo.m_expParam, m_delayState);
        m_delayState.m_rotate = m_globalEncoderBank.GetValue(0, 1, 0, 0);
        m_delayInputSetter.SetModulation(
            m_globalEncoderBank.GetValue(0, 2, 0, 0), 
            m_globalEncoderBank.GetValue(0, 2, 1, 0),
            m_delayState);
        m_delayState.m_feedback = m_globalEncoderBank.GetValue(0, 3, 0, 0);
        m_delayInputSetter.SetDamping(
            m_globalEncoderBank.GetValue(0, 0, 1, 0),
            m_globalEncoderBank.GetValue(0, 1, 1, 0),
            m_delayState);
        m_delayToReverbSend = m_globalEncoderBank.GetValue(0, 3, 1, 0);
        m_delayInputSetter.SetWiden(m_globalEncoderBank.GetValue(0, 0, 2, 0), m_delayState);
        m_delayState.m_lfoInput.m_phaseOffset = m_globalEncoderBank.GetValue(0, 1, 2, 0);
        m_delayState.m_lfoInput.m_crossDepth = m_globalEncoderBank.GetValue(0, 2, 2, 0);
        m_mixerState.m_returnGain[0] = m_globalEncoderBank.GetValue(0, 3, 2, 0);

        m_reverbInputSetter.SetDelayTime(m_globalEncoderBank.GetValue(1, 0, 0, 0), 0, m_reverbState);
        m_reverbState.m_rotate = m_globalEncoderBank.GetValue(1, 1, 0, 0);
        m_reverbInputSetter.SetModulation(
            m_globalEncoderBank.GetValue(1, 2, 0, 0),
            m_globalEncoderBank.GetValue(1, 2, 1, 0),
            m_reverbState);
        m_reverbState.m_feedback = m_globalEncoderBank.GetValue(1, 3, 0, 0);
        m_reverbInputSetter.SetDamping(
            m_globalEncoderBank.GetValue(1, 0, 1, 0),
            m_globalEncoderBank.GetValue(1, 1, 1, 0),
            m_reverbState);
        m_reverbToDelaySend = m_globalEncoderBank.GetValue(1, 3, 1, 0);
        m_reverbInputSetter.SetWiden(m_globalEncoderBank.GetValue(1, 0, 2, 0), m_reverbState);
        m_reverbState.m_lfoInput.m_phaseOffset = m_globalEncoderBank.GetValue(1, 1, 2, 0);
        m_reverbState.m_lfoInput.m_crossDepth = m_globalEncoderBank.GetValue(1, 2, 2, 0);
        m_mixerState.m_returnGain[1] = m_globalEncoderBank.GetValue(1, 3, 2, 0);

        input.m_tempo.Update(m_globalEncoderBank.GetValue(2, 0, 0, 0));
    }   

    void SaveJSON()
    {
        m_voiceEncoderBank.SaveJSON();
        m_globalEncoderBank.SaveJSON();
    }
    
    void LoadSavedJSON()
    {
        m_voiceEncoderBank.LoadSavedJSON();
        m_globalEncoderBank.LoadSavedJSON();
    }

    json_t* ToJSON()
    {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "voiceEncoderBank", m_voiceEncoderBank.ToJSON());
        json_object_set_new(rootJ, "globalEncoderBank", m_globalEncoderBank.ToJSON());
        json_object_set_new(rootJ, "recordingDirectory", json_string(GetRecordingDirectory().c_str()));
        return rootJ;
    }

    void FromJSON(json_t* rootJ)
    {
        m_voiceEncoderBank.FromJSON(json_object_get(rootJ, "voiceEncoderBank"));
        m_globalEncoderBank.FromJSON(json_object_get(rootJ, "globalEncoderBank"));
        json_t* recordingDirectoryJ = json_object_get(rootJ, "recordingDirectory");
        if (recordingDirectoryJ)
        {
            SetRecordingDirectory(json_string_value(recordingDirectoryJ));
        }
    }

    void RevertToDefault()
    {
        m_voiceEncoderBank.RevertToDefault();
    }

    void ResetGrid(uint64_t ix)
    {
        if (ix < SquiggleBoy::x_numVoices)
        {
            m_voiceEncoderBank.ResetGrid(ix);
        }
        else
        {
            m_globalEncoderBank.ResetGrid(ix - SquiggleBoy::x_numVoices);
        }
    }

    void Process(Input& input, float deltaT)
    {
        m_shift = input.m_shift;
        input.m_voiceEncoderBankInput.m_bankedEncoderInternalInput.m_shift = m_shift;
        input.m_globalEncoderBankInput.m_bankedEncoderInternalInput.m_shift = m_shift;

        SetVoiceModulators(input);
        SetGlobalModulators(input);

        m_voiceEncoderBank.Process(input.m_voiceEncoderBankInput, deltaT);
        m_globalEncoderBank.Process(input.m_globalEncoderBankInput, deltaT);

        SetEncoderParameters(input);

        SquiggleBoy::Process();
    }
};

#ifndef IOS_BUILD
struct SquiggleBoyModule : Module
{
    IOMgr m_ioMgr;
    SquiggleBoyWithEncoderBank m_squiggleBoy;
    SquiggleBoyWithEncoderBank::Input m_state;
    
    // I/O pointers
    //
    IOMgr::Input* m_vpoInput;
    IOMgr::Output* m_gridIdOutput;
    IOMgr::Output* m_mainOutput;

    IOMgr::Input* m_pageSelectInput;
    IOMgr::Input* m_gateInput;

    SquiggleBoyModule()
        : m_ioMgr(this)
    {
        m_squiggleBoy.Config(m_state);

        // Add volt per octave input
        //
        m_vpoInput = m_ioMgr.AddInput("V/Oct", false);
        m_vpoInput->MakeVoltPerOctave();
        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            m_vpoInput->SetTarget(i, &m_state.m_baseFreq[i]);
        }

        // Add grid id output
        //
        m_gridIdOutput = m_ioMgr.AddOutput("Grid ID", false);
        m_gridIdOutput->SetSource(0, &m_squiggleBoy.m_selectedGridId);

        // Add main output
        //
        m_mainOutput = m_ioMgr.AddOutput("Main", true);
        m_mainOutput->m_scale = 5;
        m_mainOutput->SetChannels(4);
        for (size_t i = 0; i < 4; ++i)
        {
            m_mainOutput->SetSource(i, &m_squiggleBoy.m_output[i]);
        }

        m_pageSelectInput = m_ioMgr.AddInput("Page Select", false);
        m_pageSelectInput->SetChannels(1);
        m_pageSelectInput->SetTarget(0, &m_squiggleBoy.m_selectedAbsoluteEncoderBank);

        m_gateInput = m_ioMgr.AddInput("Gate", true);
        for (size_t i = 0; i < SquiggleBoy::x_numVoices; ++i)
        {
            m_gateInput->SetTarget(i, &m_squiggleBoy.m_state[i].m_gate);
        }

        m_ioMgr.Config();
    }

    void SelectGridId()
    {
        m_squiggleBoy.SelectGridId();
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        if (m_ioMgr.IsControlFrame())
        {
            SelectGridId();
        }

        m_squiggleBoy.Process(m_state, args.sampleTime);
        m_ioMgr.SetOutputs();
    }
};

struct SquiggleBoyWidget : public ModuleWidget
{
    SquiggleBoyWidget(SquiggleBoyModule* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SquiggleBoy.svg")));

        // Add widgets
        //
        if (module)
        {
            module->m_vpoInput->Widget(this, 1, 1);
            module->m_gridIdOutput->Widget(this, 1, 2);
            module->m_mainOutput->Widget(this, 1, 3);
            module->m_pageSelectInput->Widget(this, 1, 4);
            module->m_gateInput->Widget(this, 1, 5);
        }
    }
};
#endif

