#include "doctest.h"
#include "NonagonWrapper.hpp"
#include "support/GlobalEnv.hpp"
#include "support/TempDir.hpp"

DOCTEST_TEST_CASE("audio reattachment preserves the live engine and recording")
{
    GlobalEnv::ResetPerTest();
    synthrig::TempDir directory;
    DOCTEST_REQUIRE(directory.Valid());
    auto app = std::make_unique<NonagonWrapper>();
    app->SetRecordingDirectory(directory.String().c_str());
    app->PrepareToPlay(512, 48000);
    app->m_internal.m_running = true;
    app->m_internal.m_squiggleBoy.m_masterVolume.m_expParam = 0.37f;
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    const juce::AudioSourceChannelInfo block(&buffer, 0, 512);
    const NonagonWrapper::IOInfo io{0, 2, 2, true};
    app->Process(block, io);
    DOCTEST_REQUIRE(app->m_internal.StartRecording());
    app->Process(block, io);

    auto& recorder = app->m_internal.m_context.m_recorder;
    const auto recordingStart = recorder.m_recordingStartSample;
    const auto acceptedFrames = recorder.m_acceptedFrames;
    const auto sample = SampleTimer::GetSample();
    const auto timer = app->m_internal.m_timer;
    JsonArena beforeArena(JsonArena::kDefaultCapacity);
    std::unique_ptr<char, decltype(&free)> before(app->m_internal.ToJSON(beforeArena).Dumps(0), free);

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        app->ReleaseResources();
        DOCTEST_CHECK(recorder.IsRecording());
        app->PrepareToPlay(512, 48000);
        DOCTEST_CHECK(recorder.IsRecording());
        DOCTEST_CHECK(recorder.m_recordingStartSample == recordingStart);
        DOCTEST_CHECK(recorder.m_acceptedFrames == acceptedFrames);
        DOCTEST_CHECK(SampleTimer::GetSample() == sample);
        DOCTEST_CHECK(app->m_internal.m_running);
        DOCTEST_CHECK(app->m_internal.m_timer == timer);
        JsonArena afterArena(JsonArena::kDefaultCapacity);
        std::unique_ptr<char, decltype(&free)> after(app->m_internal.ToJSON(afterArena).Dumps(0), free);
        DOCTEST_CHECK(std::string(after.get()) == before.get());
    }

    app->Process(block, io);
    DOCTEST_CHECK(recorder.IsRecording());
    DOCTEST_CHECK(recorder.m_acceptedFrames == acceptedFrames + 512);
    DOCTEST_CHECK(SampleTimer::GetSample() == sample + 512);
    app.reset();
    size_t recordings = 0;
    for (const auto& file : std::filesystem::directory_iterator(directory.Path()))
    {
        if (file.path().extension() == ".sgrec")
        {
            ++recordings;
        }
    }

    DOCTEST_CHECK(recordings == 1);
}

DOCTEST_TEST_CASE("audio reattachment anchors MIDI time to the first resumed block")
{
    GlobalEnv::ResetPerTest();
    synthrig::TempDir directory;
    auto app = std::make_unique<NonagonWrapper>();
    app->SetRecordingDirectory(directory.String().c_str());
    app->PrepareToPlay(512, 48000);
    SampleTimer::StartFrame(1000000);
    for (int sample = 0; sample < 512; ++sample)
    {
        SampleTimer::IncrementSample();
    }

    app->ReleaseResources();
    app->PrepareToPlay(512, 48000);
    SampleTimer::StartFrame(9000000);
    DOCTEST_CHECK(SampleTimer::GetSample() == 512);
    DOCTEST_CHECK(SampleTimer::GetAbsTimeUs() == 9000000);
    const auto scheduled = SmartGrid::MidiOutputSchedule::Plan(SampleTimer::GetAbsTimeUs(), 9005000, 24000000);
    DOCTEST_CHECK_FALSE(scheduled.m_drop);
    DOCTEST_CHECK(scheduled.m_hostTicks == 216480000);
    SampleTimer::IncrementSample();
    DOCTEST_CHECK(SampleTimer::GetAbsTimeUs() == 9000021);
    DOCTEST_CHECK_FALSE(app->m_internal.m_running);
    DOCTEST_CHECK_FALSE(app->m_internal.m_context.m_recorder.IsRecording());
}
