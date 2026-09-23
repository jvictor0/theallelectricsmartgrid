#include "doctest.h"
#include "support/SynthRig.hpp"
#include <filesystem>
#include <thread>

DOCTEST_TEST_CASE("recording engine: bulk patch replay checkpoints include loads reloads and resets")
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("smartgrid-bulk-patches-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    synthrig::SynthRig rig;
    rig.RunFrames(2);
    auto& internal = rig.Internal();
    auto& interchange = internal.m_stateInterchange;
    auto& recorder = internal.m_context.m_recorder;
    auto* encoder = internal.m_squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(0);
    const std::string base = rig.SavePatch();
    encoder->FillModulators(&internal.m_context);
    auto* depth = encoder->m_modulators.m_modulators[0].get();
    depth->SetAndRecordValue(0.875f, 0, 0);
    depth->m_modulators.AddGesture(depth, 0);
    depth->m_modulators.m_gestures[0]->SetAndRecordValue(0.75f, 0, 0);
    depth->m_modulators.m_gestures[0]->SetActive(true, 0, 0);
    encoder->m_modulators.AddGesture(encoder, 1);
    encoder->m_modulators.m_gestures[1]->SetAndRecordValue(0.625f, 0, 0);
    encoder->m_modulators.m_gestures[1]->SetActive(true, 0, 0);
    const std::string rich = rig.SavePatch();
    DOCTEST_REQUIRE(rig.PrepareRecording(directory.string()));
    auto session = recorder.m_session;
    session.m_blockFrames = 4;
    DOCTEST_REQUIRE(recorder.Prepare(session, directory.string()));
    rig.PressPad(synthrig::SynthRig::RouteBottomLeft, -1, 7);
    rig.RunSamples(1);
    DOCTEST_REQUIRE(recorder.IsRecording());
    std::vector<std::string> checkpoints;
    JsonArena snapshot(JsonArena::kDefaultCapacity);
    const auto checkpoint = [&]()
    {
        snapshot.Reset();
        char* text = internal.ToJSON(snapshot).Dumps(0);
        DOCTEST_REQUIRE(text != nullptr);
        checkpoints.push_back("{\"sample\":" + std::to_string(recorder.m_acceptedFrames - 1)
            + ",\"patch\":" + text + "}");
        std::free(text);
    };
    const auto load = [&](const std::string& text, bool restore)
    {
        DOCTEST_REQUIRE(interchange.RequestLoadText(text, restore));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        do
        {
            interchange.RetryPendingLoad();
            internal.HandleStateInterchange();
            if (!interchange.IsLoadRequested() && interchange.m_pendingLoad.empty())
            {
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);

        DOCTEST_FAIL("Patch load remained pending");
    };
    checkpoint();
    AudioInputBuffer input;
    for (size_t sample = 1; sample <= 7; ++sample)
    {
        SampleTimer::IncrementSample();
        if (sample == 1)
        {
            encoder->SetAndRecordValue(0.9f, 0, 0);
            load(base, true);
            DOCTEST_CHECK(encoder->m_modulators.m_modulators[0].get() == nullptr);
            DOCTEST_CHECK(encoder->m_modulators.m_gestures[1].get() == nullptr);
            encoder->SetAndRecordValue(0.6f, 0, 0);
        }
        else if (sample == 2)
        {
            load("{\"blend\":0.75,\"faders\":[0.875,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\"configGrid\":{}}", true);
            DOCTEST_CHECK(internal.m_context.m_sceneManager.m_blendFactor == 0.75f);
            DOCTEST_CHECK(internal.m_squiggleBoyState.m_faders[0] == 0.875f);
        }
        else if (sample == 3)
        {
            load(rich, false);
            DOCTEST_CHECK(internal.m_context.m_sceneManager.m_blendFactor == 0.75f);
            DOCTEST_CHECK(internal.m_squiggleBoyState.m_faders[0] == 0.875f);
            DOCTEST_CHECK(encoder->m_modulators.m_modulators[0].get() != nullptr);
            DOCTEST_REQUIRE(interchange.RequestSave());
            internal.HandleStateInterchange();
            DOCTEST_REQUIRE(interchange.IsSavePending());
            interchange.AckSaveCompleted();
        }
        else if (sample == 4)
        {
            encoder->SetAndRecordValue(0.25f, 0, 0);
            internal.HandleParamSet({SmartGrid::MessageIn::Mode::ParamSet14, 0, 0, 4096});
        }
        else if (sample == 5)
        {
            TheNonagonSquiggleBoyInternal::SaveLoadJSONCell reload(&internal, false);
            reload.OnPress(127);
            encoder->SetAndRecordValue(0.125f, 0, 0);
            reload.OnPress(127);
        }
        else if (sample == 6)
        {
            load("{\"nonagon\":{\"Mute_0\":[1,0,1]},\"configGrid\":{\"sourceStereo\":[true,false,true,false],\"sourceSelected\":[[true,true,true,true]],\"sourceMonitor\":[false,true,false,true]}}", true);
        }
        else
        {
            DOCTEST_REQUIRE(interchange.RequestNew());
            internal.HandleStateInterchange();
            DOCTEST_CHECK_FALSE(interchange.IsNewRequested());
            encoder->SetAndRecordValue(0.5f, 0, 0);
        }

        internal.ProcessSample(input);
        checkpoint();
    }

    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_REQUIRE(recorder.GetError() == StreamingRecorder::Error::None);
    DOCTEST_CHECK(recorder.m_writtenFrames == 8);
    if (const char* output = std::getenv("SMARTGRID_PATCH_LOAD_FIXTURE"))
    {
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            std::filesystem::copy_file(entry.path(), output, std::filesystem::copy_options::overwrite_existing);
        }

        std::ofstream expected(std::string(output) + ".json");
        expected << '[';
        for (size_t i = 0; i < checkpoints.size(); ++i)
        {
            expected << (i == 0 ? "" : ",") << checkpoints[i];
        }

        expected << ']';
        DOCTEST_REQUIRE(expected.good());
    }

    std::filesystem::remove_all(directory);
}

DOCTEST_TEST_CASE("recording engine: full patch loads and resets do not overflow parameter capture")
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("smartgrid-load-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    synthrig::SynthRig rig;
    rig.RunFrames(2);
    const std::string patch = rig.SavePatch();
    DOCTEST_REQUIRE_FALSE(patch.empty());
    DOCTEST_REQUIRE(rig.PrepareRecording(directory.string()));
    auto& internal = rig.Internal();
    auto& recorder = internal.m_context.m_recorder;
    DOCTEST_REQUIRE(internal.StartRecording());
    rig.RunSamples(1);

    DOCTEST_SUBCASE("load")
    {
        DOCTEST_REQUIRE(rig.LoadPatch(patch));
    }

    DOCTEST_SUBCASE("reset")
    {
        rig.ResetToDefaults();
    }

    DOCTEST_CHECK(recorder.GetError() == StreamingRecorder::Error::None);
    DOCTEST_CHECK(recorder.IsRecording());
    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_CHECK(recorder.m_writtenFrames == recorder.m_acceptedFrames);
    std::filesystem::remove_all(directory);
}

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
    StreamingRecorder& recorder = rig.Internal().m_context.m_recorder;
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

DOCTEST_TEST_CASE("recording engine: initial patch and all parameter types reach the recording together")
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("smartgrid-state-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    synthrig::SynthRig rig;
    // Let timestamp-zero UI messages clear the existing 20 ms input latency.
    //
    rig.RunFrames(2);
    auto& internal = rig.Internal();
    StreamingRecorder& recorder = internal.m_context.m_recorder;
    State* mute = internal.m_nonagon.m_stateSaver.Get("Mute", 0);
    State* initialMute = internal.m_nonagon.m_stateSaver.Get("Mute", 1);
    initialMute->Set(true);
    mute->Set(false);
    internal.SetBlendFactor(0.125f);
    auto* encoder = internal.m_squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(0);
    DOCTEST_REQUIRE(rig.PrepareRecording(directory.string()));
    auto session = recorder.m_session;
    session.m_blockFrames = 4;
    DOCTEST_REQUIRE(recorder.Prepare(session, directory.string()));
    rig.PressPad(synthrig::SynthRig::RouteBottomLeft, -1, 7);
    rig.PressScenePad(2);
    rig.RunSamples(1);
    DOCTEST_REQUIRE(recorder.GetState() == StreamingRecorder::State::Recording);
    DOCTEST_REQUIRE(recorder.m_acceptedFrames == 1);
    AudioInputBuffer input;
    for (size_t frame = 1; frame <= 4; ++frame)
    {
        SampleTimer::IncrementSample();
        if (frame == 1)
        {
            mute->Set(true);
            mute->Set(false);
            mute->Set(true);
            internal.m_configGrid.Get(6, 0)->OnPress(127);
            internal.HandleParamSet({SmartGrid::MessageIn::Mode::ParamSet14, 0, 0, 8192});
            internal.HandleParamSet({SmartGrid::MessageIn::Mode::ParamSet14, 4, 0, 4096});
        }
        else if (frame == 2)
        {
            internal.m_configGrid.m_sourceWidthStates[0]->Set(SourceMixer::SourceWidth::Stereo);
            internal.m_configGrid.m_sourceSelectedStates[0][1]->Set(true);
            encoder->SetAndRecordValue(0.75f, 0, 0);
            encoder->SetAndRecordValue(0.625f, 2, 1);
        }
        else if (frame == 3)
        {
            mute->LoadValFromScene(2);
            mute->Set(true);
            internal.m_activeTrioState->Set(TheNonagonSmartGrid::Trio::Water);
            encoder->m_modulators.AddGesture(encoder, 2);
            auto* gesture = encoder->m_modulators.m_gestures[2].get();
            gesture->SetActive(true);
            encoder->FillModulators(&internal.m_context);
            auto* depth = encoder->m_modulators.m_modulators[1].get();
            depth->m_modulators.AddGesture(depth, 0);
            auto* nestedGesture = depth->m_modulators.m_gestures[0].get();
            nestedGesture->SetAndRecordValue(0.625f, 1, 0);
            nestedGesture->SetActive(true, 1, 0);
        }
        else if (frame == 4)
        {
            internal.m_configGrid.Get(6, 0)->OnPress(127);
            internal.m_configGrid.Get(7, 0)->OnPress(127);
            encoder->m_modulators.m_gestures[2]->SetActive(false, 0, 0);
            internal.HandleParamSet({SmartGrid::MessageIn::Mode::ParamSet14, 4, 0, 16383});
        }

        internal.ProcessSample(input);
    }

    recorder.Stop();
    recorder.Shutdown();
    DOCTEST_REQUIRE(recorder.GetError() == StreamingRecorder::Error::None);
    std::filesystem::path recording;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        recording = entry.path();
    }

    DOCTEST_REQUIRE(!recording.empty());
    std::ifstream source(recording, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    DOCTEST_REQUIRE(bytes.size() > 12);
    uint32_t headerBytes = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        headerBytes |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[8 + i])) << (8 * i);
    }

    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON header = arena.Loads(bytes.substr(12, headerBytes).c_str());
    JSON patch = header.Get("initial_patch");
    DOCTEST_REQUIRE_FALSE(patch.Get("nonagon").IsNull());
    DOCTEST_CHECK(patch.Get("nonagon").Get("Mute_0").GetAt(0).IntegerValue() == 0);
    DOCTEST_CHECK(patch.Get("nonagon").Get("Mute_1").GetAt(0).IntegerValue() == 1);
    DOCTEST_CHECK(patch.Get("stateSaver").Get("sceneStateRight").GetAt(0).IntegerValue() == 1);
    DOCTEST_CHECK(patch.Get("stateSaver").Get("sourceMonitor_0").GetAt(0).IntegerValue() == 1);
    DOCTEST_CHECK_FALSE(patch.Get("faders").IsNull());
    DOCTEST_CHECK(patch.Get("blend").NumberValue() == 0.125);
    DOCTEST_CHECK(patch.Get("squiggleBoy").Get("Harmonics1").Get("gestures").IsNull());
    DOCTEST_CHECK(recorder.m_writtenFrames == 5);
    if (const char* output = std::getenv("SMARTGRID_STATE_RECORDING_FIXTURE"))
    {
        std::filesystem::copy_file(recording, output, std::filesystem::copy_options::overwrite_existing);
    }

    std::filesystem::remove_all(directory);
}

DOCTEST_TEST_CASE("recording engine: reconstructed patch loads recorded state and config copies")
{
    const char* path = std::getenv("SMARTGRID_RECONSTRUCTED_PATCH");
    if (path == nullptr)
    {
        return;
    }

    std::ifstream input(path);
    DOCTEST_REQUIRE(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    JsonArena arena(JsonArena::kDefaultCapacity);
    JSON patch = arena.Loads(text.c_str());
    DOCTEST_REQUIRE_FALSE(patch.IsNull());
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    internal.FromJSON(patch, true);
    State* mute = internal.m_nonagon.m_stateSaver.Get("Mute", 0);
    DOCTEST_CHECK(mute->Get<bool>());
    DOCTEST_CHECK(mute->m_buf[2] == 1);
    DOCTEST_CHECK(internal.m_activeTrio == TheNonagonSmartGrid::Trio::Water);
    DOCTEST_CHECK(internal.m_configGrid.m_sourceWidthStates[0]->Get<SourceMixer::SourceWidth>() == SourceMixer::SourceWidth::Stereo);
    DOCTEST_CHECK(internal.m_configGrid.m_sourceSelected[0][1]);
    DOCTEST_CHECK_FALSE(internal.m_configGrid.m_sourceMonitor[0]);
    DOCTEST_CHECK(internal.m_configGrid.m_sourceMonitor[1]);
    DOCTEST_CHECK(internal.m_context.m_sceneManager.m_blendFactor == doctest::Approx(8192.0f / 16383.0f));
    DOCTEST_CHECK(internal.m_squiggleBoyState.m_faders[3] == doctest::Approx(4096.0f / 16383.0f));
    auto* encoder = internal.m_squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(0);
    DOCTEST_CHECK(encoder->m_values[1][2] == 0.625f);
    auto* gesture = encoder->m_modulators.m_gestures[2].get();
    DOCTEST_REQUIRE(gesture != nullptr);
    DOCTEST_CHECK(gesture->m_isActive[0][0]);
    DOCTEST_CHECK(gesture->m_values[0][0] == 0.75f);
    auto* depth = encoder->m_modulators.m_modulators[1].get();
    DOCTEST_REQUIRE(depth != nullptr);
    DOCTEST_REQUIRE(depth->m_modulators.m_gestures[0].get() != nullptr);
    DOCTEST_CHECK(depth->m_modulators.m_gestures[0]->m_isActive[1][0]);
    DOCTEST_CHECK(depth->m_modulators.m_gestures[0]->m_values[0][1] == 0.625f);
}

DOCTEST_TEST_CASE("recording engine: owner destruction drains events before freeing states")
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("smartgrid-owner-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    auto internal = std::make_unique<TheNonagonSquiggleBoyInternal>();
    internal->SetRecordingDirectory(directory.c_str());
    DOCTEST_REQUIRE(internal->PrepareRecording());
    StreamingRecorder& recorder = internal->m_context.m_recorder;
    DOCTEST_REQUIRE(internal->StartRecording());
    internal->m_nonagon.m_stateSaver.Get("Mute", 0)->Set(true);
    recorder.BeginFrame();
    recorder.CommitFrame();
    internal.reset();
    size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        std::ifstream input(entry.path(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        DOCTEST_REQUIRE(bytes.size() > 16);
        DOCTEST_CHECK(bytes.substr(bytes.size() - 16, 4) == "END1");
        DOCTEST_CHECK(static_cast<uint8_t>(bytes[bytes.size() - 12]) == 1);
        ++files;
    }

    DOCTEST_CHECK(files == 1);
    std::filesystem::remove_all(directory);
}
