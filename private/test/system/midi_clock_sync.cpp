#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "../support/SystemFixture.hpp"
#include "../support/SynthRig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "MidiToMessageIn.hpp"

namespace
{
    static constexpr int x_midiClocksPerMasterLoop = 24;

    SmartGrid::MessageIn RealtimeMessage(SmartGrid::MessageIn::Mode mode)
    {
        return SmartGrid::MessageIn(0, SmartGrid::MidiToMessageIn::x_realtimeRouteId, mode, 0, 0);
    }

    AudioInputBuffer EmptyAudioInput()
    {
        AudioInputBuffer audioInput;
        audioInput.m_numInputs = 0;
        return audioInput;
    }

    double CircularDistance(double a, double b)
    {
        double delta = std::abs(a - b);
        return std::min(delta, 1.0 - delta);
    }

    void AdvanceSample(synthrig::SynthRig& rig, SmartGrid::MessageIn::Mode realtimeMode)
    {
        if (SampleTimer::IncrementSample())
        {
            rig.Quad().ProcessFrame();
            rig.Internal().ProcessFrame();
            rig.IoThread().Acknowledge();
        }

        if (realtimeMode != SmartGrid::MessageIn::Mode::NoMessage)
        {
            rig.Internal().Apply(RealtimeMessage(realtimeMode));
        }

        AudioInputBuffer audioInput = EmptyAudioInput();
        rig.Quad().ProcessSample();
        rig.Internal().ProcessSample(audioInput);
    }

    void AdvanceSample(synthrig::SynthRig& rig)
    {
        AdvanceSample(rig, SmartGrid::MessageIn::Mode::NoMessage);
    }

    struct ClockTickObservation
    {
        double m_phaseError;
        double m_freqOut;
        double m_selectedPhase;
        double m_expectedSelectedPhase;
    };

    ClockTickObservation AdvanceClockTick(synthrig::SynthRig& rig)
    {
        if (SampleTimer::IncrementSample())
        {
            rig.Quad().ProcessFrame();
            rig.Internal().ProcessFrame();
            rig.IoThread().Acknowledge();
        }

        TheNonagonSquiggleBoyInternal& system = rig.Internal();
        size_t j = static_cast<size_t>(SampleTimer::GetUBlockIndex());
        double phase = system.m_nonagon.m_nonagon.m_theoryOfTime.GetUnwoundMasterIndependent(j);
        int loopIndex = system.ExternalClockLoopIndex();
        double multiplier = system.m_nonagon.m_nonagon.m_theoryOfTime.GetLoopExternalMultiplier(j, loopIndex);
        double selectedPhase = phase * multiplier;
        selectedPhase = selectedPhase - std::floor(selectedPhase);
        double expectedSelectedPhase = static_cast<double>(system.m_clockSynchronizer.m_ticksOffset) /
            static_cast<double>(x_midiClocksPerMasterLoop);

        system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiClock));

        AudioInputBuffer audioInput = EmptyAudioInput();
        rig.Quad().ProcessSample();
        rig.Internal().ProcessSample(audioInput);

        return ClockTickObservation{
            CircularDistance(selectedPhase, expectedSelectedPhase),
            system.m_clockSynchronizer.m_freqOut,
            selectedPhase,
            expectedSelectedPhase
        };
    }

    struct DeterministicRng
    {
        uint32_t m_state;

        explicit DeterministicRng(uint32_t seed)
            : m_state(seed)
        {
        }

        uint32_t Next()
        {
            m_state = m_state * 1664525u + 1013904223u;
            return m_state;
        }

        int Int(int lo, int hi)
        {
            uint32_t span = static_cast<uint32_t>(hi - lo + 1);
            return lo + static_cast<int>(Next() % span);
        }
    };

    void AdvanceSamples(synthrig::SynthRig& rig, int samples)
    {
        for (int i = 0; i < samples; ++i)
        {
            AdvanceSample(rig);
        }
    }

    void SetExternalClockLoopSwitch(TheNonagonSquiggleBoyInternal& system, int switchVal)
    {
        using Param = SmartGridOneEncoders::Param;

        float value = static_cast<float>(switchVal) /
            static_cast<float>(TheoryOfTimeBase::x_numLoops - 1);
        SmartGrid::BankedEncoderCell* cell =
            system.m_squiggleBoy.m_encoders.m_encoderBankBank.GetEncoder(static_cast<size_t>(Param::ExternalClockLoop));

        DOCTEST_REQUIRE(cell != nullptr);
        cell->SetValueAllScenesAllTracks(value);
        cell->InitSlewState(value);
        for (size_t i = 0; i < 16; ++i)
        {
            cell->m_output[i] = value;
        }
    }

    void ConfigureTheoryTopology(TheNonagonSquiggleBoyInternal& system, DeterministicRng& rng)
    {
        for (int loop = 0; loop < TheoryOfTimeBase::x_masterLoop; ++loop)
        {
            int parentIndex = rng.Int(loop + 1, TheoryOfTimeBase::x_masterLoop);
            int parentMult = rng.Int(1, 5);

            system.m_nonagon.m_state.m_theoryOfTimeInput.m_input[loop].m_parentIndex = parentIndex;
            system.m_nonagon.m_state.m_theoryOfTimeInput.m_input[loop].m_parentMult = parentMult;
        }

        system.m_nonagon.m_state.m_theoryOfTimeInput.m_input[TheoryOfTimeBase::x_masterLoop].m_parentIndex =
            TheoryOfTimeBase::x_numLoops;
        system.m_nonagon.m_state.m_theoryOfTimeInput.m_input[TheoryOfTimeBase::x_masterLoop].m_parentMult = 1;
    }

    void SettleTheoryTopology(synthrig::SynthRig& rig)
    {
        TheNonagonSquiggleBoyInternal& system = rig.Internal();

        system.SetExternalClock(false);
        system.m_running = true;
        AdvanceSamples(rig, 4096);
        system.m_running = false;
        AdvanceSamples(rig, 64);
    }

    void ConfigureExternalClockTrial(synthrig::SynthRig& rig, DeterministicRng& rng, int loopIndex)
    {
        TheNonagonSquiggleBoyInternal& system = rig.Internal();
        int switchVal = TheoryOfTimeBase::x_numLoops - loopIndex - 1;

        SetExternalClockLoopSwitch(system, switchVal);
        ConfigureTheoryTopology(system, rng);
        SettleTheoryTopology(rig);

        system.SetExternalClock(true);
        system.m_running = false;
        system.m_clockTick = false;
        system.m_clockSynchronizer = ExternalClockSync();

        DOCTEST_CHECK(system.ExternalClockLoopIndex() == loopIndex);
    }

    void CheckExternalClockStartTrial(int trial, int startOffset, int clockInterval, int loopIndex, DeterministicRng& rng, double phaseErrorBound)
    {
        synthrig::SynthRig rig;
        TheNonagonSquiggleBoyInternal& system = rig.Internal();

        ConfigureExternalClockTrial(rig, rng, loopIndex);

        for (int i = 0; i < 8; ++i)
        {
            AdvanceSamples(rig, clockInterval - 1);
            ClockTickObservation observation = AdvanceClockTick(rig);
            DOCTEST_CHECK(observation.m_freqOut > 0.0);
        }

        AdvanceSamples(rig, startOffset);
        AdvanceSample(rig, SmartGrid::MessageIn::Mode::MidiStart);
        AdvanceSamples(rig, clockInterval - startOffset - 1);

        ClockTickObservation armedObservation = AdvanceClockTick(rig);
        DOCTEST_CHECK(system.m_clockSynchronizer.m_state == ExternalClockSync::State::Running);
        DOCTEST_CHECK(armedObservation.m_freqOut > 0.0);

        for (int i = 1; i <= x_midiClocksPerMasterLoop * 2; ++i)
        {
            AdvanceSamples(rig, clockInterval - 1);
            ClockTickObservation observation = AdvanceClockTick(rig);

            DOCTEST_CAPTURE(trial);
            DOCTEST_CAPTURE(i);
            DOCTEST_CAPTURE(clockInterval);
            DOCTEST_CAPTURE(startOffset);
            DOCTEST_CAPTURE(loopIndex);
            DOCTEST_CAPTURE(observation.m_selectedPhase);
            DOCTEST_CAPTURE(observation.m_expectedSelectedPhase);
            DOCTEST_CAPTURE(system.m_clockSynchronizer.m_ticksOffset);
            DOCTEST_CHECK(observation.m_freqOut > 0.0);
            DOCTEST_CHECK(observation.m_phaseError < phaseErrorBound);
        }
    }

    void CheckExternalClockJitterStartTrial(int trial, int startOffset, int clockInterval, int loopIndex, DeterministicRng& rng)
    {
        synthrig::SynthRig rig;
        TheNonagonSquiggleBoyInternal& system = rig.Internal();

        ConfigureExternalClockTrial(rig, rng, loopIndex);

        for (int i = 0; i < 8; ++i)
        {
            int jitteredInterval = clockInterval + rng.Int(-clockInterval / 20, clockInterval / 20);
            AdvanceSamples(rig, jitteredInterval - 1);
            ClockTickObservation observation = AdvanceClockTick(rig);
            DOCTEST_CHECK(observation.m_freqOut > 0.0);
        }

        AdvanceSamples(rig, startOffset);
        AdvanceSample(rig, SmartGrid::MessageIn::Mode::MidiStart);
        AdvanceSamples(rig, clockInterval - startOffset - 1);

        ClockTickObservation armedObservation = AdvanceClockTick(rig);
        DOCTEST_CHECK(system.m_clockSynchronizer.m_state == ExternalClockSync::State::Running);
        DOCTEST_CHECK(armedObservation.m_freqOut > 0.0);

        for (int i = 1; i <= x_midiClocksPerMasterLoop * 2; ++i)
        {
            int jitteredInterval = clockInterval + rng.Int(-clockInterval / 20, clockInterval / 20);
            AdvanceSamples(rig, jitteredInterval - 1);
            ClockTickObservation observation = AdvanceClockTick(rig);

            DOCTEST_CAPTURE(trial);
            DOCTEST_CAPTURE(i);
            DOCTEST_CAPTURE(jitteredInterval);
            DOCTEST_CAPTURE(clockInterval);
            DOCTEST_CAPTURE(startOffset);
            DOCTEST_CAPTURE(loopIndex);
            DOCTEST_CAPTURE(observation.m_selectedPhase);
            DOCTEST_CAPTURE(observation.m_expectedSelectedPhase);
            DOCTEST_CAPTURE(system.m_clockSynchronizer.m_ticksOffset);
            DOCTEST_CHECK(observation.m_freqOut > 0.0);
            DOCTEST_CHECK(observation.m_phaseError < 0.02);
        }
    }
}

DOCTEST_TEST_CASE("MIDI clock sync transport messages update internal running state")
{
    GlobalEnv::ResetPerTest();
    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();

    system.SetExternalClock(false);
    system.m_running = false;
    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiStart));
    DOCTEST_CHECK(system.m_running);

    system.m_running = false;
    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiContinue));
    DOCTEST_CHECK(system.m_running);

    system.m_running = true;
    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiStop));
    DOCTEST_CHECK_FALSE(system.m_running);
}

DOCTEST_TEST_CASE("MIDI clock sync external transport arms until clock")
{
    GlobalEnv::ResetPerTest();
    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();
    AudioInputBuffer audioInput = EmptyAudioInput();

    system.SetExternalClock(true);
    system.m_running = false;
    system.m_clockTick = false;
    system.m_clockSynchronizer = ExternalClockSync();

    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiStart));

    DOCTEST_CHECK(system.m_running == false);
    DOCTEST_CHECK(system.m_clockSynchronizer.m_state == ExternalClockSync::State::Armed);

    system.ProcessSample(audioInput);

    DOCTEST_CHECK(system.m_running == false);
    DOCTEST_CHECK(system.m_nonagon.m_state.m_running == false);

    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiClock));
    system.ProcessSample(audioInput);

    DOCTEST_CHECK(system.m_clockSynchronizer.m_state == ExternalClockSync::State::Running);
    DOCTEST_CHECK(system.m_running == true);
    DOCTEST_CHECK(system.m_nonagon.m_state.m_running == true);

    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiStop));

    DOCTEST_CHECK(system.m_running == true);
    DOCTEST_CHECK(system.m_clockSynchronizer.m_state == ExternalClockSync::State::Stopped);

    system.ProcessSample(audioInput);

    DOCTEST_CHECK(system.m_running == false);
    DOCTEST_CHECK(system.m_nonagon.m_state.m_running == false);
}

DOCTEST_TEST_CASE("MIDI clock sync armed clock does not update tempo")
{
    GlobalEnv::ResetPerTest();
    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();
    AudioInputBuffer audioInput = EmptyAudioInput();

    system.SetExternalClock(true);
    system.m_running = false;
    system.m_clockTick = false;
    system.m_clockSynchronizer = ExternalClockSync();
    system.m_clockSynchronizer.m_freqOut = 0.000003;

    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiStart));

    for (int i = 0; i < 2; ++i)
    {
        system.ProcessSample(audioInput);
    }

    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiClock));
    system.ProcessSample(audioInput);

    DOCTEST_CHECK(system.m_clockSynchronizer.m_state == ExternalClockSync::State::Running);
    DOCTEST_CHECK(system.m_clockSynchronizer.m_freqOut == doctest::Approx(0.000003));
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq == doctest::Approx(0.000003));
}

DOCTEST_TEST_CASE("MIDI clock sync consumes tick for one sample")
{
    GlobalEnv::ResetPerTest();
    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();
    AudioInputBuffer audioInput = EmptyAudioInput();

    system.SetExternalClock(true);
    system.m_clockTick = false;
    system.m_clockSynchronizer = ExternalClockSync();

    system.ProcessSample(audioInput);

    system.Apply(RealtimeMessage(SmartGrid::MessageIn::Mode::MidiClock));
    DOCTEST_CHECK(system.m_clockTick);

    system.ProcessSample(audioInput);
    DOCTEST_CHECK_FALSE(system.m_clockTick);
    DOCTEST_CHECK(system.m_clockSynchronizer.m_samplesSinceLastClock == 0);

    system.ProcessSample(audioInput);
    DOCTEST_CHECK_FALSE(system.m_clockTick);
    DOCTEST_CHECK(system.m_clockSynchronizer.m_samplesSinceLastClock == 1);
}

DOCTEST_TEST_CASE("MIDI clock sync external mode overrides tempo parameter")
{
    GlobalEnv::ResetPerTest();
    TheNonagonSquiggleBoyInternal& system = SystemFixture::SharedSystem();

    system.m_running = false;
    system.m_clockTick = false;
    system.m_squiggleBoyState.m_tempo.m_expParam = 0.001;
    system.m_clockSynchronizer = ExternalClockSync();
    system.m_clockSynchronizer.m_freqOut = 0.002;

    system.SetExternalClock(false);
    system.SetNonagonInputs();
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq == doctest::Approx(0.001));

    system.SetExternalClock(true);
    system.SetNonagonInputs();
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq == doctest::Approx(0.002));

    system.m_clockSynchronizer.m_freqOut = 0.0;
    system.SetNonagonInputs();
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq == std::numeric_limits<double>::denorm_min());
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq != doctest::Approx(0.001));

    system.m_clockSynchronizer.m_freqOut = -0.001;
    system.SetNonagonInputs();
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq == std::numeric_limits<double>::denorm_min());

    system.SetExternalClock(false);
    system.m_clockSynchronizer = ExternalClockSync();
}

DOCTEST_TEST_CASE("MIDI clock sync stabilizes to constant tempo and phase over many cycles")
{
    synthrig::SynthRig rig;
    TheNonagonSquiggleBoyInternal& system = rig.Internal();

    static constexpr int x_samplesPerClockTick = 40;
    static constexpr int x_numMasterCycles = 128;
    static constexpr int x_numClockTicks = x_midiClocksPerMasterLoop * x_numMasterCycles;
    static constexpr double x_expectedFreq =
        1.0 / static_cast<double>(x_samplesPerClockTick * x_midiClocksPerMasterLoop);

    system.SetExternalClock(true);
    system.m_clockTick = false;
    system.m_clockSynchronizer = ExternalClockSync();
    system.m_squiggleBoyState.m_tempo.m_expParam = 1.0 / 257.0;

    AdvanceSample(rig, SmartGrid::MessageIn::Mode::MidiStart);

    std::vector<double> finalCycleFreqs;
    std::vector<double> finalCyclePhaseErrors;

    for (int tickIx = 0; tickIx < x_numClockTicks; ++tickIx)
    {
        for (int sampleIx = 1; sampleIx < x_samplesPerClockTick; ++sampleIx)
        {
            AdvanceSample(rig);
        }

        ClockTickObservation observation = AdvanceClockTick(rig);

        if (tickIx >= x_numClockTicks - x_midiClocksPerMasterLoop)
        {
            finalCyclePhaseErrors.push_back(observation.m_phaseError);
            finalCycleFreqs.push_back(observation.m_freqOut);
        }
    }

    auto minMaxFreq = std::minmax_element(finalCycleFreqs.begin(), finalCycleFreqs.end());
    double maxPhaseError = *std::max_element(finalCyclePhaseErrors.begin(), finalCyclePhaseErrors.end());

    DOCTEST_REQUIRE(finalCycleFreqs.size() == x_midiClocksPerMasterLoop);
    DOCTEST_REQUIRE(finalCyclePhaseErrors.size() == x_midiClocksPerMasterLoop);
    DOCTEST_CHECK(system.m_nonagon.m_state.m_theoryOfTimeInput.m_freq == doctest::Approx(x_expectedFreq).epsilon(0.001));
    DOCTEST_CHECK(system.m_clockSynchronizer.m_freqOut == doctest::Approx(x_expectedFreq).epsilon(0.001));
    DOCTEST_CHECK((*minMaxFreq.second - *minMaxFreq.first) < x_expectedFreq * 0.001);
    DOCTEST_CHECK(maxPhaseError < 0.001);
}

DOCTEST_TEST_CASE("MIDI clock sync randomized starts preserve phase at fixed external tempo")
{
    DeterministicRng rng(0x5eed1234u);

    for (int trial = 0; trial < 18; ++trial)
    {
        int clockInterval = rng.Int(160, 1440);
        int startOffset = rng.Int(1, clockInterval - 2);
        int loopIndex = trial % TheoryOfTimeBase::x_numLoops;

        CheckExternalClockStartTrial(trial, startOffset, clockInterval, loopIndex, rng, 0.001);
    }
}

DOCTEST_TEST_CASE("MIDI clock sync randomized starts stay bounded with five percent jitter")
{
    DeterministicRng rng(0x51a7e005u);

    for (int trial = 0; trial < 18; ++trial)
    {
        int clockInterval = rng.Int(160, 1440);
        int startOffset = rng.Int(1, clockInterval - 2);
        int loopIndex = (trial * 5 + 2) % TheoryOfTimeBase::x_numLoops;

        CheckExternalClockJitterStartTrial(trial, startOffset, clockInterval, loopIndex, rng);
    }
}
