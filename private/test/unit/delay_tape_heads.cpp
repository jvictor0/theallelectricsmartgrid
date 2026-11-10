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
    for (int loop = 0; loop < TheoryOfTimeBase::x_numLoops; ++loop)
    {
        for (double phase : {-2.375, 0.0, 3.375, 128.125})
        {
            for (double speed : speeds)
            {
                DOCTEST_CAPTURE(loop);
                DOCTEST_CAPTURE(phase);
                DOCTEST_CAPTURE(speed);
                clock.Get()->m_phaseClock.m_phase = phase;
                ReadTapeHead reader;
                reader.m_loopSelector = loop;
                ReadTapeHead::Input input;
                input.m_theoryOfTime = clock.Get();
                input.m_requestedLoopSelector = loop;
                input.m_bufferFraction = 0.75;
                input.m_readHeadSpeed = speed;
                reader.Update(input);
                double period = ReadTapeHead::ComputeLoopPeriod(*clock.Get(), loop);
                DOCTEST_CHECK(reader.m_phase >= phase - period);
                DOCTEST_CHECK(reader.m_phase < phase);
                double wrapped = (reader.m_phase - (phase * speed - period * 0.75)) / period;
                DOCTEST_CHECK(wrapped == doctest::Approx(std::round(wrapped)));
                double before = reader.m_phase;
                clock.Get()->m_globalPeriodSamples *= 2.0;
                reader.Update(input);
                DOCTEST_CHECK(reader.m_phase == before);
                clock.Get()->m_phaseClock.Resume(-700.25);
                reader.Update(input);
                DOCTEST_CHECK(reader.m_phase == before);
            }
        }
    }
}

DOCTEST_TEST_CASE("Delay heads: transport and tempo changes keep the audio aligned with the display")
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
            setter.Process(input, output);
            for (size_t channel = 0; channel < 4; ++channel)
            {
                DOCTEST_CAPTURE(stage);
                DOCTEST_CAPTURE(channel);
                const double audioPhase = output.m_relativeWriteHeadPosition +
                    (output.m_readHeadPhase[channel] - output.m_writeHeadPhase);
                const double phaseDifference = audioPhase - output.m_relativeReadHeadPosition[channel];
                DOCTEST_CHECK(std::abs(phaseDifference - std::round(phaseDifference)) < 1e-6);
            }
        }

        DOCTEST_CHECK(std::isfinite(clock.Get()->m_phaseClock.m_offset));
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
        setter.Process(input, output);
        const double startWrite = output.m_writeHeadPhase;
        const float startDisplayWrite = output.m_relativeWriteHeadPosition;
        const QuadDouble startRead = output.m_readHeadPhase;
        const QuadFloat startDisplayRead = output.m_relativeReadHeadPosition;
        constexpr size_t x_hostSamples = 128;
        for (size_t sample = 0; sample < x_hostSamples * SampleTimer::x_oversample; ++sample)
        {
            clock.AdvanceSample();
            setter.Process(input, output);
        }

        DOCTEST_REQUIRE(clock.GlobalPhase() == 0.0);
        DOCTEST_CHECK(output.m_writeHeadPhase - startWrite == doctest::Approx(x_hostSamples / x_period));
        DOCTEST_CHECK(output.m_relativeWriteHeadPosition - startDisplayWrite == doctest::Approx(x_hostSamples / x_period));
        for (size_t channel = 0; channel < 4; ++channel)
        {
            const double speed = speeds[group * 4 + channel];
            DOCTEST_CAPTURE(speed);
            DOCTEST_CHECK(output.m_readHeadPhase[channel] - startRead[channel] == doctest::Approx(speed * x_hostSamples / x_period));
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
    delay->m_delayLine.m_inverter = clock.Get()->m_timeWarpInverter.get();
    QuadDelay::UIState ui;
    delay->SetupScopeWriter(&ui);
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = clock.Get();
    QuadDelay::Input output;
    const size_t samples = (2 * x_period + 3 * x_period / 8) * SampleTimer::x_oversample;
    for (size_t sample = 0; sample < samples; ++sample)
    {
        clock.AdvanceSample();
        setter.Process(input, output);
        if ((sample + 1) % SampleTimer::x_oversample == 0)
        {
            const double phase = output.m_writeHeadPhase;
            const double wrappedPhase = phase - std::floor(phase);
            const float audio = 0.25 <= wrappedPhase && wrappedPhase < 0.375 ? 0.5f : 0.0f;
            delay->WriteAudio(QuadFloat::Splat(audio), SampleTimer::GetHostSample());
        }

        delay->WriteEnvelopeScope();
        ui.m_scopeWriter.AdvanceIndex();
        if (sample % 512 == 511)
        {
            ui.m_scopeWriter.Publish();
            ui.m_scopeWriter.AcceptSamples();
        }
    }

    DOCTEST_REQUIRE(clock.GlobalPhase() == 0.0);
    DOCTEST_CHECK(output.m_relativeWriteHeadPosition == doctest::Approx(0.375));
    delay->PopulateUIState(&ui, output);
    ui.m_scopeWriter.AcceptSamples();
    constexpr size_t x_points = 1024;
    std::array<float, x_points> stopped{};
    ScopeReader stoppedReader(&ui.m_scopeWriter, 0, 0, x_points);
    DOCTEST_REQUIRE_FALSE(stoppedReader.m_empty);
    for (size_t point = 0; point < x_points; ++point)
    {
        stopped[point] = stoppedReader.Get(static_cast<double>(point));
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        ScopeReader reader(&ui.m_scopeWriter, channel, 0, x_points);
        DOCTEST_CHECK(reader.Get(5 * x_points / 16) == doctest::Approx(0.5));
        DOCTEST_CHECK(reader.Get(3 * x_points / 4) == doctest::Approx(0.0));
    }

    // Starting resets the musical phase. The displayed waveform must keep its
    // coordinate origin, so that reset cannot draw a false segment across it.
    //
    const double stoppedWrite = output.m_writeHeadPhase;
    clock.SetRunning(true);
    clock.AdvanceSample();
    setter.Process(input, output);
    delay->PopulateUIState(&ui, output);
    ui.m_scopeWriter.AcceptSamples();
    DOCTEST_CHECK(output.m_writeHeadPhase == stoppedWrite);
    DOCTEST_CHECK(ui.m_writeHeadPhase.load() == stoppedWrite);
    ScopeReader restarted(&ui.m_scopeWriter, 0, 0, x_points);
    DOCTEST_CHECK(restarted.m_transferXSample == stoppedReader.m_transferXSample);
    for (size_t point = 0; point < x_points; ++point)
    {
        DOCTEST_REQUIRE(restarted.Get(static_cast<double>(point)) == stopped[point]);
    }

    for (bool running : {true, false, true, false})
    {
        const float previousDisplay = output.m_relativeWriteHeadPosition;
        const double previousWrite = output.m_writeHeadPhase;
        clock.SetRunning(running);
        clock.AdvanceSample();
        setter.Process(input, output);
        const double movement = output.m_writeHeadPhase - previousWrite;
        DOCTEST_CHECK(output.m_relativeWriteHeadPosition == doctest::Approx(previousDisplay + movement).epsilon(1e-6));
    }

    // Tempo edits while stopped change the sweep rate without moving the
    // display origin or changing the real-time write speed.
    //
    const double previousWrite = output.m_writeHeadPhase;
    clock.SetGlobalPeriodSamples(2.0 * x_period * SampleTimer::x_oversample);
    for (size_t sample = 0; sample < 1024 * SampleTimer::x_oversample; ++sample)
    {
        clock.AdvanceSample();
        setter.Process(input, output);
    }

    DOCTEST_CHECK(output.m_writeHeadPhase - previousWrite == doctest::Approx(1024.0 / (2.0 * x_period)));
}

DOCTEST_TEST_CASE("Delay heads: sparse audio under the visualized heads reaches grain playback")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    clock.SetRunning(true);
    clock.AdvanceSample();
    constexpr size_t x_period = 65536;
    // A host-rate inverter, recorded once per written sample.
    //
    using Storage = DelayLineMovableWriterGeneric<float, 4, x_period, 1>;
    auto storage = std::make_unique<Storage>();
    auto inverter = std::make_unique<Storage::Inverter>(x_period);
    storage->m_inverter = inverter.get();
    auto grains = std::make_unique<QuadGrainManager<Storage>>(storage.get());
    QuadGrainManager<Storage>::Input grainInput;
    ScopeWriterGeneric<4, 1> envelope(1, x_period);
    envelope.SetTimeWarp(0, inverter.get());
    clock.Get()->m_phaseClock.m_phase = 3.375;
    const double writePhase = clock.Get()->m_phaseClock.m_phase;
    const double speeds[] = {1, 0.5, 2, -1};
    std::array<ReadTapeHead, 4> readers;
    QuadDouble audioHeads;
    QuadDouble displayedHeads;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        ReadTapeHead::Input input;
        input.m_theoryOfTime = clock.Get();
        input.m_bufferFraction = 0.75;
        input.m_readHeadSpeed = speeds[channel];
        readers[channel].Update(input);
        audioHeads[channel] = readers[channel].m_phase;
        displayedHeads[channel] = readers[channel].m_phase;
    }

    // Record one sparse tone burst per channel at the position shown by its
    // read head. The display composes the same inverse with a magnitude scope.
    //
    for (size_t sample = 0; sample < x_period; ++sample)
    {
        const double position = writePhase - 1.0 + static_cast<double>(sample) / x_period;
        QuadFloat audio;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            const double offset = (position - displayedHeads[channel]) * x_period;
            audio[channel] = -1024 <= offset && offset < 2048
                ? 0.2f * std::cos(2.0 * M_PI * 18.0 * offset / 4096.0)
                : 0.0f;
        }

        inverter->Write(position, static_cast<double>(sample + 1));
        storage->Write(audio, sample + 1);
        if (sample == 0)
        {
            for (size_t channel = 0; channel < 4; ++channel)
            {
                envelope.SetPeriodic(0, channel, {0.0, 1.0});
            }
        }

        envelope.Write(0, audio.Abs());
        envelope.AdvanceIndex();
    }

    envelope.Publish();
    envelope.AcceptSamples();
    for (size_t channel = 0; channel < 4; ++channel)
    {
        ScopeReader reader(&envelope, channel, 0, 1024);
        DOCTEST_REQUIRE_FALSE(reader.m_empty);
        DOCTEST_REQUIRE(reader.ReadCoordinate(displayedHeads[channel]) > 0.1f);
    }

    std::array<double, 4> energy{};
    for (size_t sample = 0; sample < 8192 * SampleTimer::x_oversample; ++sample)
    {
        QuadFloat output = grains->Process(audioHeads, QuadFloat(), grainInput, storage->GetLatestSampleTime());
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
