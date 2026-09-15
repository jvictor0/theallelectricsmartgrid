#include "doctest.h"
#include "support/SynthRig.hpp"
#include <filesystem>
#include <thread>

DOCTEST_TEST_CASE("recording engine: source widths stay stable and listening volume follows captured masters")
{
    struct Directory
    {
        std::filesystem::path m_path;

        Directory()
        {
            m_path = std::filesystem::temp_directory_path()
                / ("smartgrid-engine-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(m_path);
        }

        ~Directory()
        {
            std::filesystem::remove_all(m_path);
        }
    } directory;
    synthrig::SynthRig rig;
    rig.RunSamples(16);
    DOCTEST_REQUIRE(rig.PrepareRecording(directory.m_path.string()));
    auto& synth = rig.Internal().m_squiggleBoy;
    auto& engine = static_cast<SquiggleBoy&>(synth);
    auto& recorder = engine.m_mixer.m_recorder;
    const auto tracks = recorder.m_session.m_tracks;
    TheNonagonSquiggleBoyInternal::RecordCell recordCell(&rig.Internal());
    recordCell.OnPress(127);
    DOCTEST_REQUIRE(synth.IsRecording());
    synth.m_sourceMixerState.m_sources[0].m_gain.m_expParam = 1.0f;
    synth.m_sourceMixerState.m_sources[0].m_config.m_width = SourceMixer::SourceWidth::Mono;
    synth.m_masterVolume.m_expParam = 0.0f;
    AudioInputBuffer input;
    input.m_numInputs = SourceMixer::x_numPhysicalInputChannels;
    input.m_input[0] = 0.3f;
    input.m_input[1] = -0.2f;
    const bool monitors[SourceMixer::x_numSources] =
    {
        true, true, true, true
    };

    const auto process = [&]()
    {
        SampleTimer::IncrementSample();
        engine.ProcessSample(input, monitors);
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (recorder.GetState() != StreamingRecorder::State::Recording
        && std::chrono::steady_clock::now() < deadline)
    {
        process();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    DOCTEST_REQUIRE(recorder.GetState() == StreamingRecorder::State::Recording);
    for (size_t frame = 0; frame < 16; ++frame)
    {
        process();
    }

    const auto sample = [&](size_t track, size_t stream = 0)
    {
        return recorder.m_staging[recorder.m_trackOffsets[track] + stream];
    };
    DOCTEST_REQUIRE(std::abs(sample(29)) > 0.000001f);
    DOCTEST_REQUIRE(std::abs(sample(30)) > 0.000001f);
    DOCTEST_CHECK(engine.m_output.m_output == QuadFloat());
    DOCTEST_CHECK(engine.m_output.m_stereoOutput == StereoFloat());
    DOCTEST_CHECK(sample(9, 1) == 0.5f);
    DOCTEST_CHECK(sample(9, 2) == 0.5f);
    DOCTEST_CHECK(sample(10) == 0.0f);

    synth.m_masterVolume.m_expParam = 0.25f;
    synth.m_sourceMixerState.m_sources[0].m_config.m_width = SourceMixer::SourceWidth::Stereo;
    for (size_t frame = 0; frame < 16; ++frame)
    {
        process();
        for (size_t channel = 0; channel < 4; ++channel)
        {
            DOCTEST_CHECK(sample(29, channel) == engine.m_mixer.m_output.m_output[channel]);
            DOCTEST_CHECK(engine.m_output.m_output[channel] == sample(29, channel) * 0.25f);
        }

        for (size_t channel = 0; channel < 2; ++channel)
        {
            DOCTEST_CHECK(sample(30, channel) == engine.m_mixer.m_output.m_stereoOutput[channel]);
            DOCTEST_CHECK(engine.m_output.m_stereoOutput[channel] == sample(30, channel) * 0.25f);
        }
    }

    DOCTEST_CHECK(sample(9, 1) == 1.0f);
    DOCTEST_CHECK(sample(9, 2) == 0.0f);
    DOCTEST_CHECK(sample(10, 1) == 0.0f);
    DOCTEST_CHECK(sample(10, 2) == 1.0f);
    DOCTEST_CHECK(std::abs(sample(10)) > 0.000001f);
    synth.m_sourceMixerState.m_sources[0].m_config.m_width = SourceMixer::SourceWidth::Mono;
    for (size_t frame = 0; frame < 16; ++frame)
    {
        process();
    }

    Meter tailMeter;
    const float filteredTail = synth.m_sourceMixer.m_sources[0].m_output[1];
    DOCTEST_CHECK(sample(10) == doctest::Approx(tailMeter.ProcessAndSaturate(filteredTail)));
    DOCTEST_CHECK(sample(10, 1) == 0.5f);
    DOCTEST_CHECK(sample(10, 2) == 0.5f);
    DOCTEST_REQUIRE(recorder.m_session.m_tracks.size() == tracks.size());
    for (size_t track = 0; track < tracks.size(); ++track)
    {
        DOCTEST_CHECK(recorder.m_session.m_tracks[track].m_id == tracks[track].m_id);
        DOCTEST_CHECK(recorder.m_session.m_tracks[track].m_type == tracks[track].m_type);
        DOCTEST_CHECK(recorder.m_session.m_tracks[track].m_name == tracks[track].m_name);
    }

    synth.PopulateUIState(&rig.UIState().m_squiggleBoyUIState);
    DOCTEST_CHECK(rig.UIState().m_squiggleBoyUIState.m_recordingState == StreamingRecorder::State::Recording);
    DOCTEST_CHECK(rig.UIState().m_squiggleBoyUIState.m_recordingError == StreamingRecorder::Error::None);
    DOCTEST_CHECK(recordCell.GetColor() == SmartGrid::Color::Red);
    recordCell.OnPress(127);
    DOCTEST_CHECK_FALSE(synth.IsRecording());
    synth.ShutdownRecording();
    DOCTEST_CHECK(synth.GetRecordingError() == StreamingRecorder::Error::None);
    DOCTEST_REQUIRE(synth.PrepareRecording());
    recordCell.OnPress(127);
    synth.m_mixerState.m_numMonoInputs = 0;
    process();
    synth.PopulateUIState(&rig.UIState().m_squiggleBoyUIState);
    DOCTEST_CHECK(rig.UIState().m_squiggleBoyUIState.m_recordingError == StreamingRecorder::Error::InvalidConfiguration);
    const size_t savedSample = SampleTimer::s_instance->m_sample;
    SampleTimer::s_instance->m_sample = 0;
    DOCTEST_CHECK(recordCell.GetColor() == SmartGrid::Color::Red);
    SampleTimer::s_instance->m_sample = SampleTimer::x_sampleRate / 8;
    DOCTEST_CHECK(recordCell.GetColor() == SmartGrid::Color::Off);
    SampleTimer::s_instance->m_sample = SampleTimer::x_sampleRate / 4;
    DOCTEST_CHECK(recordCell.GetColor() == SmartGrid::Color::Red);
    SampleTimer::s_instance->m_sample = savedSample;
    DOCTEST_CHECK_FALSE(synth.IsRecording());
}
