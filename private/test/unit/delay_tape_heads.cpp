#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "../support/SynthRig.hpp"
#include "../support/TimeRig.hpp"
#include "QuadDelay.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>

DOCTEST_TEST_CASE("Delay heads: audio and display share a position at every speed and loop")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    clock.SetRunning(true);
    clock.AdvanceSample();
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    QuadDelay::Input output;
    input.m_theoryOfTime = clock.Get();
    for (auto& loop : clock.Get()->m_sample.m_loops)
    {
        loop.m_modulatedCycleCrossed = true;
    }

    const double speeds[] =
    {
        -4, -3, -2, -1.5, -4.0 / 3.0, -1, -0.5, -0.25,
        0.25, 0.5, 1, 4.0 / 3.0, 1.5, 2, 3, 4
    };
    for (int loop = 0; loop < TheoryOfTimeBase::x_numLoops; ++loop)
    {
        input.m_loopSelectorSwitchVal[0] = static_cast<int>(TheoryOfTimeBase::x_globalLoop) - loop;
        for (double phase : {-2.375, 0.0, 3.375, 128.125})
        {
            for (size_t setting = 0; setting < std::size(speeds); ++setting)
            {
                const double speed = speeds[setting];
                DOCTEST_CAPTURE(loop);
                DOCTEST_CAPTURE(phase);
                DOCTEST_CAPTURE(speed);
                clock.Get()->m_phaseClock.m_phase = phase;
                input.m_readHeadSpeedSwitchVal[0] = static_cast<int>(setting);
                setter.Process(input, output);
                const double period = 1.0 / static_cast<double>(clock.Get()->GetCycleRatio(loop));
                const double read = output.m_readHeadPhase[0];
                DOCTEST_CHECK(read >= phase - period);
                DOCTEST_CHECK(read < phase);
                DOCTEST_CHECK(output.m_relativeReadHeadPosition[0] == static_cast<float>(read - std::floor(read)));
                double wrapped = (read - (phase * speed - period * 0.75)) / period;
                DOCTEST_CHECK(wrapped == doctest::Approx(std::round(wrapped)));
                clock.Get()->m_globalPeriodSamples *= 2.0;
                setter.Process(input, output);
                DOCTEST_CHECK(output.m_readHeadPhase[0] == read);
                clock.Get()->m_phaseClock.Resume(-700.25);
                setter.Process(input, output);
                DOCTEST_CHECK(output.m_readHeadPhase[0] == read);
            }
        }
    }
}

DOCTEST_TEST_CASE("Delay heads: packed positions retain the scalar wrap at long phases and seams")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    QuadDelay::Input output;
    input.m_theoryOfTime = clock.Get();
    const int speedSettings[] = {11, 0, 7, 13};
    const double speeds[] = {4.0 / 3.0, -4.0, -0.25, 2.0};
    const int loops[] = {0, 1, 2, TheoryOfTimeBase::x_globalLoop};
    const int64_t ratios[] = {3, 5, 7, 1};
    input.m_delayTimeFactorKnob = QuadFloat(0.625f, 0.125f, 0.0f, 0.75f);
    for (int lane = 0; lane < 4; ++lane)
    {
        clock.Get()->m_sample.m_loops[loops[lane]].m_cycleRatio = ratios[lane];
        clock.Get()->m_sample.m_loops[loops[lane]].m_modulatedCycleCrossed = true;
        input.m_loopSelectorSwitchVal[lane] = TheoryOfTimeBase::x_globalLoop - loops[lane];
        input.m_readHeadSpeedSwitchVal[lane] = speedSettings[lane];
    }

    const double phases[] =
    {
        -1150096798.1511078,
        std::nextafter(0.75, 0.0),
        0.75,
        std::nextafter(0.75, 1.0),
        1150096798.1511078
    };
    for (double write : phases)
    {
        clock.Get()->m_phaseClock.m_phase = write;
        setter.Process(input, output);
        for (int lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(lane);
            DOCTEST_CAPTURE(write);
            const double period = 1.0 / static_cast<double>(ratios[lane]);
            const double fraction = input.m_delayTimeFactorKnob[lane] == 0.0f ? 1.0 : input.m_delayTimeFactorKnob[lane];
            const double projected = write * speeds[lane] - period * fraction;
            const double expected = PhaseUtils::WrapMod(write - period, write, projected);
            DOCTEST_CHECK(output.m_readHeadPhase[lane] == expected);
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

DOCTEST_TEST_CASE("Delay heads: FACT follows a mid-loop sweep and its endpoints agree")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    TheoryOfTimeBase::Input clock;
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = &time;
    QuadDelay::Input output;

    clock.m_running = true;
    clock.m_unmodulatedPhase = 0.25;
    time.TheoryOfTimeBase::Process(clock);
    setter.Process(input, output);
    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CHECK(output.m_readHeadPhase[channel] == output.m_writeHeadPhase - 0.75);
    }

    for (size_t step = 0; step < 8; ++step)
    {
        float fraction = 0.25f + static_cast<float>(step) / 64.0f;
        input.m_delayTimeFactorKnob = QuadFloat(0.0f, 1.0f, fraction, 0.75f - fraction);
        clock.m_unmodulatedPhase = 0.5 + static_cast<double>(step) / 64.0;
        time.TheoryOfTimeBase::Process(clock);
        DOCTEST_REQUIRE_FALSE(time.CrossedCycleBoundary(TheoryOfTimeBase::x_globalLoop, PhaseDomain::Modulated));
        setter.Process(input, output);
        DOCTEST_CHECK(output.m_readHeadPhase[0] == output.m_writeHeadPhase - 1.0);
        DOCTEST_CHECK(output.m_readHeadPhase[1] == output.m_readHeadPhase[0]);
        DOCTEST_CHECK(output.m_readHeadPhase[2] == output.m_writeHeadPhase - fraction);
        DOCTEST_CHECK(output.m_readHeadPhase[3] == output.m_writeHeadPhase - (0.75f - fraction));
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

DOCTEST_TEST_CASE("Delay scope: each lane publishes the period of its accepted loop")
{
    synthrig::SynthRig rig;
    auto& internal = rig.Internal();
    auto& synth = internal.m_squiggleBoy;
    auto& time = *synth.m_theoryOfTime;
    auto& writer = rig.UIState().m_squiggleBoyUIState.m_delayUIState.m_scopeWriter;
    QuadDelayInputSetter::Input input;
    input.m_theoryOfTime = &time;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        input.m_loopSelectorSwitchVal[channel] = static_cast<int>(channel);
    }

    for (size_t crossing = 0; crossing < 4; ++crossing)
    {
        rig.RunFrames(1);
        for (size_t loop = 0; loop < TheoryOfTimeBase::x_numLoops; ++loop)
        {
            const bool global = loop == TheoryOfTimeBase::x_globalLoop;
            time.m_sample.m_loops[loop].m_modulatedCycleCrossed = crossing == 3
                || (crossing == 1 && global) || (crossing == 2 && !global);
        }

        synth.m_delayInputSetter.Process(input, synth.m_delayState);
        internal.PopulateUIState();
        writer.AcceptSamples();
        for (size_t channel = 0; channel < 4; ++channel)
        {
            size_t loop = TheoryOfTimeBase::x_globalLoop - (crossing == 3 ? channel : 0);
            const auto& axis = writer.GetAcceptedMarkers(0, channel).m_periodic;
            DOCTEST_CHECK(axis.m_origin == 0.0);
            DOCTEST_CHECK(axis.m_period == doctest::Approx(1.0 / time.GetCycleRatio(loop)));
        }
    }

    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 3).m_periodic.m_period < 1.0);
}

DOCTEST_TEST_CASE("Delay heads: sparse audio under the visualized heads reaches grain playback")
{
    GlobalEnv::ResetPerTest();
    TimeRig clock;
    constexpr size_t x_period = 65536;
    auto delay = std::make_unique<QuadDelay>();
    auto& inverter = *clock.Get()->m_timeWarpInverter;
    delay->m_delayLine.m_inverter = &inverter;
    QuadDelay::UIState ui;
    delay->SetupScopeWriter(&ui);

    clock.Get()->m_phaseClock.m_phase = 3.375;
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input controls;
    controls.m_theoryOfTime = clock.Get();
    const int speedSettings[] = {10, 9, 13, 5};
    std::copy_n(speedSettings, 4, controls.m_readHeadSpeedSwitchVal);
    QuadDelay::Input input;
    setter.Process(controls, input);
    delay->PopulateUIState(&ui, input);
    const double writePhase = ui.m_writeHeadPhase.load();
    QuadDouble displayedHeads;
    for (size_t channel = 0; channel < 4; ++channel)
    {
        displayedHeads[channel] = ui.m_readHeadPhase[channel].load();
        DOCTEST_CHECK(displayedHeads[channel] == input.m_readHeadPhase[channel]);
    }

    // Record one sparse tone burst per channel up to the position shown by its
    // read head. The display composes the same inverse with a magnitude scope.
    //
    for (size_t sample = 0; sample < x_period; ++sample)
    {
        const double position = writePhase - 1.0 + static_cast<double>(sample) / x_period;
        QuadFloat audio;
        for (size_t channel = 0; channel < 4; ++channel)
        {
            const double offset = (position - displayedHeads[channel]) * x_period;
            audio[channel] = -2560 <= offset && offset < 64
                ? 0.2f * std::cos(2.0 * M_PI * 18.0 * offset / 4096.0)
                : 0.0f;
        }

        for (size_t subSample = 0; subSample < SampleTimer::x_oversample; ++subSample)
        {
            SampleTimer::IncrementSample();
        }

        inverter.Write(position, static_cast<double>(SampleTimer::GetSample()));
        delay->WriteAudio(audio, SampleTimer::GetHostSample());
        for (size_t subSample = 0; subSample < SampleTimer::x_oversample; ++subSample)
        {
            delay->WriteEnvelopeScope();
            ui.m_scopeWriter.AdvanceIndex();
        }

        if (SampleTimer::GetSample() % SampleTimer::x_samplesPerProcessFrame == 0)
        {
            delay->PopulateUIState(&ui, input);
            ui.m_scopeWriter.AcceptSamples();
        }
    }

    delay->PopulateUIState(&ui, input);
    ui.m_scopeWriter.AcceptSamples();
    for (size_t channel = 0; channel < 4; ++channel)
    {
        ScopeReader reader(&ui.m_scopeWriter, channel, 0, 1024);
        DOCTEST_REQUIRE_FALSE(reader.m_empty);
        DOCTEST_REQUIRE(reader.ReadCoordinate(displayedHeads[channel]) > 0.1f);
    }

    std::array<double, 4> energy{};
    for (size_t sample = 0; sample < 8192 * SampleTimer::x_oversample; ++sample)
    {
        SampleTimer::IncrementSample();
        QuadFloat output = delay->Process(input);
        ui.m_scopeWriter.AdvanceIndex();
        for (size_t channel = 0; channel < 4; ++channel)
        {
            energy[channel] += output[channel] * output[channel];
        }
    }

    for (size_t channel = 0; channel < 4; ++channel)
    {
        DOCTEST_CAPTURE(channel);
        DOCTEST_CHECK(energy[channel] > 1.0);
    }
}
