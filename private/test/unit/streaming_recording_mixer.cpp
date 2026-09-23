#include "doctest.h"
#include "support/GlobalEnv.hpp"
#include "QuadMixer.hpp"
#include "SmartGridBuildInfo.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
    struct MixerRecordingTest
    {
        std::filesystem::path m_directory;
        SmartGridOneContext m_context;
        QuadMixerInternal m_mixer;
        QuadMixerInternal::Input m_input;

        MixerRecordingTest(size_t inputs = 1, size_t monoInputs = 1)
            : m_mixer(&m_context)
        {
            static std::atomic<size_t> s_sequence = 0;
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            m_directory = std::filesystem::temp_directory_path()
                / ("smartgrid-mixer-" + std::to_string(stamp) + "-" + std::to_string(s_sequence.fetch_add(1)));
            std::filesystem::create_directories(m_directory);
            m_input.m_numInputs = inputs;
            m_input.m_numMonoInputs = monoInputs;
            m_mixer.m_recordingDirectory = m_directory.string();
            DOCTEST_REQUIRE(m_mixer.PrepareRecording(inputs, monoInputs, 48000));
        }

        ~MixerRecordingTest()
        {
            m_mixer.ShutdownRecording();
            std::filesystem::remove_all(m_directory);
        }

        void Start()
        {
            DOCTEST_REQUIRE(m_mixer.StartRecording(m_input.m_numInputs, 48000));
            m_mixer.Process(m_input);
            DOCTEST_REQUIRE(m_mixer.m_recordingFrameActive);
        }

        float Sample(size_t track, size_t stream = 0) const
        {
            return m_context.m_recorder.m_staging[m_context.m_recorder.m_trackOffsets[track] + stream];
        }
    };
}

DOCTEST_TEST_CASE("recording mixer: current layout declares typed lanes and direct masters")
{
    const auto session = QuadMixerInternal::MakeRecordingSession(17, 9, 48000);
    DOCTEST_REQUIRE(RecordingFormat::Validate(session));
    DOCTEST_CHECK(session.m_sampleRate == 48000);
    DOCTEST_CHECK(session.m_blockFrames == 48000);
    DOCTEST_CHECK(session.m_gitCommitSha == SmartGridBuildInfo::x_gitCommitSha);
    DOCTEST_REQUIRE(session.m_tracks.size() == 31);
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
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(QuadMixerInternal::MakeRecordingSession(33, 9, 48000)));
    DOCTEST_CHECK_FALSE(RecordingFormat::Validate(QuadMixerInternal::MakeRecordingSession(1, 2, 48000)));
}

DOCTEST_TEST_CASE("recording mixer: separate input and mono taps share reduction and ignore monitor")
{
    MixerRecordingTest fixture;
    auto& input = fixture.m_input;
    input.m_input[0] = 0.8f;
    input.m_gain[0].m_expParam = 0.75f;
    input.m_monoIn[0] = 0.2f;
    input.m_x[0] = 0.2f;
    input.m_y[0] = 0.8f;
    input.m_monitor[0] = false;
    fixture.Start();

    Meter meter;
    float reduction = 0;
    meter.ProcessAndSaturate(0.8f * 0.75f + 0.2f, &reduction);
    DOCTEST_CHECK(fixture.Sample(0) == doctest::Approx(0.8f * 0.75f * reduction));
    DOCTEST_CHECK(fixture.Sample(0, 1) == 0.2f);
    DOCTEST_CHECK(fixture.Sample(0, 2) == 0.8f);
    DOCTEST_CHECK(fixture.Sample(1) == doctest::Approx(0.2f * reduction));
    DOCTEST_CHECK(fixture.m_mixer.m_output.m_output == QuadFloat());
    DOCTEST_CHECK(fixture.m_mixer.m_output.m_stereoOutput == StereoFloat());
}

DOCTEST_TEST_CASE("recording mixer: returns retain corner order and masters are actual independent outputs")
{
    MixerRecordingTest fixture;
    auto& input = fixture.m_input;
    input.m_input[0] = 0.7f;
    input.m_gain[0].m_expParam = 0.8f;
    input.m_x[0] = 0.1f;
    input.m_y[0] = 0.9f;
    input.m_return[0] = QuadFloat(0.1f, 0.2f, 0.3f, 0.4f);
    input.m_returnGain[0].m_expParam = 0.5f;
    fixture.Start();

    QuadMeter expectedMeter;
    const QuadFloat expectedReturn = expectedMeter.ProcessAndSaturate(input.m_return[0] * 0.5f);
    for (size_t i = 0; i < 4; ++i)
    {
        DOCTEST_CHECK(fixture.Sample(2, i) == expectedReturn[i]);
        DOCTEST_CHECK(fixture.Sample(5, i) == fixture.m_mixer.m_output.m_output[i]);
    }

    for (size_t i = 0; i < 2; ++i)
    {
        DOCTEST_CHECK(fixture.Sample(6, i) == fixture.m_mixer.m_output.m_stereoOutput[i]);
    }

    DOCTEST_CHECK(std::abs(fixture.Sample(5)) > 0.0001f);
    DOCTEST_CHECK(std::abs(fixture.Sample(6)) > 0.0001f);
    QuadToStereoMixdown reconstructed;
    reconstructed.MixQuadSample(fixture.m_mixer.m_output.m_output);
    DOCTEST_CHECK(std::abs(reconstructed.m_output[0] - fixture.Sample(6)) > 0.0001f);
}

DOCTEST_TEST_CASE("recording mixer: noise mode zeroes skipped lanes and split processing commits masters")
{
    MixerRecordingTest fixture;
    fixture.m_input.m_input[0] = 0.5f;
    fixture.m_input.m_gain[0].m_expParam = 1.0f;
    fixture.m_input.m_monoIn[0] = 0.2f;
    fixture.m_input.m_x[0] = 0.9f;
    fixture.m_input.m_y[0] = 0.7f;
    fixture.m_input.m_return[0] = QuadFloat(0.1f, 0.2f, 0.3f, 0.4f);
    fixture.m_input.m_returnGain[0].m_expParam = 1.0f;
    fixture.Start();
    DOCTEST_REQUIRE(fixture.Sample(0) != 0);
    fixture.m_input.m_noiseMode = true;
    fixture.m_mixer.ProcessInputs(fixture.m_input);
    fixture.m_mixer.ProcessReturns(fixture.m_input);
    DOCTEST_REQUIRE(fixture.m_mixer.m_recordingFrameActive);
    for (size_t i = 0; i < fixture.m_context.m_recorder.m_trackOffsets[5]; ++i)
    {
        DOCTEST_CHECK(fixture.m_context.m_recorder.m_staging[i] == 0);
    }

    DOCTEST_CHECK(fixture.Sample(5) == fixture.m_mixer.m_output.m_output[0]);
    DOCTEST_CHECK(fixture.Sample(6) == fixture.m_mixer.m_output.m_stereoOutput[0]);
    fixture.m_input.m_noiseMode = false;
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_CHECK(fixture.Sample(0) > 0);
    DOCTEST_CHECK(fixture.Sample(0, 1) == 0.9f);
}

DOCTEST_TEST_CASE("recording mixer: structural layout changes stop capture with an error")
{
    MixerRecordingTest fixture;
    fixture.Start();
    fixture.m_input.m_numMonoInputs = 0;
    fixture.m_mixer.Process(fixture.m_input);
    DOCTEST_CHECK_FALSE(fixture.m_mixer.m_recordingFrameActive);
    DOCTEST_CHECK(fixture.m_mixer.GetRecordingError() == StreamingRecorder::Error::InvalidConfiguration);
}

DOCTEST_TEST_CASE("recording mixer: capture leaves live DSP unchanged and follows mastering controls")
{
    MixerRecordingTest fixture;
    fixture.Start();
    SmartGridOneContext referenceContext;
    QuadMixerInternal reference(&referenceContext);
    auto& input = fixture.m_input;
    input.m_gain[0].m_expParam = 0.8f;
    input.m_returnGain[0].m_expParam = 0.3f;
    for (size_t frame = 0; frame < 128; ++frame)
    {
        input.m_input[0] = std::sin(static_cast<float>(frame) * 0.3f);
        input.m_monoIn[0] = 0.2f;
        input.m_x[0] = static_cast<float>(frame) / 128.0f;
        input.m_y[0] = 0.6f;
        input.m_monitor[0] = frame % 3 != 0;
        input.m_return[0] = QuadFloat(0.1f, -0.1f, 0.2f, -0.2f);
        input.m_masterChainInput.SetMasterGain(frame < 64 ? 0.4f : 0.9f);
        const auto actual = fixture.m_mixer.Process(input);
        const auto expected = reference.Process(input);
        DOCTEST_CHECK(actual.m_output == expected.m_output);
        DOCTEST_CHECK(actual.m_stereoOutput == expected.m_stereoOutput);
        DOCTEST_CHECK(actual.m_sub == expected.m_sub);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CHECK(fixture.Sample(5, channel) == expected.m_output[channel]);
        }

        for (size_t channel = 0; channel < 2; ++channel)
        {
            DOCTEST_CHECK(fixture.Sample(6, channel) == expected.m_stereoOutput[channel]);
        }
    }
}

DOCTEST_TEST_CASE("recording mixer: writes full-layout fixture from accepted capture frames")
{
    MixerRecordingTest fixture(17, 9);
    fixture.Start();
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
        for (size_t track = 0; track < 17; ++track)
        {
            input.m_input[track] = std::sin(static_cast<float>(frame + track + 1) * 0.17f) * 0.4f;
            input.m_gain[track].m_expParam = 0.7f;
            input.m_x[track] = static_cast<float>((frame + track) % 65) / 64.0f;
            input.m_y[track] = 0.75f;
            input.m_monitor[track] = track % 3 != 0;
            input.m_monoIn[track] = track < 9 ? 0.01f * static_cast<float>(track + 1) : 0.0f;
        }

        for (size_t track = 0; track < 3; ++track)
        {
            input.m_return[track] = QuadFloat(0.1f, -0.2f, 0.3f, -0.4f) * (static_cast<float>(track) + 1.0f);
            input.m_returnGain[track].m_expParam = 0.3f;
        }

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
