#pragma once

#include "SquiggleBoySource.hpp"
#include "SquiggleBoyTrack.hpp"
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
#include "PartialMachine.hpp"
#include "KMixMidi.hpp"
#include "Math.hpp"
#include "SampleTimer.hpp"
#include "StateSaver.hpp"
#include "AudioBuffer.hpp"
#include "RecordingBuffer.hpp"
#include "SquiggleBoyVoiceConfig.hpp"
#include "RecordingManager.hpp"
#include "Saturator.hpp"
#include "DirectoryExplorer.hpp"
#include "SnapshotUIState.hpp"
#include "Oversample.hpp"
#include "TransferFunction.hpp"

#include <algorithm>
#include <cstring>

struct SquiggleBoy
{
    static constexpr size_t x_numVoices = 9;
    static constexpr size_t x_numTracks = 3;
    static constexpr size_t x_voicesPerTrack = x_numVoices / x_numTracks;
    static constexpr size_t x_numGangedRandomLFOs = SquiggleBoyTrack::x_numGangedRandomLFOs;

    static_assert(x_voicesPerTrack == 3, "tracks are trios");

    struct MixerInput : QuadMixerInternal::Input
    {
        ScopeWriterHolder m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::NumScopes)];

        MixerInput()
        {
        }
    };

    SquiggleBoyWaveTableGenerator m_waveTableGenerator[2];

    SquiggleBoyTrack m_tracks[x_numTracks];
    GangedRandomLFO<4> m_quadGangedRandomLFO[x_numGangedRandomLFOs];
    GangedRandomLFO<1> m_globalGangedRandomLFO[2];
    SquiggleLFOGeneric<4> m_quadSquiggleLFO[2];

    SourceMixer m_sourceMixer;
    SourceMixer::Input m_sourceMixerState;

    DeepVocoder m_deepVocoder;
    DeepVocoder::Input m_deepVocoderState;

    PartialMachine m_partialMachine;
    PartialMachine::Input m_partialMachineState;
    PartialMachine::InputSetter m_partialMachineInputSetter;
    PartialMachine::InputSetter::Input m_partialMachineInputSetterInput;

    QuadMixerInternal m_mixer;

    QuadDelay m_delay;
    QuadReverb m_reverb;


    QuadFloatWithStereoAndSub m_output;

    SquiggleBoyVoiceConfig m_voiceConfig[x_numVoices];
    SquiggleBoyTrack::Input m_trackState[x_numTracks];
    GangedRandomLFOInput m_gangedRandomLFOInput[x_numGangedRandomLFOs];
    GangedRandomLFOInput m_globalGangedRandomLFOInput[2];
    SquiggleLFOGeneric<4>::Input m_quadSquiggleLFOInput[2];
    MixerInput m_mixerState;

    QuadDelayInputSetter m_delayInputSetter;
    QuadReverbInputSetter m_reverbInputSetter;

    QuadDelay::Input m_delayState;
    QuadReverb::Input m_reverbState;
    TheoryOfTime* m_theoryOfTime;

    PhaseUtils::SimpleOsc m_panPhase;

    PhaseUtils::ZeroedExpParam m_masterVolume;

    RGen m_rGen;

    bool m_firstFrame;
    StateSaver* m_stateSaver;

    IoTaskThread* m_ioTaskThread;

    RecordingManager m_recordingManager;

    SquiggleBoy(SmartGridOneContext* context, StateSaver* stateSaver)
        : m_mixer(context)
        , m_stateSaver(stateSaver)
        , m_ioTaskThread(nullptr)
    {
        m_mixerState.m_numInputs = x_numVoices + SourceMixer::x_numOutputChannels;
        m_mixerState.m_numMonoInputs = x_numVoices;
        m_theoryOfTime = nullptr;

        for (size_t track = 0; track < x_numTracks; ++track)
        {
            m_trackState[track].m_sourceInput.m_sourceMixer = &m_sourceMixer;
            m_trackState[track].m_gangedRandomLFOInput = m_gangedRandomLFOInput;
            for (size_t i = 0; i < x_voicesPerTrack; ++i)
            {
                m_trackState[track].m_voiceConfig[i] = &m_voiceConfig[track * x_voicesPerTrack + i];
            }
        }

        m_gangedRandomLFOInput[0] = GangedRandomLFOInput::Standard(1.0, 0.1f);
        m_gangedRandomLFOInput[1] = GangedRandomLFOInput::Standard(4.0, 0.3f);
        m_gangedRandomLFOInput[2] = GangedRandomLFOInput::Standard(12.0, 0.2f);
        m_gangedRandomLFOInput[3] = GangedRandomLFOInput::Standard(32.0, 0.1f);

        m_globalGangedRandomLFOInput[0] = GangedRandomLFOInput::Standard(8.0, 0.2f);
        m_globalGangedRandomLFOInput[1] = GangedRandomLFOInput::Standard(16.0, 0.1f);

        m_firstFrame = true;
    }

    const std::string& GetRecordingDirectory() const
    {
        return m_mixer.m_recordingDirectory;
    }

    bool IsRecording() const
    {
        return m_mixer.IsRecording();
    }

    StreamingRecorder::State GetRecordingState() const
    {
        return m_mixer.GetRecordingState();
    }

    StreamingRecorder::Error GetRecordingError() const
    {
        return m_mixer.GetRecordingError();
    }

    bool PrepareRecording()
    {
        return m_mixer.PrepareRecording(m_mixerState.m_numInputs, m_mixerState.m_numMonoInputs,
            static_cast<uint32_t>(SampleTimer::x_hostSampleRate));
    }

    void ShutdownRecording()
    {
        m_mixer.ShutdownRecording();
    }

    bool StartRecording(JSON initialPatch)
    {
        return m_mixer.StartRecording(m_mixerState.m_numInputs,
            static_cast<uint32_t>(SampleTimer::x_hostSampleRate), initialPatch);
    }

    void SetRecordingDirectory(const std::string& directory)
    {
        m_mixer.m_recordingDirectory = directory;
    }

    void ProcessSends()
    {
        m_delayState.m_input = m_mixer.m_send[0];
        m_mixerState.m_return[0] = m_delay.Process(m_delayState);
        m_delayState.m_return = m_mixerState.m_return[0];

        m_reverbState.m_input = m_mixer.m_send[1];
        m_mixerState.m_return[1] = m_reverb.Process(m_reverbState);
        m_reverbState.m_return = m_mixerState.m_return[1];

        m_mixerState.m_return[2] = m_partialMachine.Process(m_mixer.m_send[2], m_partialMachineState);
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
                    m_waveTableGenerator[i].SetLeft(&m_tracks[j].m_source.m_dualWaveShapingVCO.m_vco[i], k);
                }

                m_waveTableGenerator[i].Clear();
                m_waveTableGenerator[i].GenerateCompletely();
                for (size_t k = 0; k < x_voicesPerTrack; ++k)
                {
                    m_waveTableGenerator[i].SetRight(&m_tracks[j].m_source.m_dualWaveShapingVCO.m_vco[i], k);
                }

                m_waveTableGenerator[i].m_leftVisible[j] = true;
                m_waveTableGenerator[i].m_rightVisible[j] = false;
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
                        if (m_tracks[j].m_gangedRandomLFO[2 + i].Output(k) < 1)
                        {
                            needLeft = false;
                            m_waveTableGenerator[i].m_leftVisible[j] = true;
                        }

                        if (m_tracks[j].m_gangedRandomLFO[2 + i].Output(k) > 0)
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
                            m_waveTableGenerator[i].SetRight(&m_tracks[j].m_source.m_dualWaveShapingVCO.m_vco[i], k);
                        }

                        m_waveTableGenerator[i].m_rightVisible[j] = false;
                        m_waveTableGenerator[i].Clear();
                        break;
                    }

                    if (needLeft)
                    {
                        INFO("New left wavetable for VCO %zu track %zu", i, j);
                        for (size_t k = 0; k < x_voicesPerTrack; ++k)
                        {
                            m_waveTableGenerator[i].SetLeft(&m_tracks[j].m_source.m_dualWaveShapingVCO.m_vco[i], k);
                        }

                        m_waveTableGenerator[i].m_leftVisible[j] = false;
                        m_waveTableGenerator[i].Clear();
                        break;
                    }
                }                
            }
        }
    }

    void ProcessFrame()
    {
        ProcessWaveTableGenerators();
    }

    void ProcessSample(const AudioInputBuffer& audioInputBuffer, const bool* sourceMonitor, bool transportRunning)
    {
        m_sourceMixerState.SetInputs(audioInputBuffer);

        if (m_firstFrame)
        {
            m_firstFrame = false;
            InitRandomWaveTables();
        }

        m_panPhase.Process();
        m_partialMachineState.m_synthesisContextInput.m_azimuthOffset = m_panPhase.m_phase;

        for (size_t j = 0; j < x_numGangedRandomLFOs; ++j)
        {
            m_quadGangedRandomLFO[j].Process(SampleTimer::x_samplePeriod, m_gangedRandomLFOInput[j]);
        }

        m_globalGangedRandomLFO[0].Process(SampleTimer::x_samplePeriod, m_globalGangedRandomLFOInput[0]);
        m_globalGangedRandomLFO[1].Process(SampleTimer::x_samplePeriod, m_globalGangedRandomLFOInput[1]);

        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            m_quadSquiggleLFO[lfo].Process(m_quadSquiggleLFOInput[lfo]);
        }

        m_sourceMixer.Process(m_sourceMixerState);

        for (size_t track = 0; track < x_numTracks; ++track)
        {
            SquiggleBoyTrack& squiggleBoyTrack = m_tracks[track];
            m_trackState[track].m_panInput.m_input = m_panPhase.m_phase;

            SquiggleBoyTrack::Vector output = squiggleBoyTrack.Processs(m_trackState[track]);
            for (size_t i = 0; i < x_voicesPerTrack; ++i)
            {
                size_t voice = track * x_voicesPerTrack + i;
                m_mixerState.m_input[voice] = output[i];
                m_mixerState.m_monoIn[voice] = squiggleBoyTrack.m_amp.m_subOut[i];
                m_mixerState.m_x[voice] = squiggleBoyTrack.m_pan.m_outputX[i];
                m_mixerState.m_y[voice] = squiggleBoyTrack.m_pan.m_outputY[i];
            }
        }

        if (SampleTimer::GetSample() % SampleTimer::x_sampleRate == 0)
        {
            m_tracks[0].DebugPrint();
        }

        m_deepVocoderState.m_enabled = m_sourceMixer.IsVocoderEnabled(m_sourceMixerState);
        float deepVocoderInput = m_sourceMixer.GetDeepVocoderInput(m_sourceMixerState);
        m_deepVocoder.Process(deepVocoderInput, m_deepVocoderState);
        for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
        {
            bool sourceIsStereo = m_sourceMixerState.m_sources[i].m_config.IsStereo();

            for (size_t lane = 0; lane < SourceMixer::x_numSourceLanes; ++lane)
            {
                size_t sourceInputIndex = i * SourceMixer::x_numSourceLanes + lane;
                size_t inputIndex = x_numVoices + i * SourceMixer::x_numSourceLanes + lane;
                m_mixerState.m_input[inputIndex] = m_sourceMixer.m_sources[i].m_output[lane];
                m_mixerState.m_monoIn[inputIndex] = 0.0f;
                if (sourceIsStereo)
                {
                    size_t panIndex = sourceInputIndex + 1;
                    m_mixerState.m_x[inputIndex] = (panIndex & 1) ? 1.0f : 0.0f;
                    m_mixerState.m_y[inputIndex] = (panIndex & 2) ? 1.0f : 0.0f;
                }
                else
                {
                    m_mixerState.m_x[inputIndex] = 0.5f;
                    m_mixerState.m_y[inputIndex] = 0.5f;
                }

                m_mixerState.m_gain[inputIndex].m_expParam = 1.0f;
                m_mixerState.m_monitor[inputIndex] = sourceMonitor[i];

                for (size_t j = 0; j < QuadMixerInternal::x_numSends; ++j)
                {
                    m_mixerState.m_sendGain[inputIndex][j].m_expParam = 0.0f;
                }
            }
        }

        m_mixer.ProcessInputs(m_mixerState);
        if (m_mixer.m_recordingFrameActive && m_theoryOfTime != nullptr)
        {
            m_mixer.m_recorder->SubmitParameter(0, m_theoryOfTime->m_sample.m_unmodulatedPhase);
            m_mixer.m_recorder->SubmitTransport(transportRunning);
        }


        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Dry)].Write(m_mixer.m_dryOutput);
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Sub)].Write(m_mixer.m_subOutput);

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
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Delay)].Write(m_mixer.m_returnOutput[0]);
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Reverb)].Write(m_mixer.m_returnOutput[1]);
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::PartialMachine)].Write(m_mixer.m_returnOutput[2]);
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
    SmartGridOneContext* m_context;

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
            SourceMixer,
            FXMatrix,
            Sub
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

            std::atomic<SquiggleBoyTrack::VoiceConfig::FilterMachine> m_filterMachine;

            VoiceFilterUIState()
                : m_filterMachine(SquiggleBoyTrack::VoiceConfig::FilterMachine::Ladder4Pole)
            {
            }

            float FrequencyResponse(float freq) const override
            {
                if (m_filterMachine.load() == SquiggleBoyTrack::VoiceConfig::FilterMachine::Ladder4Pole)
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
                if (m_filterMachine.load() == SquiggleBoyTrack::VoiceConfig::FilterMachine::Ladder4Pole)
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
            struct SampleDirectoryUIState
            {
                static constexpr size_t x_pathCapacity = 512;

                struct UISnapshot
                {
                    char m_path[x_pathCapacity];
                };

                struct UIState : SnapshotUIState<UISnapshot>
                {
                };

                UIState m_uiState;

                static void CopyPath(char* destination, const char* source)
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

                    std::strncpy(destination, source, x_pathCapacity - 1);
                    destination[x_pathCapacity - 1] = '\0';
                }

                const char* GetPath() const
                {
                    return m_uiState.GetCurrentSnapshot().m_path;
                }

                void SetPath(const char* path)
                {
                    UISnapshot& snapshot = m_uiState.BeginSnapshot();
                    CopyPath(snapshot.m_path, path);
                    m_uiState.CommitSnapshot();
                }
            };

            std::atomic<SquiggleBoyTrack::VoiceConfig::SourceMachine> m_sourceMachine;
            PhysicalModelingSource::UIState m_physicalModelingUIState;
            AudioBufferBank::UIState m_audioBufferBankUIState;
            SampleSource::UIState m_sampleSourceUIState;
            SampleDirectoryUIState m_sampleDirectoryUIState;

            VoiceSourceUIState()
                : m_sourceMachine(SquiggleBoyTrack::VoiceConfig::SourceMachine::DualWaveShapingVCO)
            {
            }
        };

        std::atomic<VisualDisplayMode> m_visualDisplayMode = VisualDisplayMode::Voice;
        std::atomic<StreamingRecorder::State> m_recordingState = StreamingRecorder::State::Idle;
        std::atomic<StreamingRecorder::Error> m_recordingError = StreamingRecorder::Error::None;

        EncoderBankUIState m_encoderBankUIState;
        ModulationMatrixUIState m_modulationMatrixUIState;
        ScopeWriter m_audioScopeWriter;
        ScopeWriter m_controlScopeWriter;
        ScopeWriter m_quadScopeWriter;
        ScopeWriter m_quadControlScopeWriter;
        ScopeWriter m_globalControlScopeWriter;
        ScopeWriter m_sourceMixerScopeWriter;
        ScopeWriter m_monoScopeWriter;
        ScopeWriter m_monoAudioScopeWriter;
        std::atomic<size_t> m_firstAudioSample{0};
        std::atomic<size_t> m_firstControlSample{0};

        GangedRandomLFOUIState<x_voicesPerTrack>
            m_gangedRandomLFOUIState[x_numGangedRandomLFOs][x_numTracks];
        GangedRandomLFOUIState<4> m_quadGangedRandomLFOUIState[x_numGangedRandomLFOs];
        GangedRandomLFOUIState<1> m_globalGangedRandomLFOUIState[2];

        std::atomic<size_t> m_activeTrack = 0;

        VoiceFilterUIState m_voiceFilterUIState[x_numVoices];
        VoiceSourceUIState m_voiceSourceUIState[x_numVoices];

        std::atomic<float> m_xPos[x_numVoices];
        std::atomic<float> m_yPos[x_numVoices];

        MeterReader m_voiceMeterReader[x_numVoices + SourceMixer::x_numOutputChannels];
        MeterReader m_subMeterReader[x_numVoices];
        QuadMeterReader m_returnMeterReader[QuadMixerInternal::x_numSends];
        QuadMeterReader m_quadMasterMeterReader;
        StereoMeterReader m_stereoMasterMeterReader;

        MultibandSaturator<4, 2>::UIState m_stereoMasteringChainUIState;

        QuadDelay::UIState m_delayUIState;
        QuadReverb::UIState m_reverbUIState;
        PartialMachine::UIState m_partialMachineUIState;

        SourceMixer::UIState m_sourceMixerUIState;
        DeepVocoder::UIState m_deepVocoderUIState;

        DirectoryExplorer::UIState m_directoryExplorerUIState;

        void ReadMeters()
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_subMeterReader[i].Process();
            }

            for (size_t i = 0; i < x_numVoices + SourceMixer::x_numOutputChannels; ++i)
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
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_subMeterReader[i].m_meter = &mixer->m_monoMeters[i];
            }

            for (size_t i = 0; i < x_numVoices + SourceMixer::x_numOutputChannels; ++i)
            {
                m_voiceMeterReader[i].m_meter = &mixer->m_voiceMeters[i];
            }
            
            for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
            {
                m_returnMeterReader[i].SetMeter(&mixer->m_returnMeters[i]);
            }

            m_quadMasterMeterReader.SetMeter(&mixer->m_masterMeter);
            m_stereoMasterMeterReader.SetMeter(&mixer->m_stereoMeter);

            mixer->m_masterChain.m_stereoMasteringChain.m_saturator.SetupUIState(&m_stereoMasteringChainUIState);
        }


        void PopulateVoiceFilterUIState(
            size_t i,
            SquiggleBoyTrack::Shape* shape,
            size_t channel,
            SquiggleBoyTrack::VoiceConfig::FilterMachine filterMachine)
        {
            shape->m_ladder4Pole.m_lpLadder.PopulateUIState(&m_voiceFilterUIState[i].m_lpLadder, channel);
            shape->m_ladder4Pole.m_hp4Pole.PopulateUIState(&m_voiceFilterUIState[i].m_hp4Pole, channel);
            shape->m_svf2Pole.m_lpSVF.PopulateUIState(&m_voiceFilterUIState[i].m_lpSVF, channel);
            shape->m_svf2Pole.m_hpSVF.PopulateUIState(&m_voiceFilterUIState[i].m_hpSVF, channel);
            m_voiceFilterUIState[i].m_filterMachine.store(filterMachine);
        }

        void PopulateVoiceSourceUIState(
            size_t i,
            SquiggleBoyTrack::VoiceConfig* voiceConfig,
            SquiggleBoyTrack::Source* squiggleBoySource,
            size_t channel)
        {
            m_voiceSourceUIState[i].m_sourceMachine.store(voiceConfig->m_sourceMachine);

            AudioBufferBank* bank = voiceConfig->m_audioBufferBank;
            const char* path = (bank && !bank->m_directoryName.empty()) ? bank->m_directoryName.c_str() : "";
            m_voiceSourceUIState[i].m_sampleDirectoryUIState.SetPath(path);

            if (voiceConfig->m_sourceMachine == SquiggleBoyTrack::VoiceConfig::SourceMachine::PhysicalModeling)
            {
                squiggleBoySource->m_physicalModeling.PopulateUIState(&m_voiceSourceUIState[i].m_physicalModelingUIState, channel);
            }
            else if (voiceConfig->m_sourceMachine == SquiggleBoyTrack::VoiceConfig::SourceMachine::Sample)
            {
                squiggleBoySource->m_sampleSource.PopulateUIState(&m_voiceSourceUIState[i].m_sampleSourceUIState, channel);
                if (bank)
                {
                    bank->PopulateUIState(&m_voiceSourceUIState[i].m_audioBufferBankUIState);
                }
            }
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
            , m_controlScopeWriter(x_numVoices, static_cast<size_t>(SmartGridOne::ControlScopes::NumScopes), ScopeWriter::x_controlDecimation)
            , m_quadScopeWriter(4, static_cast<size_t>(SmartGridOne::QuadScopes::NumScopes))
            , m_quadControlScopeWriter(4, static_cast<size_t>(SmartGridOne::QuadControlScopes::NumScopes), ScopeWriter::x_controlDecimation)
            , m_globalControlScopeWriter(1, static_cast<size_t>(SmartGridOne::GlobalControlScopes::NumScopes), ScopeWriter::x_controlDecimation)
            , m_sourceMixerScopeWriter(SourceMixer::x_numSources, static_cast<size_t>(SmartGridOne::SourceScopes::NumScopes))
            , m_monoScopeWriter(1, static_cast<size_t>(SmartGridOne::MonoScopes::NumScopes), ScopeWriter::x_controlDecimation)
            , m_monoAudioScopeWriter(1, static_cast<size_t>(SmartGridOne::MonoAudioScopes::NumScopes))
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_xPos[i] = 0;
                m_yPos[i] = 0;
            }
        }

        void AdvanceScopeIndices()
        {
            if (m_audioScopeWriter.m_index == 0)
            {
                m_firstAudioSample.store(SampleTimer::GetSample());
            }

            m_audioScopeWriter.AdvanceIndex();
            m_quadScopeWriter.AdvanceIndex();
            m_sourceMixerScopeWriter.AdvanceIndex();
            m_monoAudioScopeWriter.AdvanceIndex();

            if (m_controlScopeWriter.m_index == 0 && m_controlScopeWriter.IsWriteSample())
            {
                m_firstControlSample.store(SampleTimer::GetSample());
            }

            m_controlScopeWriter.AdvanceIndex();
            m_quadControlScopeWriter.AdvanceIndex();
            m_globalControlScopeWriter.AdvanceIndex();
            m_monoScopeWriter.AdvanceIndex();
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
            size_t leftChannel = i * SourceMixer::x_numSourceLanes;
            size_t rightChannel = leftChannel + 1;
            bool sourceIsStereo = m_sourceMixerState.m_sources[i].m_config.IsStereo();

            kMixMidi->SetTrim(leftChannel, ccValue);
            kMixMidi->SetTrim(rightChannel, sourceIsStereo ? ccValue : 0.0f);
        }
    }

    void SetupScopeWriters(UIState* uiState)
    {
        m_delay.m_lfo.ConfigureScopeWriter(&uiState->m_quadControlScopeWriter, static_cast<size_t>(SmartGridOne::QuadControlScopes::DelayLFO));
        m_reverb.m_lfo.ConfigureScopeWriter(&uiState->m_quadControlScopeWriter, static_cast<size_t>(SmartGridOne::QuadControlScopes::ReverbLFO));

        for (size_t i = 0; i < 4; ++i)
        {
            m_quadSquiggleLFO[0].m_scopeWriter[i] = ScopeWriterHolder(
                &uiState->m_quadControlScopeWriter,
                i,
                static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadLFO1));
            m_quadSquiggleLFO[1].m_scopeWriter[i] = ScopeWriterHolder(
                &uiState->m_quadControlScopeWriter,
                i,
                static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadLFO2));
        }

        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Delay)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Delay));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Reverb)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Reverb));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Master)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Master));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Dry)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Dry));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Sub)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Sub));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::Stereo)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::Stereo));
        m_mixerState.m_scopeWriter[static_cast<size_t>(SmartGridOne::QuadScopes::PartialMachine)] = ScopeWriterHolder(&uiState->m_quadScopeWriter, 0, static_cast<size_t>(SmartGridOne::QuadScopes::PartialMachine));

        m_partialMachine.SetupAudioScopeWriter(&uiState->m_monoAudioScopeWriter);

        m_sourceMixer.SetupScopeWriters(&uiState->m_sourceMixerScopeWriter);
        
        for (size_t track = 0; track < x_numTracks; ++track)
        {
            m_tracks[track].SetupAudioScopeWriters(&uiState->m_audioScopeWriter, track);
            m_tracks[track].SetupControlScopeWriters(&uiState->m_controlScopeWriter, track);
        }
    }

    void SetupUIState(UIState* uiState)
    {
        for (size_t random = 0; random < x_numGangedRandomLFOs; ++random)
        {
            for (size_t track = 0; track < x_numTracks; ++track)
            {
                m_tracks[track].m_gangedRandomLFO[random].m_history =
                    &uiState->m_gangedRandomLFOUIState[random][track].m_history;
            }

            m_quadGangedRandomLFO[random].m_history =
                &uiState->m_quadGangedRandomLFOUIState[random].m_history;
        }

        for (size_t random = 0; random < 2; ++random)
        {
            m_globalGangedRandomLFO[random].m_history =
                &uiState->m_globalGangedRandomLFOUIState[random].m_history;
        }

        uiState->SetMeterReaders(&m_mixer);
        m_sourceMixer.SetupUIState(&uiState->m_sourceMixerUIState, m_sourceMixerState);
        SetupScopeWriters(uiState);
    }

    struct Input
    {
        float m_baseFreq[x_numVoices];
        
        float m_sheafyModulators[x_numVoices][3];

        float m_faders[x_numFaders];
        bool m_sourceMonitor[SourceMixer::x_numSources];
        bool m_transportRunning = false;
        SampleTop m_top;

        PhaseUtils::ExpParam m_tempo;

        float GetGainFader(size_t i)
        {
            return m_faders[5 + i / x_numTracks];
        }

        Input()
            : m_tempo(SampleTimer::HzToCycPerSamp(1.0 / 64.0), SampleTimer::HzToCycPerSamp(4.0))
        {
            for (size_t i = 0; i < x_numVoices; ++i)
            {
                m_baseFreq[i] = PhaseUtils::VOctToNatural(0.0, SampleTimer::x_samplePeriod);
                m_sheafyModulators[i][0] = 0;
                m_sheafyModulators[i][1] = 0;
                m_sheafyModulators[i][2] = 0;
            }

            for (size_t i = 0; i < x_numFaders; ++i)
            {
                m_faders[i] = 0;
            }

            for (size_t i = 0; i < SourceMixer::x_numSources; ++i)
            {
                m_sourceMonitor[i] = true;
            }

            m_top = false;
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
        if (m_context->m_sceneManager.m_shift)
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
        for (size_t track = 0; track < x_numTracks; ++track)
        {
            m_trackState[track].m_squiggleLFOInput[0].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_trackState[track].m_squiggleLFOInput[1].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_trackState[track].m_squiggleLFOInput[0].m_polyXFaderInput.m_phaseDomain = PhaseDomain::Modulated;
            m_trackState[track].m_squiggleLFOInput[1].m_polyXFaderInput.m_phaseDomain = PhaseDomain::Modulated;

            m_trackState[track].SetTheoryOfTimeInputs(m_theoryOfTime);

            for (size_t i = 0; i < x_voicesPerTrack; ++i)
            {
                size_t voice = track * x_voicesPerTrack + i;
                m_tracks[track].m_recordingBuffer[i].m_theoryOfTime = m_theoryOfTime;
                m_recordingManager.m_voiceRecordingBuffers[voice] = &m_tracks[track].m_recordingBuffer[i];
                m_recordingManager.m_voiceBufferWriters[voice] = &m_tracks[track].m_recordingBufferWriter[i];
                m_recordingManager.m_voiceConfig[voice] = &m_voiceConfig[voice];
            }
        }

        m_recordingManager.m_ioTaskThread = m_ioTaskThread;

        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            m_quadSquiggleLFOInput[lfo].m_polyXFaderInput.m_theoryOfTime = m_theoryOfTime;
            m_quadSquiggleLFOInput[lfo].m_polyXFaderInput.m_phaseDomain = PhaseDomain::Modulated;
        }
    }

    SquiggleBoyWithEncoderBank(SmartGridOneContext* context, StateSaver* stateSaver)
        : SquiggleBoy(context, stateSaver)
        , m_encoders(context)
        , m_context(context)
    {
    }

    void SetTrack(size_t track)
    {
        UpdateEncodersForMachine(track);
        m_encoders.SetTrack(track);
    }

    void UpdateEncodersForMachine(size_t track)
    {
        size_t voiceIx = track * x_voicesPerTrack;
        m_encoders.UpdateEncodersForMachine(
            m_voiceConfig[voiceIx].m_sourceMachine,
            m_voiceConfig[voiceIx].m_filterMachine,
            track);
    }

    void UpdateEncodersForMachine()
    {
        UpdateEncodersForMachine(static_cast<size_t>(m_encoders.GetCurrentTrack()));
    }

    void ResetBank(Bank bank)
    {
        m_encoders.ResetBank(bank);
    }

    void SetVoiceModulators(Input& input)
    {
        for (size_t track = 0; track < x_numTracks; ++track)
        {
            auto& modulatorValues = m_encoders.GetModulatorValues(SmartGridOneEncoders::VoiceModeForTrio(track));
            SquiggleBoyTrack& squiggleBoyTrack = m_tracks[track];
            for (size_t trackVoice = 0; trackVoice < x_voicesPerTrack; ++trackVoice)
            {
                size_t voice = track * x_voicesPerTrack + trackVoice;
                for (size_t j = 0; j < x_numGangedRandomLFOs; ++j)
                {
                    modulatorValues.Set(j, trackVoice, squiggleBoyTrack.m_gangedRandomLFO[j].Output(trackVoice));
                }

                const AHD& modulationAHD = squiggleBoyTrack.m_amp.m_modulationAHD.m_channels[trackVoice];
                const AHD& ampAHD = squiggleBoyTrack.m_amp.m_ahd.m_channels[trackVoice];
                modulatorValues.Set(VoiceModulator::ModulationEnvelope, trackVoice, modulationAHD.m_rawOutput);
                modulatorValues.Set(VoiceModulator::AmpEnvelope, trackVoice, ampAHD.m_rawOutput);
                modulatorValues.SetAmplitude(VoiceModulator::ModulationEnvelope, trackVoice, modulationAHD.m_amplitude);
                modulatorValues.SetAmplitude(VoiceModulator::AmpEnvelope, trackVoice, ampAHD.m_amplitude);

                modulatorValues.Set(VoiceModulator::LFO1, trackVoice, squiggleBoyTrack.m_squiggleLFO[0].m_output[trackVoice]);
                modulatorValues.Set(VoiceModulator::LFO2, trackVoice, squiggleBoyTrack.m_squiggleLFO[1].m_output[trackVoice]);
                modulatorValues.SetAmplitude(VoiceModulator::LFO1, trackVoice, squiggleBoyTrack.m_squiggleLFO[0].m_polyXFader.m_amplitude[trackVoice]);
                modulatorValues.SetAmplitude(VoiceModulator::LFO2, trackVoice, squiggleBoyTrack.m_squiggleLFO[1].m_polyXFader.m_amplitude[trackVoice]);

                modulatorValues.Set(VoiceModulator::Sheaf1, trackVoice, input.m_sheafyModulators[voice][0]);
                modulatorValues.Set(VoiceModulator::Sheaf2, trackVoice, input.m_sheafyModulators[voice][1]);
                modulatorValues.Set(VoiceModulator::Sheaf3, trackVoice, input.m_sheafyModulators[voice][2]);

                modulatorValues.Set(VoiceModulator::Spread, trackVoice, static_cast<float>(trackVoice) / (x_voicesPerTrack - 1));

                modulatorValues.Set(VoiceModulator::Noise, trackVoice, m_rGen.UniGen());
            }

            for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numGestureParams; ++i)
            {
                modulatorValues.m_gestureWeights[i] = input.m_faders[i];
            }
        }
    }

    void SetQuadModulators(Input& input)
    {
        auto& modulatorValues = m_encoders.GetModulatorValues(BankMode::Quad);
    
        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t random = 0; random < x_numGangedRandomLFOs; ++random)
            {
                modulatorValues.Set(random, i, m_quadGangedRandomLFO[random].Output(i));
            }

            modulatorValues.Set(QuadModulator::DelayLFO, i, m_delay.m_lfo.m_output[i] / 2.0 + 0.5);
            modulatorValues.Set(QuadModulator::ReverbLFO, i, m_reverb.m_lfo.m_output[i] / 2.0 + 0.5);

            modulatorValues.Set(QuadModulator::LFO1, i, m_quadSquiggleLFO[0].m_output[i]);
            modulatorValues.Set(QuadModulator::LFO2, i, m_quadSquiggleLFO[1].m_output[i]);
            modulatorValues.SetAmplitude(QuadModulator::LFO1, i, m_quadSquiggleLFO[0].m_polyXFader.m_amplitude[i]);
            modulatorValues.SetAmplitude(QuadModulator::LFO2, i, m_quadSquiggleLFO[1].m_polyXFader.m_amplitude[i]);

            modulatorValues.Set(QuadModulator::Spread, i, static_cast<float>(i % 4) / 3);

            modulatorValues.Set(QuadModulator::Noise, i, m_rGen.UniGen());
        }

        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numGestureParams; ++i)
        {
            modulatorValues.m_gestureWeights[i] = input.m_faders[i];
        }    
    }

    void SetGlobalModulators(Input& input)
    {
        auto& modulatorValues = m_encoders.GetModulatorValues(BankMode::Global);

        modulatorValues.Set(GlobalModulator::Random1, 0, m_globalGangedRandomLFO[0].Output(0));
        modulatorValues.Set(GlobalModulator::Random2, 0, m_globalGangedRandomLFO[1].Output(0));

        modulatorValues.Set(GlobalModulator::Noise, 0, m_rGen.UniGen());

        for (size_t i = 0; i < SmartGrid::BankedEncoderCell::x_numGestureParams; ++i)
        {
            modulatorValues.m_gestureWeights[i] = input.m_faders[i];
        }
    }

    void SetEncoderParameters(Input& input)
    {
        for (size_t track = 0; track < x_numTracks; ++track)
        {
            SquiggleBoyTrack::Vector targetFreq;
            for (size_t i = 0; i < x_voicesPerTrack; ++i)
            {
                targetFreq[i] = input.m_baseFreq[track * x_voicesPerTrack + i];
            }

            m_tracks[track].SetEncoderParams(
                m_encoders,
                m_trackState[track],
                track,
                targetFreq,
                SquiggleBoyTrack::Vector::Splat(input.GetGainFader(track * x_voicesPerTrack)));
        }

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            m_mixerState.m_gain[i].Update(m_encoders.GetValue(Param::AmpGain, i));
            m_mixerState.m_sendGain[i][0].Update(m_encoders.GetValue(Param::DelaySend, i));
            m_mixerState.m_sendGain[i][1].Update(m_encoders.GetValue(Param::ReverbSend, i));
            m_mixerState.m_sendGain[i][2].Update(m_encoders.GetValue(Param::PartialMachineSend, i));
        }

        QuadDelayInputSetter::Input delayInputSetterInput;
        for (int i = 0; i < 4; ++i)
        {
            delayInputSetterInput.m_loopSelectorSwitchVal[i] =
                m_encoders.GetSwitchVal(Param::DelayTime, i);
            delayInputSetterInput.m_delayTimeFactorSwitchVal[i] = m_encoders.GetSwitchVal(Param::DelayTimeFactor, i);
            delayInputSetterInput.m_readHeadSpeedSwitchVal[i] = m_encoders.GetSwitchVal(Param::DelayReadSpeed, i);
            delayInputSetterInput.m_rotateSwitchVal[i] = m_encoders.GetSwitchVal(Param::DelayRotate, i);
            delayInputSetterInput.m_resynthShiftSwitchVal[i] = m_encoders.GetSwitchVal(Param::ResynthShiftInterval, i);
        }

        delayInputSetterInput.m_feedbackKnob = m_encoders.GetValue<4>(Param::DelayFeedback, 0);
        delayInputSetterInput.m_dampingBaseKnob = m_encoders.GetValue<4>(Param::DelayDampBase, 0);
        delayInputSetterInput.m_dampingWidthKnob = m_encoders.GetValue<4>(Param::DelayDampWidth, 0);
        delayInputSetterInput.m_resynthSlewUpKnob = m_encoders.GetValue<4>(Param::ResynthSlewUp, 0);
        delayInputSetterInput.m_widenKnob = m_encoders.GetValue<4>(Param::DelayWiden, 0);
        delayInputSetterInput.m_resynthUnisonKnob = m_encoders.GetValue<4>(Param::ResynthUnison, 0);
        delayInputSetterInput.m_resynthShiftFadeKnob = m_encoders.GetValue<4>(Param::ResynthShiftFade, 0);
        delayInputSetterInput.m_modFreqKnob = m_encoders.GetValue<4>(Param::DelayModSpeed, 0);
        delayInputSetterInput.m_lfoPhaseKnob = m_encoders.GetValue<4>(Param::DelayModPhaseShift, 0);

        delayInputSetterInput.m_theoryOfTime = m_theoryOfTime;
        m_delayInputSetter.Process(delayInputSetterInput, m_delayState, &m_delay);
        m_mixerState.m_returnSendGain[0][1].Update(m_encoders.GetValue(Param::DelayReverbSend));
        m_mixerState.m_returnSendGain[0][2].Update(m_encoders.GetValue(Param::DelayPartialMachineSend));
        m_mixerState.m_returnGain[0].Update(m_encoders.GetValue(Param::DelayReturn));

        static constexpr Param x_quadLFOSkew[2] = {Param::QuadLFO1Skew, Param::QuadLFO2Skew};
        static constexpr Param x_quadLFOMult[2] = {Param::QuadLFO1Mult, Param::QuadLFO2Mult};
        static constexpr Param x_quadLFOShape[2] = {Param::QuadLFO1Shape, Param::QuadLFO2Shape};
        static constexpr Param x_quadLFOAmplitude[2] = {Param::QuadLFO1Amplitude, Param::QuadLFO2Amplitude};
        static constexpr Param x_quadLFOCenter[2] = {Param::QuadLFO1Center, Param::QuadLFO2Center};
        static constexpr Param x_quadLFOSlope[2] = {Param::QuadLFO1Slope, Param::QuadLFO2Slope};
        static constexpr Param x_quadLFOPhaseShift[2] = {Param::QuadLFO1PhaseShift, Param::QuadLFO2PhaseShift};
        static constexpr Param x_quadLFOSampleAndHold[2] = {Param::QuadLFO1SampleAndHold, Param::QuadLFO2SampleAndHold};
        QuadFloat channelFrac(0.0f, 0.25f, 0.5f, 0.75f);
        bool delayLoopTop[4];
        for (int i = 0; i < 4; ++i)
        {
            delayLoopTop[i] = m_theoryOfTime->CrossedCycleBoundary(
                m_delayInputSetter.m_readTapeHead[i].m_loopSelector,
                PhaseDomain::Modulated).m_triggered;
        }

        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            SquiggleLFOGeneric<4>::Input& lfoInput = m_quadSquiggleLFOInput[lfo];
            PolyXFaderGeneric<4>::Input& faderInput = lfoInput.m_polyXFaderInput;
            faderInput.m_attackFrac = m_encoders.GetValue<4>(x_quadLFOSkew[lfo], 0);
            lfoInput.m_mult.Update(m_encoders.GetValue<4>(x_quadLFOMult[lfo], 0));
            faderInput.m_shape = m_encoders.GetValue<4>(x_quadLFOShape[lfo], 0);
            faderInput.m_amplitude = m_encoders.GetValue<4>(x_quadLFOAmplitude[lfo], 0);
            faderInput.m_center = QuadFloat::Splat(1.0f) - m_encoders.GetValue<4>(x_quadLFOCenter[lfo], 0);
            faderInput.m_slope = m_encoders.GetValue<4>(x_quadLFOSlope[lfo], 0);
            faderInput.m_phaseShift = m_encoders.GetValue<4>(x_quadLFOPhaseShift[lfo], 0) * channelFrac;
            faderInput.m_theoryOfTime = m_theoryOfTime;
            faderInput.m_phaseDomain = PhaseDomain::Modulated;
            faderInput.m_shFade = m_encoders.GetValue<4>(x_quadLFOSampleAndHold[lfo], 0);
            for (int i = 0; i < 4; ++i)
            {
                faderInput.m_trig[i] = delayLoopTop[i];
            }
        }

        QuadReverbInputSetter::Input reverbInputSetterInput;
        reverbInputSetterInput.m_reverbTimeKnob = m_encoders.GetValue<4>(Param::ReverbTime, 0);
        reverbInputSetterInput.m_feedbackKnob = m_encoders.GetValue<4>(Param::ReverbFeedback, 0);
        reverbInputSetterInput.m_dampingBaseKnob = m_encoders.GetValue<4>(Param::ReverbDampBase, 0);
        reverbInputSetterInput.m_dampingWidthKnob = m_encoders.GetValue<4>(Param::ReverbDampWidth, 0);
        reverbInputSetterInput.m_modDepthKnob = m_encoders.GetValue<4>(Param::ReverbModDepth, 0);
        reverbInputSetterInput.m_modFreqKnob = m_encoders.GetValue<4>(Param::ReverbModSpeed, 0);
        reverbInputSetterInput.m_lfoPhaseKnob = m_encoders.GetValue<4>(Param::ReverbModPhaseShift, 0);
        
        m_reverbInputSetter.Process(reverbInputSetterInput, m_reverbState);
        m_mixerState.m_returnSendGain[1][0].Update(m_encoders.GetValue(Param::ReverbDelaySend));
        m_mixerState.m_returnSendGain[1][2].Update(m_encoders.GetValue(Param::ReverbPartialMachineSend));
        m_mixerState.m_returnGain[1].Update(m_encoders.GetValue(Param::ReverbReturn));

        m_partialMachineInputSetterInput.m_attack = m_encoders.GetValue<4>(Param::PartialMachineAttack, 0);
        m_partialMachineInputSetterInput.m_decay = m_encoders.GetValue<4>(Param::PartialMachineDecay, 0);
        m_partialMachineInputSetterInput.m_portamento = m_encoders.GetValue<4>(Param::PartialMachinePortamento, 0);
        m_partialMachineInputSetterInput.m_density = m_encoders.GetValue<4>(Param::PartialMachineDensity, 0);
        m_partialMachineInputSetterInput.m_bwBaseFrequency = m_encoders.GetValue<4>(Param::PartialMachineBWBase, 0);
        m_partialMachineInputSetterInput.m_bwWidth = m_encoders.GetValue<4>(Param::PartialMachineBWWidth, 0);
        m_partialMachineInputSetterInput.m_reductionFeedback = m_encoders.GetValue<4>(Param::PartialMachineReductionFeedback, 0);
        m_partialMachineInputSetterInput.m_bassCutoff = m_encoders.GetValue<4>(Param::PartialMachineBassCutoff, 0);
        m_partialMachineInputSetterInput.m_azimuthFactor = m_encoders.GetValue<4>(Param::PartialMachineAzimuthFactor, 0);
        m_partialMachineInputSetterInput.m_unison = m_encoders.GetValue<4>(Param::PartialMachineUnison, 0);
        m_partialMachineInputSetterInput.m_parameterLinearFrequency = m_encoders.GetValue<4>(Param::PartialMachineLinearFrequency, 0);
        m_partialMachineInputSetterInput.m_pitchShiftDepth = m_encoders.GetValue<4>(Param::PartialMachinePitchShiftDepth, 0);
        m_partialMachineInputSetterInput.m_pitchShift = m_encoders.GetValue<4>(Param::PartialMachinePitchShift, 0);

        m_partialMachineInputSetter.SetInput(m_partialMachineInputSetterInput, m_partialMachineState);
        m_mixerState.m_returnSendGain[2][0].Update(m_encoders.GetValue(Param::PartialMachineDelaySend));
        m_mixerState.m_returnSendGain[2][1].Update(m_encoders.GetValue(Param::PartialMachineReverbSend));
        m_mixerState.m_returnGain[2].Update(m_encoders.GetValue(Param::PartialMachineVolume));

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

        m_sourceMixerState.m_sources[0].m_hpCutoff.Update(m_encoders.GetValue(Param::Source1HP));
        m_sourceMixerState.m_sources[0].m_lpFactor.Update(m_encoders.GetValue(Param::Source1LP));
        m_sourceMixerState.m_sources[0].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValue(Param::Source1Gain)));
        m_sourceMixerState.m_sources[1].m_hpCutoff.Update(m_encoders.GetValue(Param::Source2HP));
        m_sourceMixerState.m_sources[1].m_lpFactor.Update(m_encoders.GetValue(Param::Source2LP));
        m_sourceMixerState.m_sources[1].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValue(Param::Source2Gain)));
        m_sourceMixerState.m_sources[2].m_hpCutoff.Update(m_encoders.GetValue(Param::Source3HP));
        m_sourceMixerState.m_sources[2].m_lpFactor.Update(m_encoders.GetValue(Param::Source3LP));
        m_sourceMixerState.m_sources[2].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValue(Param::Source3Gain)));
        m_sourceMixerState.m_sources[3].m_hpCutoff.Update(m_encoders.GetValue(Param::Source4HP));
        m_sourceMixerState.m_sources[3].m_lpFactor.Update(m_encoders.GetValue(Param::Source4LP));
        m_sourceMixerState.m_sources[3].m_gain.Update(std::min(1.0f, 2 * m_encoders.GetValue(Param::Source4Gain)));

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

    JSON ToJSON(JsonArena& a)
    {
        return m_encoders.ToJSON(a);
    }

    void FromJSON(JSON rootJ)
    {
        m_encoders.FromJSON(rootJ);
    }

    void RevertToDefault(bool allScenes, bool allTracks)
    {
        std::ignore = allTracks;
        m_encoders.RevertToDefault(allScenes);
    }

    void ClearGesture(int gesture)
    {
        m_encoders.ClearGesture(gesture);
    }

    BitSet16 GetGesturesAffectingBankForTrack(Bank bank, size_t track)
    {
        BankMode mode = m_encoders.GetModeForBank(bank);
        size_t effectiveTrack = SmartGridOneEncoders::IsVoiceMode(mode) ? track : 0;
        return m_encoders.GetGesturesAffectingBankForTrack(bank, effectiveTrack);
    }

    bool IsGestureAffectingBank(int gesture, Bank bank, size_t track)
    {
        BankMode mode = m_encoders.GetModeForBank(bank);
        size_t effectiveTrack = SmartGridOneEncoders::IsVoiceMode(mode) ? track : 0;
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
        uiState->m_recordingState.store(GetRecordingState());
        uiState->m_recordingError.store(GetRecordingError());
        m_encoders.PopulateUIState(&uiState->m_encoderBankUIState);
        m_encoders.PopulateModulationMatrixUIState(&uiState->m_modulationMatrixUIState);
        uiState->m_activeTrack.store(m_encoders.GetCurrentTrack());

        for (size_t random = 0; random < x_numGangedRandomLFOs; ++random)
        {
            for (size_t track = 0; track < x_numTracks; ++track)
            {
                m_tracks[track].m_gangedRandomLFO[random].PopulateUIState(
                    &uiState->m_gangedRandomLFOUIState[random][track]);
            }

            m_quadGangedRandomLFO[random].PopulateUIState(
                &uiState->m_quadGangedRandomLFOUIState[random]);
        }

        for (size_t random = 0; random < 2; ++random)
        {
            m_globalGangedRandomLFO[random].PopulateUIState(
                &uiState->m_globalGangedRandomLFOUIState[random]);
        }

        uiState->m_audioScopeWriter.Publish();
        uiState->m_controlScopeWriter.Publish();
        uiState->m_quadScopeWriter.Publish();
        uiState->m_quadControlScopeWriter.Publish();
        uiState->m_globalControlScopeWriter.Publish();
        uiState->m_sourceMixerScopeWriter.Publish();
        uiState->m_monoScopeWriter.Publish();
        uiState->m_monoAudioScopeWriter.Publish();
        
        uiState->ReadMeters();

        for (size_t track = 0; track < x_numTracks; ++track)
        {
            SquiggleBoyTrack& squiggleBoyTrack = m_tracks[track];
            for (size_t i = 0; i < x_voicesPerTrack; ++i)
            {
                size_t voice = track * x_voicesPerTrack + i;
                uiState->SetPos(voice, squiggleBoyTrack.m_pan.m_outputX[i], squiggleBoyTrack.m_pan.m_outputY[i]);
                uiState->PopulateVoiceFilterUIState(
                    voice,
                    &squiggleBoyTrack.m_shape,
                    i,
                    m_voiceConfig[voice].m_filterMachine);
                uiState->PopulateVoiceSourceUIState(
                    voice,
                    &m_voiceConfig[voice],
                    &squiggleBoyTrack.m_source,
                    i);
            }
        }

        m_mixerState.m_masterChainInput.PopulateUIState(&uiState->m_stereoMasteringChainUIState);
        m_delay.PopulateUIState(&uiState->m_delayUIState, m_delayState);
        m_reverb.PopulateUIState(&uiState->m_reverbUIState);
        m_partialMachine.PopulateUIState(uiState->m_partialMachineUIState, m_partialMachineState);

        m_sourceMixer.PopulateUIState(&uiState->m_sourceMixerUIState, m_sourceMixerState);
        m_deepVocoder.PopulateUIState(&uiState->m_deepVocoderUIState);

        Bank selectedBank = SmartGridOneEncoders::BankForTrio(m_encoders.m_selectedBank, 0);
        switch (selectedBank)
        {
            case Bank::Source:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Voice);
                break;
            }
            case Bank::Shape:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Filter);
                break;
            }
            case Bank::FXMatrix:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::FXMatrix);
                break;
            }
            case Bank::Sub:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Sub);
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
            case Bank::PartialMachine:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::QuadMaster);
                break;
            }
            case Bank::QuadLFOs:
            {
                uiState->m_visualDisplayMode.store(UIState::VisualDisplayMode::Control);
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
        SetVoiceModulators(input);
        SetGlobalModulators(input);
        SetQuadModulators(input);

        m_encoders.Process();

        SetEncoderParameters(input);

        this->SquiggleBoy::ProcessSample(audioInputBuffer, input.m_sourceMonitor, input.m_transportRunning);
    }
};
