#pragma once

#include "QuadUtils.hpp"
#include "DelayLine.hpp"
#include "SmartGridOneContext.hpp"
#include "Noise.hpp"
#include "QuadMasterChain.hpp"
#include "QuadToStereoMixdown.hpp"
#include "Metering.hpp"
#include "Oversample.hpp"
#include "TrioUtils.hpp"
#include "RecordingFormat.hpp"
#include "SmartGridBuildInfo.hpp"

// SquiggleBoy's mixer: three voice trios with their sub trios, four stereo
// external inputs, three effect sends and returns, and the mastering chains.
//
struct QuadMixerInternal
{
    static constexpr size_t x_numSends = 3;
    static constexpr size_t x_numTracks = 3;
    static constexpr size_t x_voicesPerTrack = 3;
    static constexpr size_t x_numVoices = x_numTracks * x_voicesPerTrack;
    static constexpr size_t x_numExternalInputs = 4;
    static constexpr size_t x_externalInputLanes = 2;
    static constexpr size_t x_oversample = SampleTimer::x_oversample;

    // Recording tracks: one panned track per voice, then per external lane,
    // then one mono track per sub, then the returns and the two masters.
    //
    static constexpr size_t x_firstExternalRecordingTrack = x_numVoices;
    static constexpr size_t x_firstSubRecordingTrack = x_firstExternalRecordingTrack + x_numExternalInputs * x_externalInputLanes;
    static constexpr size_t x_firstReturnRecordingTrack = x_firstSubRecordingTrack + x_numVoices;
    static constexpr size_t x_masterQuadRecordingTrack = x_firstReturnRecordingTrack + x_numSends;
    static constexpr size_t x_masterStereoRecordingTrack = x_masterQuadRecordingTrack + 1;
    static constexpr size_t x_numRecordingTracks = x_masterStereoRecordingTrack + 1;

    using TrackGain = PhaseUtils::ZeroedExpParamGeneric<float, x_voicesPerTrack>;

    // The mastered output and the recorded streams are decimated to the host
    // rate. Recording frames begin and commit once per host frame.
    //
    using PannedDecimator = DownsamplerGeneric<float, 3, 8, x_oversample>;
    using MonoDecimator = DownsamplerGeneric<float, 1, 8, x_oversample>;
    using TrioDecimator = DownsamplerGeneric<float, 3, 8, x_oversample>;
    using QuadDecimator = DownsamplerGeneric<float, 4, 8, x_oversample>;
    using StereoDecimator = DownsamplerGeneric<float, 2, 8, x_oversample>;

    struct OutputDecimator
    {
        QuadDecimator m_quad;
        StereoDecimator m_stereo;
        MonoDecimator m_sub;

        QuadFloatWithStereoAndSub Process(const QuadFloatWithStereoAndSub& input)
        {
            return QuadFloatWithStereoAndSub(
                m_quad.Process(input.m_output),
                m_stereo.Process(input.m_stereoOutput),
                m_sub.Process(MonoDecimator::Vector::Splat(input.m_sub))[0]);
        }
    };

    QuadFloatWithStereoAndSub m_output;
    QuadFloat m_dryOutput;
    QuadFloat m_subOutput;
    QuadFloat m_returnOutput[x_numSends];
    QuadFloat m_send[x_numSends];
    StreamingRecorder* m_recorder;
    std::string m_recordingDirectory;
    bool m_recordingFrameActive = false;
    size_t m_recordingSubSample = 0;
    PinkNoiseGeneric<1> m_pinkNoise;

    OutputDecimator m_outputDecimator;
    PannedDecimator m_pannedRecordingDecimators[x_firstSubRecordingTrack];
    TrioDecimator m_subRecordingDecimators[x_numTracks];
    QuadDecimator m_returnRecordingDecimators[x_numSends];

    TrioMeter m_trackMeters[x_numTracks];
    TrioMeter m_subMeters[x_numTracks];
    StereoMeter m_externalMeters[x_numExternalInputs];
    QuadMeter m_returnMeters[x_numSends];
    QuadMeter m_masterMeter;
    StereoMeter m_stereoMeter;

    DualMasteringChain m_masterChain;
    QuadToStereoMixdown m_quadToStereoMixdown;

    QuadMixerInternal(SmartGridOneContext* context)
        : m_recorder(&context->m_recorder)
        , m_pinkNoise(0)
    {
    }

    struct TrackInput
    {
        TrioFloat m_input;
        TrioFloat m_x;
        TrioFloat m_y;
        TrioFloat m_sub;
        TrackGain m_gain;
        TrackGain m_sendGain[x_numSends];
    };

    struct ExternalInput
    {
        StereoFloat m_input;
        StereoFloat m_x;
        StereoFloat m_y;
        bool m_monitor;

        ExternalInput()
            : m_monitor(true)
        {
        }
    };

    struct Input
    {
        TrackInput m_tracks[x_numTracks];
        ExternalInput m_externals[x_numExternalInputs];
        QuadFloat m_return[x_numSends];
        PhaseUtils::ZeroedExpParam m_returnGain[x_numSends];
        PhaseUtils::ZeroedExpParam m_returnSendGain[x_numSends][x_numSends];
        bool m_noiseMode;

        DualMasteringChain::Input m_masterChainInput;

        Input()
            : m_noiseMode(false)
        {
            for (size_t i = 0; i < x_numSends; ++i)
            {
                m_returnSendGain[i][i].m_expParam = 0.0;
            }
        }
    };

    // Saturates a group of panned lanes, then feeds the faded saturated signal to
    // bus and the stereo mixdown when monitored, and the unfaded saturated signal
    // to the sends when sendGains is set. Returns the faded signal for recording.
    //
    template <size_t Size>
    MultiChannelNumber<float, Size> Mix(
        const MultiChannelNumber<float, Size>& input,
        const MultiChannelNumber<float, Size>& gain,
        const MultiChannelNumber<float, Size>& x,
        const MultiChannelNumber<float, Size>& y,
        const PhaseUtils::ZeroedExpParamGeneric<float, Size>* sendGains,
        bool monitor,
        MeterGeneric<float, Size>& meter,
        QuadFloat& bus)
    {
        MultiChannelNumber<float, Size> saturated = meter.ProcessAndSaturate(input);
        MultiChannelNumber<float, Size> faded = saturated * gain;
        for (size_t i = 0; i < Size; ++i)
        {
            QuadFloat pan = QuadFloat::Pan(x[i], y[i], 1.0f);
            if (monitor)
            {
                bus += pan * faded[i];
                m_quadToStereoMixdown.MixSample(x[i], y[i], faded[i]);
            }

            if (sendGains != nullptr)
            {
                QuadFloat panned = pan * saturated[i];
                for (size_t j = 0; j < x_numSends; ++j)
                {
                    m_send[j] += panned * sendGains[j].m_expParam[i];
                }
            }
        }

        return faded;
    }

    // Noise mode replaces panning with an unsaturated center mix of each lane.
    //
    template <size_t Size>
    void MixNoise(const MultiChannelNumber<float, Size>& input)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            m_dryOutput += QuadFloat::Splat(input[i]);
            m_quadToStereoMixdown.MixSample(0.5f, 0.5f, input[i]);
        }
    }

    template <size_t Size>
    void RecordPanned(
        size_t firstTrack,
        const MultiChannelNumber<float, Size>& signal,
        const MultiChannelNumber<float, Size>& x,
        const MultiChannelNumber<float, Size>& y)
    {
        for (size_t i = 0; i < Size; ++i)
        {
            size_t track = firstTrack + i;
            TrioFloat panned = m_pannedRecordingDecimators[track].Process(TrioFloat(signal[i], x[i], y[i]));
            if (m_recordingFrameActive)
            {
                m_recorder->Submit(track, panned.m_values, 3);
            }
        }
    }

    void RecordSub(size_t track, const TrioFloat& signal)
    {
        TrioFloat sub = m_subRecordingDecimators[track].Process(signal);
        if (m_recordingFrameActive)
        {
            for (size_t i = 0; i < x_voicesPerTrack; ++i)
            {
                m_recorder->Submit(x_firstSubRecordingTrack + track * x_voicesPerTrack + i, &sub.m_values[i], 1);
            }
        }
    }

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

    static void AddTrack(RecordingFormat::Session& session, const std::string& name, RecordingFormat::TrackType type, const std::string& role, const std::string& tap)
    {
        session.m_tracks.push_back({static_cast<uint32_t>(session.m_tracks.size()), name, type, role, tap});
    }

    static RecordingFormat::Session MakeRecordingSession(uint32_t sampleRate)
    {
        RecordingFormat::Session session;
        session.m_sampleRate = sampleRate;
        session.m_blockFrames = sampleRate;
        session.m_gitCommitSha = SmartGridBuildInfo::x_gitCommitSha;
        session.m_parameters = {"global_phase_unmodulated"};
        for (size_t i = 0; i < x_firstSubRecordingTrack; ++i)
        {
            AddTrack(session, "input_" + std::to_string(i), RecordingFormat::TrackType::PannedMono, "input", "post_fader_post_saturation");
        }

        for (size_t i = 0; i < x_numVoices; ++i)
        {
            AddTrack(session, "mono_" + std::to_string(i), RecordingFormat::TrackType::Mono, "mono_input", "post_saturation");
        }

        for (size_t i = 0; i < x_numSends; ++i)
        {
            AddTrack(session, "return_" + std::to_string(i), RecordingFormat::TrackType::Quad, "return", "post_gain_post_saturation");
        }

        AddTrack(session, "master_quad", RecordingFormat::TrackType::Quad, "master_quad", "post_mastering_pre_master_volume");
        AddTrack(session, "master_stereo", RecordingFormat::TrackType::Stereo, "master_stereo", "post_mastering_pre_master_volume");
        return session;
    }

    bool PrepareRecording(uint32_t sampleRate)
    {
        m_recordingFrameActive = false;
        return m_recorder->Prepare(MakeRecordingSession(sampleRate), m_recordingDirectory);
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

    bool StartRecording(uint32_t sampleRate, JSON initialPatch)
    {
        if (sampleRate != m_recorder->m_session.m_sampleRate)
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

    void ProcessInputs(const Input& input)
    {
        ++m_recordingSubSample;
        bool hostFrame = m_recordingSubSample == x_oversample;
        if (hostFrame)
        {
            m_recordingSubSample = 0;
        }

        bool recording = IsRecording();
        m_recordingFrameActive = hostFrame && m_recorder->BeginFrame();
        m_dryOutput = QuadFloat();
        m_subOutput = QuadFloat();
        m_quadToStereoMixdown.Clear();
        for (size_t i = 0; i < x_numSends; ++i)
        {
            m_send[i] = QuadFloat();
        }

        if (input.m_noiseMode)
        {
            float pink = m_pinkNoise.Generate();
            m_dryOutput = QuadFloat::Splat(pink);
            for (const TrackInput& track : input.m_tracks)
            {
                MixNoise(track.m_input * track.m_gain.m_expParam);
            }

            for (const ExternalInput& external : input.m_externals)
            {
                MixNoise(external.m_input);
            }
        }
        else
        {
            for (size_t i = 0; i < x_numTracks; ++i)
            {
                const TrackInput& track = input.m_tracks[i];
                TrioFloat faded = Mix(track.m_input, track.m_gain.m_expParam, track.m_x, track.m_y, track.m_sendGain, true, m_trackMeters[i], m_dryOutput);
                if (recording)
                {
                    RecordPanned(i * x_voicesPerTrack, faded, track.m_x, track.m_y);
                }
            }

            for (size_t i = 0; i < x_numExternalInputs; ++i)
            {
                const ExternalInput& external = input.m_externals[i];
                StereoFloat saturated = Mix<x_externalInputLanes>(external.m_input, StereoFloat::Splat(1.0f), external.m_x, external.m_y, nullptr, external.m_monitor, m_externalMeters[i], m_dryOutput);
                if (recording)
                {
                    RecordPanned(x_firstExternalRecordingTrack + i * x_externalInputLanes, saturated, external.m_x, external.m_y);
                }
            }

            const TrioFloat unity = TrioFloat::Splat(1.0f);
            const TrioFloat center = TrioFloat::Splat(0.5f);
            for (size_t i = 0; i < x_numTracks; ++i)
            {
                TrioFloat saturated = Mix<x_voicesPerTrack>(input.m_tracks[i].m_sub, unity, center, center, nullptr, true, m_subMeters[i], m_subOutput);
                if (recording)
                {
                    RecordSub(i, saturated);
                }
            }
        }

        m_output.m_output = m_dryOutput + m_subOutput;
        ProcessReturnSends(input);
    }

    QuadFloatWithStereoAndSub ProcessReturns(const Input& input)
    {
        for (size_t j = 0; j < x_numSends; ++j)
        {
            QuadFloat& postReturn = m_returnOutput[j];
            postReturn = QuadFloat();
            if (!input.m_noiseMode)
            {
                postReturn = input.m_return[j] * input.m_returnGain[j].m_expParam;
                postReturn = m_returnMeters[j].ProcessAndSaturate(postReturn);
                m_quadToStereoMixdown.MixQuadSample(postReturn);
                m_output.m_output += postReturn;

                if (IsRecording())
                {
                    QuadFloat recorded = m_returnRecordingDecimators[j].Process(postReturn);
                    if (m_recordingFrameActive)
                    {
                        m_recorder->Submit(x_firstReturnRecordingTrack + j, recorded.m_values, 4);
                    }
                }
            }
        }

        m_output = m_outputDecimator.Process(
            m_masterChain.Process(input.m_masterChainInput, m_output.m_output, m_quadToStereoMixdown.m_output));

        if (m_recordingFrameActive)
        {
            m_recorder->Submit(x_masterQuadRecordingTrack, m_output.m_output.m_values, 4);
            m_recorder->Submit(x_masterStereoRecordingTrack, m_output.m_stereoOutput.m_values, 2);
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
