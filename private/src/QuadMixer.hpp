#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "QuadLFO.hpp"
#include "SmartGridOneContext.hpp"
#include "Noise.hpp"
#include "QuadMasterChain.hpp"
#include "QuadToStereoMixdown.hpp"
#include "Metering.hpp"
#include "RecordingFormat.hpp"
#include "SmartGridBuildInfo.hpp"

struct QuadMixerInternal
{
    static constexpr size_t x_numSends = 3;
    static constexpr size_t x_maxInputs = 32;

    QuadFloatWithStereoAndSub m_output;
    QuadFloat m_send[x_numSends];
    StreamingRecorder* m_recorder;
    std::string m_recordingDirectory;
    size_t m_recordingNumInputs = 0;
    size_t m_recordingNumMonoInputs = 0;
    bool m_recordingFrameActive = false;
    PinkNoise m_pinkNoise;

    Meter m_voiceMeters[x_maxInputs];
    QuadMeter m_returnMeters[x_numSends];
    QuadMeter m_masterMeter;
    StereoMeter m_stereoMeter;

    static constexpr float x_smoothingAlpha = 0.0007;

    DualMasteringChain m_masterChain;
    QuadToStereoMixdown m_quadToStereoMixdown;

    QuadMixerInternal(SmartGridOneContext* context)
        : m_recorder(&context->m_recorder)
    {
    }
    
    struct Input
    {
        size_t m_numInputs;
        size_t m_numMonoInputs;
        float m_input[x_maxInputs];
        float m_monoIn[x_maxInputs];
        PhaseUtils::ZeroedExpParam m_gain[x_maxInputs];
        PhaseUtils::ZeroedExpParam m_sendGain[x_maxInputs][x_numSends];
        float m_x[x_maxInputs];
        float m_y[x_maxInputs];
        QuadFloat m_return[x_numSends];
        PhaseUtils::ZeroedExpParam m_returnGain[x_numSends];
        PhaseUtils::ZeroedExpParam m_returnSendGain[x_numSends][x_numSends];
        bool m_noiseMode;
        bool m_monitor[x_maxInputs];

        DualMasteringChain::Input m_masterChainInput;

        Input()
            : m_numInputs(0)
            , m_numMonoInputs(0)
            , m_input{}
            , m_monoIn{}
            , m_x{}
            , m_y{}
            , m_noiseMode(false)
            , m_monitor{}
        {
            for (size_t i = 0; i < x_maxInputs; ++i)
            {
                m_monitor[i] = true;
            }

            for (size_t i = 0; i < x_numSends; ++i)
            {
                m_returnSendGain[i][i].m_expParam = 0.0;
            }
        }
    };

    void ProcessReturnSends(const Input& input)
    {
        for (size_t returnIndex = 0; returnIndex < x_numSends; ++returnIndex)
        {
            for (size_t sendIndex = 0; sendIndex < x_numSends; ++sendIndex)
            {
                if (returnIndex != sendIndex)
                {
                    m_send[sendIndex] += input.m_return[returnIndex] * input.m_returnSendGain[returnIndex][sendIndex].m_expParam;
                }
            }
        }
    }

    static RecordingFormat::Session MakeRecordingSession(size_t numInputs, size_t numMonoInputs, uint32_t sampleRate)
    {
        RecordingFormat::Session session;
        session.m_sampleRate = sampleRate;
        session.m_blockFrames = sampleRate;
        session.m_gitCommitSha = SmartGridBuildInfo::x_gitCommitSha;
        if (numInputs > x_maxInputs || numMonoInputs > numInputs)
        {
            return session;
        }

        for (size_t i = 0; i < numInputs; ++i)
        {
            session.m_tracks.push_back(
            {
                static_cast<uint32_t>(session.m_tracks.size()),
                "input_" + std::to_string(i), RecordingFormat::TrackType::PannedMono,
                "input", "post_fader_post_shared_reduction"
            });
        }

        for (size_t i = 0; i < numMonoInputs; ++i)
        {
            session.m_tracks.push_back(
            {
                static_cast<uint32_t>(session.m_tracks.size()),
                "mono_" + std::to_string(i), RecordingFormat::TrackType::Mono,
                "mono_input", "post_shared_reduction"
            });
        }

        for (size_t i = 0; i < x_numSends; ++i)
        {
            session.m_tracks.push_back(
            {
                static_cast<uint32_t>(session.m_tracks.size()),
                "return_" + std::to_string(i), RecordingFormat::TrackType::Quad,
                "return", "post_gain_post_saturation"
            });
        }

        session.m_tracks.push_back(
        {
            static_cast<uint32_t>(session.m_tracks.size()),
            "master_quad", RecordingFormat::TrackType::Quad,
            "master_quad", "post_mastering_pre_master_volume"
        });
        session.m_tracks.push_back(
        {
            static_cast<uint32_t>(session.m_tracks.size()),
            "master_stereo", RecordingFormat::TrackType::Stereo,
            "master_stereo", "post_mastering_pre_master_volume"
        });
        return session;
    }

    bool PrepareRecording(size_t numInputs, size_t numMonoInputs, uint32_t sampleRate)
    {
        m_recordingNumInputs = numInputs;
        m_recordingNumMonoInputs = numMonoInputs;
        m_recordingFrameActive = false;
        return m_recorder->Prepare(MakeRecordingSession(numInputs, numMonoInputs, sampleRate), m_recordingDirectory);
    }

    void ShutdownRecording()
    {
        m_recorder->Shutdown();
        m_recordingFrameActive = false;
    }

    bool IsRecording() const
    {
        return m_recorder->IsRecording();
    }

    StreamingRecorder::State GetRecordingState() const
    {
        return m_recorder->GetState();
    }

    StreamingRecorder::Error GetRecordingError() const
    {
        return m_recorder->GetError();
    }

    bool StartRecording(size_t numInputs, uint32_t sampleRate, JSON initialPatch = {})
    {
        if (numInputs != m_recordingNumInputs || sampleRate != m_recorder->m_session.m_sampleRate)
        {
            m_recorder->Fail(StreamingRecorder::Error::InvalidConfiguration);
            return false;
        }

        return m_recorder->Start(initialPatch);
    }

    void StopRecording()
    {
        m_recorder->Stop();
    }

    bool RecordingLayoutMatches(const Input& input) const
    {
        return input.m_numInputs == m_recordingNumInputs
            && input.m_numMonoInputs == m_recordingNumMonoInputs;
    }

    void ProcessInputs(const Input& input)
    {
        if (IsRecording() && !RecordingLayoutMatches(input))
        {
            m_recorder->Fail(StreamingRecorder::Error::InvalidConfiguration);
        }

        m_recordingFrameActive = m_recorder->BeginFrame();
        m_output.m_output = QuadFloat();
        m_quadToStereoMixdown.Clear();
        for (size_t i = 0; i < x_numSends; ++i)
        {
            m_send[i] = QuadFloat();
        }
            
        if (input.m_noiseMode)
        {
            float pink = m_pinkNoise.Generate();
            m_output.m_output += QuadFloat(pink, pink, pink, pink);
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                m_output.m_output += QuadFloat(input.m_input[i], input.m_input[i], input.m_input[i], input.m_input[i]) * input.m_gain[i].m_expParam;
                m_quadToStereoMixdown.MixSample(0.5, 0.5, input.m_input[i] * input.m_gain[i].m_expParam);
            }
        }
        else
        {
            for (size_t i = 0; i < input.m_numInputs; ++i)
            {
                if (input.m_monitor[i])
                {
                    m_quadToStereoMixdown.MixSample(input.m_x[i], input.m_y[i], input.m_input[i] * input.m_gain[i].m_expParam);
                    m_quadToStereoMixdown.MixSample(0.5f, 0.5f, input.m_monoIn[i]);
                }

                QuadFloat pan = QuadFloat::Pan(input.m_x[i], input.m_y[i], input.m_input[i]);

                for (size_t j = 0; j < x_numSends; ++j)
                {
                    m_send[j] += pan * input.m_sendGain[i][j].m_expParam;
                }

                float reduction;
                m_voiceMeters[i].ProcessAndSaturate(input.m_input[i]* input.m_gain[i].m_expParam + input.m_monoIn[i], &reduction);

                QuadFloat mono = QuadFloat::Pan(0.5f, 0.5f, input.m_monoIn[i]);                
                QuadFloat postFader = (pan * input.m_gain[i].m_expParam + mono) * reduction;

                if (input.m_monitor[i])
                {
                    m_output.m_output += postFader;
                }

                if (m_recordingFrameActive)
                {
                    const float panned[] =
                    {
                        input.m_input[i] * input.m_gain[i].m_expParam * reduction,
                        input.m_x[i], input.m_y[i]
                    };


                    m_recorder->Submit(i, panned, 3);
                    if (i < input.m_numMonoInputs)
                    {
                        const float monoSample = input.m_monoIn[i] * reduction;
                        m_recorder->Submit(m_recordingNumInputs + i, &monoSample, 1);
                    }
                }
            }
        }

        ProcessReturnSends(input);
    }

    QuadFloatWithStereoAndSub ProcessReturns(const Input& input)
    {
        if (m_recordingFrameActive && !RecordingLayoutMatches(input))
        {
            m_recorder->Fail(StreamingRecorder::Error::InvalidConfiguration);
            m_recordingFrameActive = false;
        }

        if (!input.m_noiseMode)
        {
            for (size_t j = 0; j < x_numSends; ++j)
            {
                m_quadToStereoMixdown.MixQuadSample(input.m_return[j] * input.m_returnGain[j].m_expParam);
                QuadFloat postReturn = input.m_return[j] * input.m_returnGain[j].m_expParam;
                postReturn = m_returnMeters[j].ProcessAndSaturate(postReturn);
                m_output.m_output += postReturn;            

                if (m_recordingFrameActive)
                {
                    m_recorder->Submit(m_recordingNumInputs + m_recordingNumMonoInputs + j, postReturn.m_values, 4);
                }
            }
        }

        m_output = m_masterChain.Process(input.m_masterChainInput, m_output.m_output, m_quadToStereoMixdown.m_output);
        
        if (m_recordingFrameActive)
        {
            const size_t masterTrack = m_recordingNumInputs + m_recordingNumMonoInputs + x_numSends;
            m_recorder->Submit(masterTrack, m_output.m_output.m_values, 4);
            m_recorder->Submit(masterTrack + 1, m_output.m_stereoOutput.m_values, 2);
            m_recorder->CommitFrame();
        }
        
        m_masterMeter.Process(m_output.m_output);
        m_stereoMeter.Process(m_output.m_stereoOutput);

        return m_output;
    }

    QuadFloatWithStereoAndSub Process(const Input& input)
    {
        ProcessInputs(input);
        return ProcessReturns(input);
    }
};
