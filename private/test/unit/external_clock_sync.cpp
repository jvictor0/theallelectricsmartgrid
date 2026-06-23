#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "../support/TimeRig.hpp"

#include "ExternalClockSync.hpp"

#include <type_traits>

DOCTEST_TEST_CASE("ExternalClockSync: constructor initializes defaults")
{
    GlobalEnv::ResetPerTest();

    ExternalClockSync sync;
    ExternalClockSync::Input input;

    DOCTEST_CHECK(sync.m_samplesSinceLastClock == 0);
    DOCTEST_CHECK(sync.m_ticksOffset == 0);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(ExternalClockSync::x_defaultFreqOut));
    DOCTEST_CHECK(sync.EffectiveFreq() == doctest::Approx(ExternalClockSync::x_defaultFreqOut));
    DOCTEST_CHECK(sync.m_state == ExternalClockSync::State::Stopped);
    DOCTEST_CHECK(sync.IsRunning() == false);

    DOCTEST_CHECK(input.m_theoryOfTime == nullptr);
    DOCTEST_CHECK(input.m_clockTick == false);
    DOCTEST_CHECK(input.m_ticksMultiplier == 24);
    DOCTEST_CHECK(input.m_loopIndex == TheoryOfTimeBase::x_masterLoop);
}

DOCTEST_TEST_CASE("ExternalClockSync: frequency output is double precision")
{
    ExternalClockSync sync;

    DOCTEST_CHECK(std::is_same_v<decltype(sync.m_freqOut), double>);
    DOCTEST_CHECK(std::is_same_v<decltype(sync.EffectiveFreq()), double>);
    DOCTEST_CHECK(std::is_same_v<decltype(ExternalClockSync::x_defaultFreqOut), double const>);
}

DOCTEST_TEST_CASE("ExternalClockSync: start arms until the next clock tick")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    ExternalClockSync sync;

    ExternalClockSync::Input input;
    input.m_theoryOfTime = rig.Get();

    for (int i = 0; i < 40; ++i)
    {
        sync.Process(input);
    }

    input.m_clockTick = true;
    sync.Process(input);
    double stoppedEstimate = sync.m_freqOut;

    input.m_clockTick = false;
    for (int i = 0; i < 20; ++i)
    {
        sync.Process(input);
    }

    sync.Start();

    DOCTEST_CHECK(sync.m_state == ExternalClockSync::State::Armed);
    DOCTEST_CHECK(sync.IsRunning() == false);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(stoppedEstimate));

    input.m_clockTick = true;
    sync.Process(input);

    DOCTEST_CHECK(sync.m_state == ExternalClockSync::State::Running);
    DOCTEST_CHECK(sync.IsRunning() == true);
    DOCTEST_CHECK(sync.m_ticksOffset == 1);
    DOCTEST_CHECK(sync.m_samplesSinceLastClock == 0);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(stoppedEstimate));
}

DOCTEST_TEST_CASE("ExternalClockSync: armed clock does not update tempo from a short start interval")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    ExternalClockSync sync;

    ExternalClockSync::Input input;
    input.m_theoryOfTime = rig.Get();

    for (int i = 0; i < 40; ++i)
    {
        sync.Process(input);
    }

    input.m_clockTick = true;
    sync.Process(input);
    double stoppedEstimate = sync.m_freqOut;

    input.m_clockTick = false;
    for (int i = 0; i < 2; ++i)
    {
        sync.Process(input);
    }

    sync.Start();

    input.m_clockTick = true;
    sync.Process(input);

    DOCTEST_CHECK(sync.m_state == ExternalClockSync::State::Running);
    DOCTEST_CHECK(sync.m_ticksOffset == 1);
    DOCTEST_CHECK(sync.m_samplesSinceLastClock == 0);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(stoppedEstimate));
}

DOCTEST_TEST_CASE("ExternalClockSync: steady tick after 24 samples produces positive estimate")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    ExternalClockSync sync;
    ExternalClockSync::Input input;
    input.m_theoryOfTime = rig.Get();
    sync.Start();
    input.m_clockTick = true;
    sync.Process(input);
    input.m_clockTick = false;

    for (int i = 0; i < 24; ++i)
    {
        sync.Process(input);
    }

    input.m_clockTick = true;
    sync.Process(input);

    DOCTEST_CHECK(sync.m_freqOut > 0.0);
}

DOCTEST_TEST_CASE("ExternalClockSync: stopped estimate uses selected loop multiplier")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    rig.AdvanceControlFrame();

    ExternalClockSync sync;
    ExternalClockSync::Input input;
    input.m_theoryOfTime = rig.Get();
    input.m_loopIndex = 1;

    for (int i = 0; i < 40; ++i)
    {
        sync.Process(input);
    }

    size_t j = static_cast<size_t>(SampleTimer::GetUBlockIndex());
    double multiplier = input.m_theoryOfTime->GetLoopExternalMultiplier(j, input.m_loopIndex);
    double expectedFreq = 1.0 / (static_cast<double>(input.m_ticksMultiplier) * multiplier * 40.0);

    input.m_clockTick = true;
    sync.Process(input);

    DOCTEST_CHECK(multiplier > 1.0);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(expectedFreq));
}

DOCTEST_TEST_CASE("ExternalClockSync: longer interval lowers external estimate")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    ExternalClockSync shortSync;
    ExternalClockSync longSync;
    ExternalClockSync::Input input;
    input.m_theoryOfTime = rig.Get();
    shortSync.Start();
    longSync.Start();
    input.m_clockTick = true;
    shortSync.Process(input);
    longSync.Process(input);
    input.m_clockTick = false;

    for (int i = 0; i < 24; ++i)
    {
        shortSync.Process(input);
    }

    input.m_clockTick = true;
    shortSync.Process(input);
    double shortEstimate = shortSync.m_freqOut;

    input.m_clockTick = false;
    for (int i = 0; i < 48; ++i)
    {
        longSync.Process(input);
    }

    input.m_clockTick = true;
    longSync.Process(input);

    DOCTEST_CHECK(longSync.m_freqOut < shortEstimate);
}

DOCTEST_TEST_CASE("ExternalClockSync: stop transition preserves counters and tempo")
{
    GlobalEnv::ResetPerTest();
    TimeRig rig;

    ExternalClockSync sync;

    ExternalClockSync::Input input;
    input.m_theoryOfTime = rig.Get();
    sync.Start();
    input.m_clockTick = true;
    sync.Process(input);
    input.m_clockTick = false;

    for (int i = 0; i < 10; ++i)
    {
        sync.Process(input);
    }

    input.m_clockTick = true;
    sync.Process(input);

    int samplesSince = sync.m_samplesSinceLastClock;
    int ticksOffset = sync.m_ticksOffset;
    double freqOut = sync.m_freqOut;

    sync.Stop();
    input.m_clockTick = false;
    sync.Process(input);

    DOCTEST_CHECK(sync.m_state == ExternalClockSync::State::Stopped);
    DOCTEST_CHECK(sync.IsRunning() == false);
    DOCTEST_CHECK(sync.m_ticksOffset == ticksOffset);
    DOCTEST_CHECK(sync.m_samplesSinceLastClock == samplesSince + 1);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(freqOut));
}
