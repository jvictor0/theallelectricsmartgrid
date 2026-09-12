#include "doctest.h"
#include "TheoryOfTime.hpp"
#include "PhasorPlayHead.hpp"
#include "IndexArp.hpp"
#include "RecordingBuffer.hpp"
#include "ExternalClockSync.hpp"
#include "../support/GlobalEnv.hpp"
#include <memory>

DOCTEST_TEST_CASE("AbsoluteTime: sample playback applies speed before wrapping")
{
    TheoryOfTime time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    PhasorPlayHead head;
    PhasorPlayHead::Input input;
    input.m_theoryOfTime = &time;
    input.m_loopIndex = TheoryOfTimeBase::x_globalLoop;
    input.m_sampleIndex = 1;
    input.m_speed = 0.5f;

    clock.m_unmodulatedPhase = 1.5;
    time.TheoryOfTimeBase::Process(1, clock);
    DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.75));
    input.m_speed = -0.5f;
    DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.25));
    input.m_speed = 0.25f;
    DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.375));
    input.m_speed = 0.0f;
    input.m_start = 0.2f;
    DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.2));
    input.m_speed = 0.5f;
    input.m_length = 0.4f;
    DOCTEST_CHECK(head.Process(input) == doctest::Approx(0.5));

    const float speeds[] = {-4, -3, -2, -1.5f, -4.0f / 3, -1, -0.5f, -0.25f,
        0, 0.25f, 0.5f, 1, 4.0f / 3, 1.5f, 2, 3, 4};
    const double phases[] = {-0.5, 0.5, 1.5, 2.5, 1048576.375};
    for (double phase : phases)
    {
        clock.m_unmodulatedPhase = phase;
        time.TheoryOfTimeBase::Process(1, clock);
        for (float speed : speeds)
        {
            input.m_speed = speed;
            double travel = phase * speed;
            double expected = input.m_start + input.m_length * (travel - std::floor(travel));
            expected -= std::floor(expected);
            DOCTEST_CHECK(head.Process(input) == doctest::Approx(expected));
        }
    }
}

DOCTEST_TEST_CASE("AbsoluteTime: PolyXFader partial lobes remain loop periodic")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    PolyXFaderInternal::Input lfo;
    lfo.m_theoryOfTime = &time;
    lfo.m_phaseDomain = PhaseDomain::Unmodulated;
    lfo.m_samplePosition = 1;
    lfo.m_mult = 2.5f;
    lfo.m_phaseShift = -0.75f;
    lfo.m_attackFrac = 0.5f;
    lfo.m_shape = 0.5f;
    clock.m_unmodulatedPhase = 0.9;
    time.Process(1, clock);
    float first = lfo.ComputePhase(5);
    DOCTEST_CHECK(first == doctest::Approx(0.5));
    clock.m_unmodulatedPhase = 1048576.9;
    time.Process(1, clock);
    DOCTEST_CHECK(lfo.ComputePhase(5) == doctest::Approx(first));
}

DOCTEST_TEST_CASE("AbsoluteTime: arp handles negative and wide step coordinates")
{
    IndexArp arp;
    IndexArp::Input input;
    input.m_clock = true;
    input.m_totalIndex = -1;
    arp.Process(input);
    DOCTEST_CHECK(arp.m_rhythmIndex == 7);
    DOCTEST_CHECK(arp.m_motiveIndex == -1);
    input.m_totalIndex = 4294967299LL;
    arp.Process(input);
    DOCTEST_CHECK(arp.m_rhythmIndex == 3);
    DOCTEST_CHECK(arp.m_motiveIndex == 536870912LL);
    input.m_min = 0.0f;
    input.m_max = 1.0f;
    input.m_pageInterval = 0.125f;
    DOCTEST_CHECK(input.GetOutput(0, 536870913LL) == doctest::Approx(0.125));
    input.m_cycle = true;
    DOCTEST_CHECK(input.GetOutput(0, -1) == doctest::Approx(0.125));
    DOCTEST_CHECK(input.GetOutput(0, 12) == doctest::Approx(0.5));
}

DOCTEST_TEST_CASE("AbsoluteTime: recording spans use only unmodulated global phase")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_unmodulatedPhase = 4.9;
    clock.m_phaseOffset = 2.0;
    for (size_t j = 1; j <= 8; ++j)
    {
        time.TheoryOfTimeBase::Process(j, clock);
    }

    time.RolloverMicroblockBuffer();
    RecordingBuffer recording;
    recording.m_theoryOfTime.store(&time);
    recording.StartRecording();
    recording.Process(0.5f);
    clock.m_unmodulatedPhase = 5.1;
    clock.m_phaseOffset = -2.0;
    for (size_t j = 1; j <= 8; ++j)
    {
        time.TheoryOfTimeBase::Process(j, clock);
    }

    time.RolloverMicroblockBuffer();
    recording.StopRecording();
    DOCTEST_CHECK(recording.m_state == RecordingBuffer::State::Done);
    DOCTEST_CHECK(recording.m_loopPositionRecordingStart.load() == doctest::Approx(4.9));
    DOCTEST_CHECK(recording.m_loopPositionRecordingStop.load() == doctest::Approx(5.1));
    DOCTEST_CHECK(recording.m_recordingRepeats.load() == 8);
}

DOCTEST_TEST_CASE("AbsoluteTime: PolyXFader evaluates accepted reparenting without interpolation travel")
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    for (size_t i = 0; i < 5; ++i)
    {
        clock.m_input[i].m_parentIndex = 5;
        clock.m_input[i].m_parentMult = 1;
    }

    clock.m_input[4].m_parentMult = 2;
    clock.m_input[3].m_parentIndex = 4;
    clock.m_unmodulatedPhase = 9.49;
    time.Process(1, clock);
    clock.m_input[3].m_parentIndex = 5;
    clock.m_input[3].m_parentMult = 3;
    clock.m_unmodulatedPhase = 9.51;
    time.Process(2, clock);
    DOCTEST_CHECK(time.GetCycleRatio(3, 2) == 2);
    clock.m_unmodulatedPhase = 9.99;
    time.Process(3, clock);
    clock.m_unmodulatedPhase = 10.01;
    time.Process(4, clock);
    DOCTEST_REQUIRE(time.GetCycleRatio(3, 4) == 3);

    PolyXFaderInternal lfo;
    PolyXFaderInternal::Input input;
    input.m_theoryOfTime = &time;
    input.m_size = 6;
    input.m_center = 0.5f;
    input.m_mult = 2.5f;
    input.m_phaseShift = -0.75f;
    input.m_attackFrac = 0.5f;
    input.m_shape = 0.5f;
    input.m_samplePosition = 3.5f;
    lfo.Process(input);
    DOCTEST_CHECK(lfo.m_valuesPostQuantize[3] == doctest::Approx(0.0));
    input.m_samplePosition = 3.75f;
    lfo.Process(input);
    DOCTEST_CHECK(lfo.m_valuesPostQuantize[3] == doctest::Approx(0.05));
    input.m_samplePosition = 4.0f;
    lfo.Process(input);
    DOCTEST_CHECK(lfo.m_valuesPostQuantize[3] == doctest::Approx(0.15));
}

DOCTEST_TEST_CASE("AbsoluteTime: MIDI ignores modulation while scope projects modulated phase")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime plain;
    TheoryOfTime modulated;
    TheoryOfTime::Input plainInput;
    TheoryOfTime::Input modulatedInput;
    plainInput.m_running = modulatedInput.m_running = true;
    plainInput.m_unmodulatedPhase = modulatedInput.m_unmodulatedPhase = 7.0;
    plainInput.m_freq = modulatedInput.m_freq = 1.0 / 256.0;
    plainInput.m_modIndex.m_expParam = 0.0;
    modulatedInput.m_modIndex.m_expParam = 2.0;
    modulatedInput.m_phaseModLFOInput.m_attackFrac = 0.5f;
    SmartGrid::MessageOutBuffer plainMessages;
    SmartGrid::MessageOutBuffer modulatedMessages;
    plain.SetupMessageOutBuffer(&plainMessages);
    modulated.SetupMessageOutBuffer(&modulatedMessages);
    auto scope = std::make_unique<ScopeWriter>(1, 1);
    scope->m_advanceStartIndices[0][0] = 0;
    modulated.m_scopeWriter = ScopeWriterHolder(scope.get(), 0, 0);
    double largestOffset = 0.0;
    int clocks = 0;
    for (size_t block = 0; block < 64; ++block)
    {
        plain.RolloverMicroblockBuffer();
        modulated.RolloverMicroblockBuffer();
        for (size_t j = 1; j <= 8; ++j)
        {
            plain.Process(j, plainInput);
            modulated.Process(j, modulatedInput);
            DOCTEST_REQUIRE(plainMessages.m_numMessages == modulatedMessages.m_numMessages);
            for (size_t i = 0; i < plainMessages.m_numMessages; ++i)
            {
                DOCTEST_CHECK(plainMessages.m_messages[i].m_mode == modulatedMessages.m_messages[i].m_mode);
                clocks += plainMessages.m_messages[i].m_mode == SmartGrid::MessageOut::Mode::Clock;
            }

            plainMessages.Clear();
            modulatedMessages.Clear();
            double phase = modulated.GetPhase(5, j, PhaseDomain::Modulated);
            largestOffset = std::max(largestOffset, std::abs(modulatedInput.m_phaseOffset));
            DOCTEST_CHECK(scope->ReadSample(0, 0, j) == doctest::Approx(phase - std::floor(phase)));
        }
    }

    DOCTEST_CHECK(largestOffset > 0.001);
    DOCTEST_CHECK(clocks > 0);
    DOCTEST_CHECK(modulated.GetPhase(5, 8, PhaseDomain::Unmodulated) == doctest::Approx(9.0));
}

DOCTEST_TEST_CASE("AbsoluteTime: external sync follows unmodulated phase at large coordinates")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_unmodulatedPhase = 1048576.25;
    clock.m_phaseOffset = -3.7;
    for (size_t j = 1; j <= 8; ++j)
    {
        time.TheoryOfTimeBase::Process(j, clock);
    }

    time.RolloverMicroblockBuffer();
    ExternalClockSync sync;
    ExternalClockSync::Input input;
    input.m_theoryOfTime = &time;
    input.m_clockTick = true;
    input.m_ticksMultiplier = 4;
    sync.Start();
    sync.Process(input);
    sync.m_samplesSinceLastClock = 99;
    sync.Process(input);
    DOCTEST_CHECK(sync.m_freqOut == doctest::Approx(0.0025));
}

DOCTEST_TEST_CASE("AbsoluteTime: MIDI tick boundaries floor negative phase")
{
    Phasor2Tick ticks;
    ticks.m_divisions = 1;
    DOCTEST_CHECK(ticks.Process(-0.1));
    DOCTEST_CHECK_FALSE(ticks.Process(-0.2));
    DOCTEST_CHECK(ticks.Process(-1.1));
    DOCTEST_CHECK(ticks.Process(0.1));
}

DOCTEST_TEST_CASE("AbsoluteTime: lattice arithmetic floors signed coordinates")
{
    DOCTEST_CHECK(PhaseUtils::FloorDiv(-1, 8) == -1);
    DOCTEST_CHECK(PhaseUtils::FloorMod(-1, 8) == 7);
    DOCTEST_CHECK(PhaseUtils::FloorDiv(-8, 8) == -1);
    DOCTEST_CHECK(PhaseUtils::FloorMod(-8, 8) == 0);
    DOCTEST_CHECK(PhaseUtils::FloorDiv(4294967299LL, 8) == 536870912LL);
    DOCTEST_CHECK(PhaseUtils::FloorMod(4294967299LL, 8) == 3);
    DOCTEST_CHECK(PhaseUtils::FloorDiv(std::numeric_limits<int64_t>::min(), 8) == std::numeric_limits<int64_t>::min() / 8);
    DOCTEST_CHECK(PhaseUtils::FloorMod(std::numeric_limits<int64_t>::min(), 8) == 0);
}
