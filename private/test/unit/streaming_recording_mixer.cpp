#include "doctest.h"
#include "support/GlobalEnv.hpp"
#include "QuadMixer.hpp"
#include "Saturator.hpp"
#include "SmartGridBuildInfo.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
    constexpr float x_recordingGain = 0.5f;
    constexpr size_t x_subTrack = QuadMixerInternal::x_firstSubRecordingTrack;
    constexpr size_t x_externalTrack = QuadMixerInternal::x_firstExternalRecordingTrack;
    constexpr size_t x_returnTrack = QuadMixerInternal::x_firstReturnRecordingTrack;
    constexpr size_t x_masterQuadTrack = QuadMixerInternal::x_masterQuadRecordingTrack;
    constexpr size_t x_masterStereoTrack = QuadMixerInternal::x_masterStereoRecordingTrack;

    struct MixerRecordingTest
    {
        std::filesystem::path m_directory;
        SmartGridOneContext m_context;
        QuadMixerInternal m_mixer;
        QuadMixerInternal::Input m_input;

        MixerRecordingTest()
            : m_mixer(&m_context)
        {
            static std::atomic<size_t> s_sequence = 0;
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            m_directory = std::filesystem::temp_directory_path()
                / ("smartgrid-mixer-" + std::to_string(stamp) + "-" + std::to_string(s_sequence.fetch_add(1)));
            std::filesystem::create_directories(m_directory);
            m_mixer.m_recordingDirectory = m_directory.string();
            DOCTEST_REQUIRE(m_mixer.PrepareRecording(48000));
        }

        ~MixerRecordingTest()
        {
            m_mixer.ShutdownRecording();
            std::filesystem::remove_all(m_directory);
        }

        // Host frames of constant input that let the recording decimators settle.
        //
        static constexpr size_t x_settleHostFrames = 64;

        void Start()
        {
            DOCTEST_REQUIRE(m_mixer.StartRecording(48000, JSON()));
            PrepareFrame(m_input, x_settleHostFrames);
            m_mixer.Process(m_input);
            DOCTEST_REQUIRE(m_mixer.m_recordingFrameActive);
        }

        // Runs settleHostFrames host frames of constant input, then stops so the
        // caller's next mixer call is the recording sample of a host frame.
        //
        void PrepareFrame(const QuadMixerInternal::Input& input, size_t settleHostFrames)
        {
            for (size_t i = 0; i < settleHostFrames * QuadMixerInternal::x_oversample; ++i)
            {
                m_mixer.Process(input);
            }

            while (m_mixer.m_recordingSubSample + 1 != QuadMixerInternal::x_oversample)
            {
                m_mixer.Process(input);
            }
        }

        void Settle(const QuadMixerInternal::Input& input)
        {
            PrepareFrame(input, x_settleHostFrames);
        }

        float Sample(size_t track, size_t stream = 0) const
        {
            return m_context.m_recorder.m_staging[m_context.m_recorder.m_trackOffsets[track] + stream];
        }
    };

    void CheckQuadApprox(const QuadFloat& actual, const QuadFloat& expected)
    {
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CHECK(actual[channel] == doctest::Approx(expected[channel]));
        }
    }
}

DOCTEST_TEST_CASE("recording mixer: fixed layout declares typed lanes and direct masters")
{
    const auto session = QuadMixerInternal::MakeRecordingSession(48000);
    DOCTEST_REQUIRE(RecordingFormat::Validate(session));
    DOCTEST_CHECK(session.m_sampleRate == 48000);
    DOCTEST_CHECK(session.m_blockFrames == 48000);
    DOCTEST_CHECK(session.m_gitCommitSha == SmartGridBuildInfo::x_gitCommitSha);
    DOCTEST_REQUIRE(session.m_tracks.size() == 31);
    DOCTEST_CHECK(QuadMixerInternal::x_numRecordingTracks == 31);
    DOCTEST_CHECK(x_externalTrack == 9);
    DOCTEST_CHECK(x_subTrack == 17);
    DOCTEST_CHECK(x_returnTrack == 26);
    DOCTEST_CHECK(x_masterQuadTrack == 29);
    DOCTEST_CHECK(session.StreamCount() == 78);
    for (size_t i = 0; i < 31; ++i)
    {
        DOCTEST_CHECK(session.m_tracks[i].m_id == i);
        const auto type = i < 17 ? RecordingFormat::TrackType::PannedMono
            : i < 26 ? RecordingFormat::TrackType::Mono
            : i < 30 ? RecordingFormat::TrackType::Quad
            : RecordingFormat::TrackType::Stereo;
        DOCTEST_CHECK(session.m_tracks[i].m_type == type);
    }

    DOCTEST_CHECK(session.m_tracks[29].m_role == "master_quad");
    DOCTEST_CHECK(session.m_tracks[30].m_role == "master_stereo");
    DOCTEST_CHECK(session.m_tracks[29].m_tap == "post_mastering_pre_master_volume");
    DOCTEST_CHECK(session.m_tracks[30].m_tap == "post_mastering_pre_master_volume");
    DOCTEST_CHECK(session.m_tracks[0].m_tap == "post_fader_post_saturation");
    DOCTEST_CHECK(session.m_tracks[17].m_tap == "post_saturation");
}

DOCTEST_TEST_CASE("recording mixer: track and sub taps saturate independently")
{
    MixerRecordingTest fixture;
    auto& track = fixture.m_input.m_tracks[1];
    track.m_input[2] = 0.8f;
    track.m_gain.m_expParam[2] = 0.75f;
    track.m_sub[2] = 0.2f;
    track.m_x[2] = 0.2f;
    track.m_y[2] = 0.8f;
    fixture.Start();

    const size_t voice = QuadMixerInternal::x_voicesPerTrack + 2;
    DOCTEST_CHECK(fixture.Sample(voice) == doctest::Approx(TanhSaturator<false>::Tanh(0.8f * 0.75f) * x_recordingGain));
    DOCTEST_CHECK(fixture.Sample(voice, 1) == doctest::Approx(0.2f));
    DOCTEST_CHECK(fixture.Sample(voice, 2) == doctest::Approx(0.8f));
    DOCTEST_CHECK(fixture.Sample(x_subTrack + voice) == doctest::Approx(TanhSaturator<false>::Tanh(0.2f) * x_recordingGain));
    for (size_t other = 0; other < QuadMixerInternal::x_numVoices; ++other)
    {
        if (other != voice)
        {
            DOCTEST_CHECK(fixture.Sample(other) == 0.0f);
            DOCTEST_CHECK(fixture.Sample(x_subTrack + other) == 0.0f);
        }
    }
}

DOCTEST_TEST_CASE("recording mixer: unmonitored external input still records")
{
    MixerRecordingTest fixture;
    auto& external = fixture.m_input.m_externals[2];
    external.m_input[1] = 0.5f;
    external.m_x[1] = 0.3f;
    external.m_y[1] = 0.4f;
    external.m_monitor = false;
    fixture.Start();

    const size_t lane = x_externalTrack + 2 * QuadMixerInternal::x_externalInputLanes + 1;
    DOCTEST_CHECK(fixture.Sample(lane) == doctest::Approx(TanhSaturator<false>::Tanh(0.5f) * x_recordingGain));
    DOCTEST_CHECK(fixture.Sample(lane, 1) == doctest::Approx(0.3f));
    DOCTEST_CHECK(fixture.Sample(lane, 2) == doctest::Approx(0.4f));
    DOCTEST_CHECK(fixture.m_mixer.m_output.m_output == QuadFloat());
    DOCTEST_CHECK(fixture.m_mixer.m_output.m_stereoOutput == StereoFloat());
}

DOCTEST_TEST_CASE("recording mixer: raw sends bypass gain and saturation while gain drives the dry saturator")
{
    MixerRecordingTest fixture;
    auto& track = fixture.m_input.m_tracks[2];
    track.m_input[1] = 4.0f;
    track.m_gain.m_expParam[1] = 0.5f;
    track.m_x[1] = 0.3f;
    track.m_y[1] = 0.6f;
    const float sendGains[QuadMixerInternal::x_numSends] = {0.25f, 0.5f, 1.0f};
    for (size_t send = 0; send < QuadMixerInternal::x_numSends; ++send)
    {
        track.m_sendGain[send].m_expParam[1] = sendGains[send];
    }

    fixture.Start();
    fixture.m_mixer.ProcessInputs(fixture.m_input);
    const QuadFloat panned = QuadFloat::Pan(0.3f, 0.6f, 4.0f);
    for (size_t send = 0; send < QuadMixerInternal::x_numSends; ++send)
    {
        CheckQuadApprox(fixture.m_mixer.m_send[send], panned * sendGains[send]);
    }

    const float saturated = TanhSaturator<false>::Tanh(4.0f * 0.5f);
    CheckQuadApprox(fixture.m_mixer.m_dryOutput, QuadFloat::Pan(0.3f, 0.6f, saturated));
    DOCTEST_CHECK(fixture.m_mixer.m_trackMeters[2].m_peak[1] == doctest::Approx(saturated));
    DOCTEST_CHECK(fixture.Sample(QuadMixerInternal::x_voicesPerTrack * 2 + 1) == doctest::Approx(saturated * x_recordingGain));
    fixture.m_mixer.ProcessReturns(fixture.m_input);

    track.m_gain.m_expParam[1] = 0.0f;
    fixture.Settle(fixture.m_input);
    fixture.m_mixer.ProcessInputs(fixture.m_input);
    for (size_t send = 0; send < QuadMixerInternal::x_numSends; ++send)
    {
        CheckQuadApprox(fixture.m_mixer.m_send[send], panned * sendGains[send]);
    }

    DOCTEST_CHECK(fixture.m_mixer.m_dryOutput == QuadFloat());
}

DOCTEST_TEST_CASE("recording mixer: zero voice gain silences its meter and recording without limiting sends")
{
    MixerRecordingTest fixture;
    auto& track = fixture.m_input.m_tracks[0];
    track.m_input[0] = 4.0f;
    track.m_gain.m_expParam[0] = 0.0f;
    track.m_sendGain[0].m_expParam[0] = 1.0f;
    track.m_x[0] = 0.5f;
    track.m_y[0] = 0.5f;
    fixture.Start();

    DOCTEST_CHECK(fixture.m_mixer.m_trackMeters[0].m_rms[0] == 0.0f);
    DOCTEST_CHECK(fixture.m_mixer.m_trackMeters[0].m_peak[0] == 0.0f);
    DOCTEST_CHECK(fixture.m_mixer.m_dryOutput == QuadFloat());
    DOCTEST_CHECK(fixture.Sample(0) == 0.0f);
    CheckQuadApprox(fixture.m_mixer.m_send[0], QuadFloat::Pan(0.5f, 0.5f, 4.0f));
}

DOCTEST_TEST_CASE("recording mixer: every audio track has headroom for saturated square wave decimation")
{
    MixerRecordingTest fixture;
    auto& input = fixture.m_input;
    for (auto& track : input.m_tracks)
    {
        track.m_gain.m_expParam = TrioFloat::Splat(1.0f);
        track.m_x = TrioFloat::Splat(0.5f);
        track.m_y = TrioFloat::Splat(0.5f);
    }

    for (auto& external : input.m_externals)
    {
        external.m_x = StereoFloat::Splat(0.5f);
        external.m_y = StereoFloat::Splat(0.5f);
    }

    for (auto& gain : input.m_returnGain)
    {
        gain.m_expParam = 1.0f;
    }

    fixture.Start();
    float peaks[QuadMixerInternal::x_numRecordingTracks] = {};
    for (size_t sample = 0; sample < 512 * QuadMixerInternal::x_oversample; ++sample)
    {
        const float raw = sample % (SampleTimer::x_sampleRate / 1000) < SampleTimer::x_sampleRate / 2000 ? 4.0f : -4.0f;
        for (auto& track : input.m_tracks)
        {
            track.m_input = TrioFloat::Splat(raw);
            track.m_sub = TrioFloat::Splat(raw);
        }

        for (auto& external : input.m_externals)
        {
            external.m_input = StereoFloat::Splat(raw);
        }

        for (auto& returned : input.m_return)
        {
            returned = QuadFloat::Splat(raw);
        }

        fixture.m_mixer.Process(input);
        if (fixture.m_mixer.m_recordingFrameActive)
        {
            for (size_t track = 0; track < QuadMixerInternal::x_numRecordingTracks; ++track)
            {
                const auto type = fixture.m_context.m_recorder.m_session.m_tracks[track].m_type;
                const size_t channels = type == RecordingFormat::TrackType::PannedMono ? 1 : RecordingFormat::StreamCount(type);
                for (size_t channel = 0; channel < channels; ++channel)
                {
                    const float value = fixture.Sample(track, channel);
                    peaks[track] = std::max(peaks[track], std::abs(value));
                    DOCTEST_CHECK(std::abs(RecordingFormat::Quantize(value)) < 8388607);
                }
            }
        }
    }

    for (float peak : peaks)
    {
        DOCTEST_CHECK(peak > 0.4f);
        DOCTEST_CHECK(peak < 1.0f);
    }

    // These saturated lanes overshoot unity before recording attenuation.
    //
    DOCTEST_CHECK(peaks[0] > 0.5f);
    DOCTEST_CHECK(peaks[x_externalTrack] > 0.5f);
    DOCTEST_CHECK(peaks[x_subTrack] > 0.5f);
    DOCTEST_CHECK(peaks[x_returnTrack] > 0.5f);
    DOCTEST_CHECK(fixture.Sample(0, 1) == doctest::Approx(0.5f));
    DOCTEST_CHECK(fixture.Sample(0, 2) == doctest::Approx(0.5f));
}

DOCTEST_TEST_CASE("recording mixer: sub saturation cannot duck the voice and bypasses gain and sends")
{
    MixerRecordingTest fixture;
    auto& track = fixture.m_input.m_tracks[0];
    track.m_input[0] = 4.0f;
    track.m_gain.m_expParam[0] = 1.0f;
    track.m_x[0] = 0.2f;
    track.m_y[0] = 0.8f;
    for (size_t send = 0; send < QuadMixerInternal::x_numSends; ++send)
    {
        track.m_sendGain[send].m_expParam[0] = 0.5f;
    }

    fixture.Start();
    const float voice = fixture.Sample(0);
    const QuadFloat send = fixture.m_mixer.m_send[0];
    track.m_sub[0] = -4.0f;
    fixture.Settle(fixture.m_input);
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_CHECK(fixture.Sample(0) == doctest::Approx(voice));
    const float sub = fixture.Sample(x_subTrack);
    DOCTEST_CHECK(sub == doctest::Approx(TanhSaturator<false>::Tanh(-4.0f) * x_recordingGain));
    for (size_t i = 0; i < QuadMixerInternal::x_numSends; ++i)
    {
        DOCTEST_CHECK(fixture.m_mixer.m_send[i] == send);
    }

    track.m_gain.m_expParam[0] = 0.0f;
    fixture.Settle(fixture.m_input);
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_CHECK(fixture.Sample(0) == doctest::Approx(0.0f));
    DOCTEST_CHECK(fixture.Sample(x_subTrack) == doctest::Approx(sub));
    track.m_input[0] = 0.0f;
    fixture.Settle(fixture.m_input);
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_CHECK(fixture.Sample(x_subTrack) == doctest::Approx(sub));
    for (const auto& output : fixture.m_mixer.m_send)
    {
        DOCTEST_CHECK(output == QuadFloat());
    }
}

DOCTEST_TEST_CASE("recording mixer: tracks feed the dry bus, subs the sub bus, and the external monitor gates both live buses")
{
    MixerRecordingTest fixture;
    auto& input = fixture.m_input;
    auto& track = input.m_tracks[0];
    track.m_input[0] = 0.8f;
    track.m_sub[0] = 0.3f;
    track.m_gain.m_expParam[0] = 0.5f;
    track.m_x[0] = 0.1f;
    track.m_y[0] = 0.9f;
    auto& external = input.m_externals[0];
    external.m_input[1] = 0.6f;
    external.m_x[1] = 0.7f;
    external.m_y[1] = 0.2f;
    fixture.Start();

    for (bool monitor : {false, true})
    {
        external.m_monitor = monitor;
        fixture.Settle(input);
        fixture.m_mixer.ProcessInputs(input);
        const float voice = TanhSaturator<false>::Tanh(0.8f * 0.5f);
        const float lane = monitor ? TanhSaturator<false>::Tanh(0.6f) : 0.0f;
        const float sub = TanhSaturator<false>::Tanh(0.3f);
        const QuadFloat expectedDry = QuadFloat::Pan(0.1f, 0.9f, voice) + QuadFloat::Pan(0.7f, 0.2f, lane);
        const QuadFloat expectedSub = QuadFloat::Pan(0.5f, 0.5f, sub);
        QuadToStereoMixdown expectedStereo;
        expectedStereo.Clear();
        expectedStereo.MixSample(0.1f, 0.9f, voice);
        expectedStereo.MixSample(0.7f, 0.2f, lane);
        expectedStereo.MixSample(0.5f, 0.5f, sub);
        CheckQuadApprox(fixture.m_mixer.m_output.m_output, expectedDry + expectedSub);
        CheckQuadApprox(fixture.m_mixer.m_dryOutput, expectedDry);
        CheckQuadApprox(fixture.m_mixer.m_subOutput, expectedSub);
        for (size_t channel = 0; channel < 2; ++channel)
        {
            DOCTEST_CHECK(fixture.m_mixer.m_quadToStereoMixdown.m_output[channel] == doctest::Approx(expectedStereo.m_output[channel]));
        }

        DOCTEST_CHECK(fixture.Sample(0) == doctest::Approx(voice * x_recordingGain));
        DOCTEST_CHECK(fixture.Sample(x_externalTrack + 1) == doctest::Approx(TanhSaturator<false>::Tanh(0.6f) * x_recordingGain));
        DOCTEST_CHECK(fixture.Sample(x_subTrack) == doctest::Approx(sub * x_recordingGain));
        fixture.m_mixer.ProcessReturns(input);
    }
}

DOCTEST_TEST_CASE("recording mixer: returns retain corner order and masters are actual independent outputs")
{
    MixerRecordingTest fixture;
    auto& input = fixture.m_input;
    auto& track = input.m_tracks[0];
    track.m_input[0] = 0.7f;
    track.m_gain.m_expParam[0] = 0.8f;
    track.m_x[0] = 0.1f;
    track.m_y[0] = 0.9f;
    input.m_return[0] = QuadFloat(0.1f, 0.2f, 0.3f, 0.4f);
    input.m_returnGain[0].m_expParam = 0.5f;
    fixture.Start();

    QuadMeter expectedMeter;
    const QuadFloat expectedReturn = expectedMeter.ProcessAndSaturate(input.m_return[0] * 0.5f);
    for (size_t i = 0; i < 4; ++i)
    {
        DOCTEST_CHECK(fixture.Sample(x_returnTrack, i) == doctest::Approx(expectedReturn[i] * x_recordingGain));
        DOCTEST_CHECK(fixture.m_mixer.m_returnOutput[0][i] == expectedReturn[i]);
        DOCTEST_CHECK(fixture.Sample(x_masterQuadTrack, i) == fixture.m_mixer.m_output.m_output[i] * x_recordingGain);
    }

    for (size_t i = 0; i < 2; ++i)
    {
        DOCTEST_CHECK(fixture.Sample(x_masterStereoTrack, i) == fixture.m_mixer.m_output.m_stereoOutput[i] * x_recordingGain);
    }

    DOCTEST_CHECK(std::abs(fixture.Sample(x_masterQuadTrack)) > 0.0001f);
    DOCTEST_CHECK(std::abs(fixture.Sample(x_masterStereoTrack)) > 0.0001f);
    QuadToStereoMixdown reconstructed;
    reconstructed.MixQuadSample(fixture.m_mixer.m_output.m_output);
    DOCTEST_CHECK(std::abs(reconstructed.m_output[0] * x_recordingGain - fixture.Sample(x_masterStereoTrack)) > 0.0001f);
}

DOCTEST_TEST_CASE("recording mixer: noise mode zeroes skipped lanes and split processing commits masters")
{
    MixerRecordingTest fixture;
    auto& track = fixture.m_input.m_tracks[0];
    track.m_input[0] = 0.5f;
    track.m_gain.m_expParam[0] = 1.0f;
    track.m_sub[0] = 0.2f;
    track.m_x[0] = 0.9f;
    track.m_y[0] = 0.7f;
    fixture.m_input.m_return[0] = QuadFloat(0.1f, 0.2f, 0.3f, 0.4f);
    fixture.m_input.m_returnGain[0].m_expParam = 1.0f;
    fixture.Start();
    DOCTEST_REQUIRE(fixture.Sample(0) != 0);
    fixture.m_input.m_noiseMode = true;
    fixture.Settle(fixture.m_input);
    fixture.m_mixer.ProcessInputs(fixture.m_input);
    DOCTEST_CHECK(fixture.m_mixer.m_subOutput == QuadFloat());
    DOCTEST_CHECK(fixture.m_mixer.m_dryOutput == fixture.m_mixer.m_output.m_output);
    fixture.m_mixer.ProcessReturns(fixture.m_input);
    for (const auto& output : fixture.m_mixer.m_returnOutput)
    {
        DOCTEST_CHECK(output == QuadFloat());
    }

    DOCTEST_REQUIRE(fixture.m_mixer.m_recordingFrameActive);
    for (size_t i = 0; i < fixture.m_context.m_recorder.m_trackOffsets[x_masterQuadTrack]; ++i)
    {
        DOCTEST_CHECK(fixture.m_context.m_recorder.m_staging[i] == 0);
    }

    DOCTEST_CHECK(fixture.Sample(x_masterQuadTrack) == fixture.m_mixer.m_output.m_output[0] * x_recordingGain);
    DOCTEST_CHECK(fixture.Sample(x_masterStereoTrack) == fixture.m_mixer.m_output.m_stereoOutput[0] * x_recordingGain);
    fixture.m_input.m_noiseMode = false;
    fixture.Settle(fixture.m_input);
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_CHECK(fixture.Sample(0) > 0);
    DOCTEST_CHECK(fixture.Sample(0, 1) == doctest::Approx(0.9f));
}

DOCTEST_TEST_CASE("recording mixer: capture leaves live DSP unchanged and follows mastering controls")
{
    MixerRecordingTest fixture;
    fixture.Start();
    SmartGridOneContext referenceContext;
    QuadMixerInternal reference(&referenceContext);
    auto& input = fixture.m_input;
    auto& track = input.m_tracks[0];
    auto& external = input.m_externals[0];
    track.m_gain.m_expParam[0] = 0.8f;
    input.m_returnGain[0].m_expParam = 0.3f;
    for (size_t frame = 0; frame < 128; ++frame)
    {
        track.m_input[0] = std::sin(static_cast<float>(frame) * 0.3f);
        track.m_sub[0] = 0.2f;
        track.m_x[0] = static_cast<float>(frame) / 128.0f;
        track.m_y[0] = 0.6f;
        external.m_input[0] = std::sin(static_cast<float>(frame) * 0.2f);
        external.m_x[0] = 0.4f;
        external.m_monitor = frame % 3 != 0;
        input.m_return[0] = QuadFloat(0.1f, -0.1f, 0.2f, -0.2f);
        input.m_masterChainInput.SetMasterGain(frame < 64 ? 0.4f : 0.9f);
        QuadFloatWithStereoAndSub expected;
        for (size_t sub = 0; sub < QuadMixerInternal::x_oversample; ++sub)
        {
            const auto actual = fixture.m_mixer.Process(input);
            expected = reference.Process(input);
            DOCTEST_CHECK(actual.m_output == expected.m_output);
            DOCTEST_CHECK(actual.m_stereoOutput == expected.m_stereoOutput);
            DOCTEST_CHECK(actual.m_sub == expected.m_sub);
        }

        DOCTEST_REQUIRE(fixture.m_mixer.m_recordingFrameActive);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CHECK(fixture.Sample(x_masterQuadTrack, channel) == expected.m_output[channel] * x_recordingGain);
        }

        for (size_t channel = 0; channel < 2; ++channel)
        {
            DOCTEST_CHECK(fixture.Sample(x_masterStereoTrack, channel) == expected.m_stereoOutput[channel] * x_recordingGain);
        }
    }
}

DOCTEST_TEST_CASE("recording mixer: writes full-layout fixture from accepted capture frames")
{
    MixerRecordingTest fixture;
    // Start with silence without recording an unaccounted warm-up prefix.
    //
    DOCTEST_REQUIRE(fixture.m_mixer.StartRecording(48000, JSON()));
    fixture.PrepareFrame(fixture.m_input, 0);
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_REQUIRE(fixture.m_mixer.m_recordingFrameActive);
    StreamingRecorder& recorder = fixture.m_context.m_recorder;
    const auto session = recorder.m_session;
    std::vector<std::vector<int32_t>> frames;
    const auto capture = [&]()
    {
        std::vector<int32_t> frame;
        for (size_t track = 0; track < session.m_tracks.size(); ++track)
        {
            for (size_t stream = 0; stream < RecordingFormat::StreamCount(session.m_tracks[track].m_type); ++stream)
            {
                frame.push_back(RecordingFormat::Quantize(fixture.Sample(track, stream),
                    session.m_tracks[track].m_type == RecordingFormat::TrackType::PannedMono && stream != 0));
            }
        }

        frames.push_back(std::move(frame));
    };
    capture();
    for (size_t frame = 0; frame < 64; ++frame)
    {
        auto& input = fixture.m_input;
        input.m_noiseMode = frame >= 20 && frame < 24;
        auto setLane = [&](size_t index, float& signal, float& x, float& y)
        {
            signal = std::sin(static_cast<float>(frame + index + 1) * 0.17f) * 0.4f;
            x = static_cast<float>((frame + index) % 65) / 64.0f;
            y = 0.75f;
        };

        for (size_t track = 0; track < QuadMixerInternal::x_numTracks; ++track)
        {
            auto& mixerTrack = input.m_tracks[track];
            mixerTrack.m_gain.m_expParam = TrioFloat::Splat(0.7f);
            for (size_t lane = 0; lane < QuadMixerInternal::x_voicesPerTrack; ++lane)
            {
                size_t voice = track * QuadMixerInternal::x_voicesPerTrack + lane;
                setLane(voice, mixerTrack.m_input[lane], mixerTrack.m_x[lane], mixerTrack.m_y[lane]);
                mixerTrack.m_sub[lane] = 0.01f * static_cast<float>(voice + 1);
            }
        }

        for (size_t i = 0; i < QuadMixerInternal::x_numExternalInputs; ++i)
        {
            auto& external = input.m_externals[i];
            external.m_monitor = i % 2 != 0;
            for (size_t lane = 0; lane < QuadMixerInternal::x_externalInputLanes; ++lane)
            {
                setLane(x_externalTrack + i * QuadMixerInternal::x_externalInputLanes + lane,
                    external.m_input[lane], external.m_x[lane], external.m_y[lane]);
            }
        }

        for (size_t track = 0; track < 3; ++track)
        {
            input.m_return[track] = QuadFloat(0.1f, -0.2f, 0.3f, -0.4f) * (static_cast<float>(track) + 1.0f);
            input.m_returnGain[track].m_expParam = 0.3f;
        }

        fixture.PrepareFrame(input, 0);
        if (frame % 2 == 0)
        {
            fixture.m_mixer.Process(input);
        }
        else
        {
            fixture.m_mixer.ProcessInputs(input);
            fixture.m_mixer.ProcessReturns(input);
        }

        DOCTEST_REQUIRE(fixture.m_mixer.m_recordingFrameActive);
        capture();
    }

    fixture.m_mixer.StopRecording();
    fixture.m_mixer.ShutdownRecording();
    DOCTEST_CHECK(fixture.m_mixer.GetRecordingError() == StreamingRecorder::Error::None);
    std::vector<std::filesystem::path> paths;
    for (const auto& item : std::filesystem::directory_iterator(fixture.m_directory))
    {
        if (item.path().extension() == ".sgrec")
        {
            paths.push_back(item.path());
        }
    }

    DOCTEST_REQUIRE(paths.size() == 1);
    DOCTEST_CHECK(std::filesystem::file_size(paths[0]) > 16);
    const char* output = std::getenv("SMARTGRID_RECORDING_FIXTURE_OUTPUT");
    if (output == nullptr)
    {
        return;
    }

    std::filesystem::copy_file(paths[0], output, std::filesystem::copy_options::overwrite_existing);
    std::ofstream expected(std::string(output) + ".expected.json");
    expected << "{\"git_commit_sha\":\"" << session.m_gitCommitSha << "\",\"sample_rate\":48000,\"tracks\":[";
    for (size_t track = 0; track < session.m_tracks.size(); ++track)
    {
        expected << (track == 0 ? "" : ",") << "{\"id\":" << session.m_tracks[track].m_id
            << ",\"stream_count\":" << RecordingFormat::StreamCount(session.m_tracks[track].m_type) << "}";
    }

    expected << "],\"frames\":[";
    for (size_t frame = 0; frame < frames.size(); ++frame)
    {
        expected << (frame == 0 ? "[" : ",[");
        for (size_t stream = 0; stream < frames[frame].size(); ++stream)
        {
            expected << (stream == 0 ? "" : ",") << frames[frame][stream];
        }

        expected << "]";
    }

    expected << "]}\n";
    DOCTEST_REQUIRE(expected.good());
}
