#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"
#include "QuadDelay.hpp"
#include <array>
#include <cmath>
#include <memory>

DOCTEST_TEST_CASE("Delay heads: audio and display share a position at every speed and loop")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    clock.SetRunning(true);
    clock.AdvanceSample();
    const double speeds[] =
    {
        -4, -3, -2, -1.5, -4.0 / 3.0, -1, -0.5, -0.25,
        0.25, 0.5, 1, 4.0 / 3.0, 1.5, 2, 3, 4
    };
    constexpr double x_period = 65536;
    for (int loop = 0; loop < TheoryOfTimeBase::x_numLoops; ++loop)
    {
        for (double phase : {-2.375, 0.0, 3.375, 128.125})
        {
            for (double glue : {-7919.25, 0.0, 11713.5})
            {
                for (double speed : speeds)
                {
                    DOCTEST_CAPTURE(loop);
                    DOCTEST_CAPTURE(phase);
                    DOCTEST_CAPTURE(glue);
                    DOCTEST_CAPTURE(speed);
                    WriteTapeHead writer;
                    writer.m_globalPeriodSamples = x_period;
                    writer.m_relativePosition = phase;
                    writer.m_actualPosition = phase * x_period + glue;
                    writer.m_glue = glue;
                    ReadTapeHead reader;
                    reader.m_writeTapeHead = &writer;
                    reader.m_loopSelector = loop;
                    ReadTapeHead::Input input;
                    input.m_theoryOfTime = clock.Get();
                    input.m_requestedLoopSelector = loop;
                    input.m_bufferFraction = 0.75;
                    input.m_readHeadSpeed = speed;
                    reader.Update(input);

                    const double audioDelay = writer.m_actualPosition - reader.m_actualPosition;
                    const double displayDelay = (writer.m_relativePosition - reader.m_relativePosition) * x_period;
                    DOCTEST_CHECK(audioDelay == doctest::Approx(displayDelay).epsilon(1e-10));
                    const double hop = Resynthesizer::GetGrainLaunchSamples();
                    const double loopSamples = TapeHead::ComputeLoopSamples(clock.Get(), loop, x_period);
                    DOCTEST_CHECK(audioDelay >= hop - 1e-8);
                    DOCTEST_CHECK(audioDelay <= hop + loopSamples + 1e-8);
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("Delay heads: transport and tempo glue keep the audio aligned with the display")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    clock.SetGlobalPeriodSamples(65536.0 * SampleTimer::x_oversample);
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = clock.Get();
    const int speedSettings[] = {5, 9, 10, 13};
    for (size_t channel = 0; channel < 4; ++channel)
    {
        input.m_readHeadSpeedSwitchVal[channel] = speedSettings[channel];
    }

    QuadDelay::Input output;
    for (int stage = 0; stage < 5; ++stage)
    {
        clock.SetRunning(stage != 0 && stage != 3);
        if (stage == 2)
        {
            clock.SetGlobalPeriodSamples(49152.0 * SampleTimer::x_oversample);
        }

        const int samples = stage == 0 ? 8192 : 32;
        for (int sample = 0; sample < samples; ++sample)
        {
            clock.AdvanceSample();
            setter.Process(input, output, nullptr);
            const auto& writer = setter.m_writeTapeHead;
            for (size_t channel = 0; channel < 4; ++channel)
            {
                DOCTEST_CAPTURE(stage);
                DOCTEST_CAPTURE(channel);
                const double audioPhase = output.m_relativeWriteHeadPosition +
                    (output.m_readHeadPosition[channel] - output.m_writeHeadPosition) / writer.m_globalPeriodSamples;
                const double phaseDifference = audioPhase - output.m_relativeReadHeadPosition[channel];
                DOCTEST_CHECK(std::abs(phaseDifference - std::round(phaseDifference)) < 1e-6);
            }
        }

        DOCTEST_REQUIRE(std::abs(setter.m_writeTapeHead.m_glue) > 1000.0);
    }
}

DOCTEST_TEST_CASE("Delay heads: stopped transport retains every selected playback speed")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    constexpr double x_period = 65536;
    clock.SetGlobalPeriodSamples(x_period * SampleTimer::x_oversample);
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = clock.Get();
    QuadDelay::Input output;
    const double speeds[] =
    {
        -4, -3, -2, -1.5, -4.0 / 3.0, -1, -0.5, -0.25,
        0.25, 0.5, 1, 4.0 / 3.0, 1.5, 2, 3, 4
    };
    for (int group = 0; group < 4; ++group)
    {
        for (size_t channel = 0; channel < 4; ++channel)
        {
            input.m_readHeadSpeedSwitchVal[channel] = group * 4 + channel;
            setter.m_bufferFrac[channel] = 0.75f;
        }

        clock.AdvanceSample();
        setter.Process(input, output, nullptr);
        const double startWrite = output.m_writeHeadPosition;
        const float startDisplayWrite = output.m_relativeWriteHeadPosition;
        const QuadDouble startRead = output.m_readHeadPosition;
        const QuadFloat startDisplayRead = output.m_relativeReadHeadPosition;
        constexpr size_t x_hostSamples = 128;
        for (size_t sample = 0; sample < x_hostSamples * SampleTimer::x_oversample; ++sample)
        {
            clock.AdvanceSample();
            setter.Process(input, output, nullptr);
        }

        DOCTEST_REQUIRE(clock.GlobalPhase() == 0.0);
        DOCTEST_CHECK(output.m_writeHeadPosition - startWrite == doctest::Approx(x_hostSamples));
        DOCTEST_CHECK(output.m_relativeWriteHeadPosition - startDisplayWrite == doctest::Approx(x_hostSamples / x_period));
        for (size_t channel = 0; channel < 4; ++channel)
        {
            const double speed = speeds[group * 4 + channel];
            DOCTEST_CAPTURE(speed);
            DOCTEST_CHECK(output.m_readHeadPosition[channel] - startRead[channel] == doctest::Approx(speed * x_hostSamples));
            DOCTEST_CHECK(output.m_relativeReadHeadPosition[channel] - startDisplayRead[channel] ==
                doctest::Approx(speed * x_hostSamples / x_period).epsilon(1e-4));
        }
    }
}

DOCTEST_TEST_CASE("Delay heads: stopped waveform fills the display and survives transport restart")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    constexpr size_t x_period = 32768;
    clock.SetGlobalPeriodSamples(x_period * SampleTimer::x_oversample);
    auto delay = std::make_unique<QuadDelay>();
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = clock.Get();
    QuadDelay::Input output;
    const size_t samples = (2 * x_period + 3 * x_period / 8) * SampleTimer::x_oversample;
    for (size_t sample = 0; sample < samples; ++sample)
    {
        clock.AdvanceSample();
        setter.Process(input, output, delay.get());
        if ((sample + 1) % SampleTimer::x_oversample == 0)
        {
            const double phase = output.m_writeHeadPosition / x_period;
            const double wrappedPhase = phase - std::floor(phase);
            const float audio = 0.25 <= wrappedPhase && wrappedPhase < 0.375 ? 0.5f : 0.0f;
            delay->m_delayLine.Write(QuadFloat::Splat(audio), output.m_writeHeadPosition);
        }
    }

    DOCTEST_REQUIRE(clock.GlobalPhase() == 0.0);
    DOCTEST_CHECK(output.m_relativeWriteHeadPosition == doctest::Approx(0.375));
    DOCTEST_CHECK(delay->m_positionalBufferRecorder.m_maxIndex == QuadDelay::x_positionalBufferSize - 1);
    QuadDelay::UIState ui;
    delay->PopulateUIState(&ui, output);
    const QuadDelay::UISnapshot stopped = ui.GetCurrentSnapshot();
    const size_t toneBucket = 5 * QuadDelay::x_positionalBufferSize / 16;
    const size_t silentBucket = 3 * QuadDelay::x_positionalBufferSize / 4;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(stopped.m_delayEnvelopeState[channel].m_maxEnvelope[toneBucket] == doctest::Approx(0.5));
        DOCTEST_CHECK(stopped.m_delayEnvelopeState[channel].m_maxEnvelope[silentBucket] == doctest::Approx(0.0));
    }

    // Starting resets the musical phase. The displayed waveform must keep its
    // coordinate origin, so that reset cannot draw a false segment across it.
    //
    const double stoppedWrite = output.m_writeHeadPosition;
    clock.SetRunning(true);
    clock.AdvanceSample();
    setter.Process(input, output, delay.get());
    delay->PopulateUIState(&ui, output);
    const auto& restarted = ui.GetCurrentSnapshot();
    DOCTEST_CHECK(output.m_writeHeadPosition == stoppedWrite);
    DOCTEST_CHECK(restarted.m_delayEnvelopeState[0].m_relativeWriteHeadPosition ==
        stopped.m_delayEnvelopeState[0].m_relativeWriteHeadPosition);
    for (size_t channel = 0; channel < 4; ++channel)
    {
        for (size_t bucket = 0; bucket < QuadDelay::x_positionalBufferSize; ++bucket)
        {
            DOCTEST_REQUIRE(restarted.m_delayEnvelopeState[channel].m_maxEnvelope[bucket] ==
                stopped.m_delayEnvelopeState[channel].m_maxEnvelope[bucket]);
            DOCTEST_REQUIRE(restarted.m_delayEnvelopeState[channel].m_minEnvelope[bucket] ==
                stopped.m_delayEnvelopeState[channel].m_minEnvelope[bucket]);
        }
    }

    for (bool running : {true, false, true, false})
    {
        const float previousDisplay = output.m_relativeWriteHeadPosition;
        const double previousWrite = output.m_writeHeadPosition;
        clock.SetRunning(running);
        clock.AdvanceSample();
        setter.Process(input, output, delay.get());
        const double movement = output.m_writeHeadPosition - previousWrite;
        DOCTEST_CHECK(output.m_relativeWriteHeadPosition == doctest::Approx(previousDisplay + movement / x_period).epsilon(1e-6));
    }

    // Tempo edits while stopped change the sweep rate without moving the
    // display origin or changing the real-time write speed.
    //
    const double previousPhase = setter.m_writeTapeHead.m_relativePosition;
    const double previousWrite = output.m_writeHeadPosition;
    clock.SetGlobalPeriodSamples(2.0 * x_period * SampleTimer::x_oversample);
    for (size_t sample = 0; sample < 1024 * SampleTimer::x_oversample; ++sample)
    {
        clock.AdvanceSample();
        setter.Process(input, output, delay.get());
    }

    DOCTEST_CHECK(output.m_writeHeadPosition - previousWrite == doctest::Approx(1024.0));
    DOCTEST_CHECK(setter.m_writeTapeHead.m_relativePosition - previousPhase == doctest::Approx(1024.0 / (2.0 * x_period)));
}

DOCTEST_TEST_CASE("Delay heads: sparse audio under the visualized heads reaches grain playback")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    clock.SetRunning(true);
    clock.AdvanceSample();
    constexpr size_t x_period = 65536;
    constexpr double x_glue = 30000.75;
    using Storage = QuadDelayLineMovableWriter<x_period>;
    auto storage = std::make_unique<Storage>();
    auto grains = std::make_unique<QuadGrainManager<Storage>>(storage.get());
    QuadGrainManager<Storage>::Input grainInput;
    PositionalBufferRecorder<QuadDelay::x_positionalBufferSize> positions;
    WriteTapeHead writer;
    writer.m_globalPeriodSamples = x_period;
    writer.m_relativePosition = 3.375;
    writer.m_actualPosition = writer.m_relativePosition * x_period + x_glue;
    writer.m_glue = x_glue;
    const double speeds[] = {1, 0.5, 2, -1};
    std::array<ReadTapeHead, 4> readers;
    QuadDouble audioHeads;
    QuadDouble displayedHeads;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        readers[channel].m_writeTapeHead = &writer;
        ReadTapeHead::Input input;
        input.m_theoryOfTime = clock.Get();
        input.m_bufferFraction = 0.75;
        input.m_readHeadSpeed = speeds[channel];
        readers[channel].Update(input);
        audioHeads[channel] = readers[channel].m_actualPosition;
        displayedHeads[channel] = readers[channel].m_relativePosition * x_period + x_glue;
    }

    // Record one sparse tone burst per channel at the position shown by its
    // read head. Populate the same phase-to-buffer map used by the visualizer.
    //
    for (size_t sample = 0; sample < x_period; ++sample)
    {
        const double position = writer.m_actualPosition - x_period + sample;
        QuadFloat audio;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            const double offset = position - displayedHeads[channel];
            audio[channel] = -1024 <= offset && offset < 2048
                ? 0.2f * std::cos(2.0 * M_PI * 18.0 * offset / 4096.0)
                : 0.0f;
        }

        storage->Write(audio, position);
        positions.Record((position - x_glue) / x_period, position);
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        const double displayedPosition = positions.Get(readers[channel].m_relativePosition);
        auto envelope = storage->GetEnvelopeAtWarpedTime(displayedPosition);
        DOCTEST_REQUIRE(envelope.first[channel] > 0.1f);
        DOCTEST_REQUIRE(envelope.second[channel] < -0.1f);
    }

    std::array<double, 4> energy{};
    for (size_t sample = 0; sample < 8192 * SampleTimer::x_oversample; ++sample)
    {
        QuadFloat output = grains->Process(audioHeads, QuadFloat(), grainInput, storage->m_lastTime - 1.0);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            energy[channel] += output[channel] * output[channel];
        }
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CAPTURE(speeds[channel]);
        DOCTEST_CHECK(energy[channel] > 1.0);
    }
}
