#include "doctest.h"
#include "TheNonagon.hpp"
#include "PhysicalModelingSource.hpp"
#include "QuadDelay.hpp"
#include "../support/GlobalEnv.hpp"
#include <memory>

namespace
{

TheoryOfTimeBase::Input FlatClock(double phase)
{
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_unmodulatedPhase = phase;
    for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
    {
        clock.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
        clock.m_input[i].m_parentMult = 1;
    }

    return clock;
}

void FillClock(TheoryOfTimeBase& time, TheoryOfTimeBase::Input& clock, double increment)
{
    for (size_t j = 1; j <= TheoryOfTimeBase::x_microBlockSize; ++j)
    {
        clock.m_unmodulatedPhase += increment;
        time.Process(j, clock);
    }
}

}

DOCTEST_TEST_CASE("AbsoluteTime: Nonagon wires source ratio separately from voice gate ratio")
{
    GlobalEnv::ResetPerTest();
    auto nonagon = std::make_unique<TheNonagonInternal>();
    TheNonagonInternal::Input input;
    auto clock = FlatClock(1.25);
    clock.m_input[0].m_parentMult = 7;
    clock.m_input[4].m_parentMult = 3;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    nonagon->m_theoryOfTime.m_globalPeriodSamples = 18000.0;
    for (size_t trio = 0; trio < TheNonagonInternal::x_numTrios; ++trio)
    {
        input.m_arpInput.m_clockSelect[trio] = 4;
        for (size_t bit = 0; bit < TheNonagonInternal::x_numTimeBits; ++bit)
        {
            nonagon->m_lameJuis.m_lanes[trio].m_coMuteState.m_coMutes[bit] = true;
        }
    }

    input.m_running = true;
    input.m_multiPhasorGateInput.m_numTrigs = TheNonagonInternal::x_numVoices;
    input.m_trigLogic.m_trigOnSubTrigger[0] = true;
    nonagon->m_indexArp.m_arp[0].m_triggered = true;
    nonagon->SetMultiPhasorGateInputs(input);
    DOCTEST_REQUIRE(input.m_multiPhasorGateInput.m_trigs[0]);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_phaseRatio == 7.0);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_voiceCycleRatio[0] == 3);
    nonagon->m_multiPhasorGate.Process(input.m_multiPhasorGateInput);
    auto& control = nonagon->m_multiPhasorGate.m_ahdControl[0];
    DOCTEST_REQUIRE(control.m_trig);
    DOCTEST_CHECK(control.m_envelopePeriodSamples == 6000.0);

    AHD envelope;
    AHD::Input envelopeInput;
    envelopeInput.m_theoryOfTime = &nonagon->m_theoryOfTime;
    envelopeInput.m_attackIncrement = 0.0001f;
    envelopeInput.Set(control);
    envelope.Process(envelopeInput);
    envelopeInput.m_trig = false;
    clock.m_unmodulatedPhase = 1.375;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    DOCTEST_CHECK(envelope.Process(envelopeInput) == doctest::Approx(0.525));
}

DOCTEST_TEST_CASE("AbsoluteTime: physical-model slewed hold scales with captured period")
{
    for (double period : {3000.0, 48000.0})
    {
        TheoryOfTimeBase time;
        auto clock = FlatClock(1.0);
        FillClock(time, clock, 0.0);
        time.RolloverMicroblockBuffer();
        PhysicalModelingSource::Input sourceInput;
        sourceInput.SetAHD(0.0f, 1.0f, 0.5f, 1.0f);
        auto& input = sourceInput.m_ahdInput;
        input.m_theoryOfTime = &time;
        for (size_t i = 0; i < 4096; ++i)
        {
            sourceInput.m_ahdInputSetter.Process(input);
        }

        DOCTEST_REQUIRE(input.m_holdLoops == doctest::Approx(16.0));
        AHD::AHDControl control;
        control.m_trig = true;
        control.m_phaseRatio = 1.0;
        control.m_envelopePeriodSamples = period;
        input.Set(control);
        AHD envelope;
        envelope.Process(input);
        input.m_trig = false;
        double attackSamples = 1.0 / input.m_attackIncrement;
        double holdSamples = 16.0 * period;
        double decaySamples = 1.0 / input.m_decayIncrement;
        clock.m_unmodulatedPhase = 1.0 + (attackSamples + holdSamples - 1.0) / period;
        FillClock(time, clock, 0.0);
        time.RolloverMicroblockBuffer();
        envelope.Process(input);
        DOCTEST_CHECK(envelope.m_rawOutput == doctest::Approx(1.0));
        clock.m_unmodulatedPhase = 1.0 + (attackSamples + holdSamples + 0.5 * decaySamples) / period;
        FillClock(time, clock, 0.0);
        time.RolloverMicroblockBuffer();
        envelope.Process(input);
        DOCTEST_CHECK(envelope.m_rawOutput == doctest::Approx(0.5).epsilon(0.01));
    }
}

DOCTEST_TEST_CASE("AbsoluteTime: delay setter follows per-sample phase through rollover and tempo glue")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTime time;
    auto clock = FlatClock(100.0);
    FillClock(time, clock, 0.0);
    time.RolloverMicroblockBuffer();
    time.m_globalPeriodSamples = 64.0;
    QuadDelayInputSetter setter;
    QuadDelayInputSetter::Input input;
    QuadDelay::Input output;
    input.m_theoryOfTime = &time;
    input.m_readHeadSpeedSwitchVal[1] = 6;
    for (size_t block = 0; block < 2; ++block)
    {
        FillClock(time, clock, 1.0 / 64.0);
        for (size_t j = 0; j < TheoryOfTimeBase::x_microBlockSize; ++j)
        {
            SampleTimer::s_instance->m_sample = block * 8 + j;
            setter.Process(input, output, nullptr);
            for (size_t channel = 0; channel < 4; ++channel)
            {
                DOCTEST_CHECK(output.m_writeHeadPosition[channel] == doctest::Approx(block * 8 + j));
                double top = output.m_writeHeadPosition[channel] - Resynthesizer::GetGrainLaunchSamples();
                DOCTEST_CHECK(output.m_readHeadPosition[channel] >= top - 64.0);
                DOCTEST_CHECK(output.m_readHeadPosition[channel] < top);
                DOCTEST_CHECK(output.m_relativeWriteHeadPosition[channel] >= 0.0f);
                DOCTEST_CHECK(output.m_relativeWriteHeadPosition[channel] < 1.0f);
                DOCTEST_CHECK(output.m_relativeReadHeadPosition[channel] >= 0.0f);
                DOCTEST_CHECK(output.m_relativeReadHeadPosition[channel] < 1.0f);
            }
        }

        time.RolloverMicroblockBuffer();
    }

    time.m_globalPeriodSamples = 128.0;
    FillClock(time, clock, 1.0 / 128.0);
    SampleTimer::s_instance->m_sample = 16;
    setter.Process(input, output, nullptr);
    DOCTEST_CHECK(output.m_writeHeadPosition[0] == doctest::Approx(15.0));
    SampleTimer::s_instance->m_sample = 17;
    setter.Process(input, output, nullptr);
    DOCTEST_CHECK(output.m_writeHeadPosition[0] == doctest::Approx(16.0));

    time.RolloverMicroblockBuffer();
    clock.m_running = false;
    FillClock(time, clock, 0.0);
    SampleTimer::s_instance->m_sample = 24;
    setter.Process(input, output, nullptr);
    DOCTEST_CHECK(output.m_writeHeadPosition[0] == doctest::Approx(17.0));
    SampleTimer::s_instance->m_sample = 25;
    setter.Process(input, output, nullptr);
    DOCTEST_CHECK(output.m_writeHeadPosition[0] == doctest::Approx(18.0));
}

DOCTEST_TEST_CASE("WholeTick: voice timing uses undoubled clock and read LCM and captures it at trigger")
{
    GlobalEnv::ResetPerTest();
    auto nonagon = std::make_unique<TheNonagonInternal>();
    TheNonagonInternal::Input input;
    auto clock = FlatClock(0.1);
    clock.m_input[0].m_parentMult = 7;
    clock.m_input[4].m_parentMult = 3;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    nonagon->m_theoryOfTime.m_globalPeriodSamples = 21000.0;
    for (size_t bit = 0; bit < TheNonagonInternal::x_numTimeBits; ++bit)
    {
        nonagon->m_lameJuis.m_lanes[0].m_coMuteState.m_coMutes[bit] = true;
    }

    input.m_arpInput.m_clockSelect[0] = -1;
    nonagon->SetMultiPhasorGateInputs(input);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_voiceCycleRatio[0] == 1);
    input.m_arpInput.m_clockSelect[0] = 4;
    nonagon->SetMultiPhasorGateInputs(input);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_voiceCycleRatio[0] == 3);
    input.m_arpInput.m_clockSelect[0] = -1;
    nonagon->m_lameJuis.m_lanes[0].m_coMuteState.m_coMutes[0] = false;
    nonagon->SetMultiPhasorGateInputs(input);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_voiceCycleRatio[0] == 7);

    input.m_arpInput.m_clockSelect[0] = 4;
    input.m_running = true;
    input.m_multiPhasorGateInput.m_numTrigs = TheNonagonInternal::x_numVoices;
    input.m_trigLogic.m_trigOnSubTrigger[0] = true;
    nonagon->m_indexArp.m_arp[0].m_triggered = true;
    nonagon->SetMultiPhasorGateInputs(input);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_voiceCycleRatio[0] == 21);
    nonagon->m_multiPhasorGate.Process(input.m_multiPhasorGateInput);
    DOCTEST_REQUIRE(nonagon->m_multiPhasorGate.m_gate[0]);
    DOCTEST_CHECK(nonagon->m_multiPhasorGate.m_ahdControl[0].m_envelopePeriodSamples == 1000.0);

    nonagon->m_indexArp.m_arp[0].m_triggered = false;
    nonagon->m_lameJuis.m_lanes[0].m_coMuteState.m_coMutes[0] = true;
    nonagon->m_theoryOfTime.m_globalPeriodSamples = 63000.0;
    nonagon->SetMultiPhasorGateInputs(input);
    DOCTEST_CHECK(input.m_multiPhasorGateInput.m_voiceCycleRatio[0] == 3);
    clock.m_unmodulatedPhase = 0.1 + 0.49 / 21.0;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    nonagon->m_multiPhasorGate.Process(input.m_multiPhasorGateInput);
    DOCTEST_CHECK(nonagon->m_multiPhasorGate.m_gate[0]);
    DOCTEST_CHECK(nonagon->m_multiPhasorGate.m_ahdControl[0].m_envelopePeriodSamples == 1000.0);
    clock.m_unmodulatedPhase = 0.1 + 0.51 / 21.0;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    nonagon->m_multiPhasorGate.Process(input.m_multiPhasorGateInput);
    DOCTEST_CHECK_FALSE(nonagon->m_multiPhasorGate.m_gate[0]);
}

DOCTEST_TEST_CASE("WholeTick: Nonagon clocks and reads tick even when gate values repeat")
{
    GlobalEnv::ResetPerTest();
    auto nonagon = std::make_unique<TheNonagonInternal>();
    TheNonagonInternal::Input input;
    auto clock = FlatClock(0.1);
    clock.m_rhythm[5].m_gate[1] = true;
    input.m_arpInput.m_clockSelect[0] = 5;
    input.m_arpInput.m_resetSelect[0] = -1;
    for (size_t bit = 0; bit < TheNonagonInternal::x_numTimeBits; ++bit)
    {
        nonagon->m_lameJuis.m_lanes[0].m_coMuteState.m_coMutes[bit] = bit != 5;
    }

    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    clock.m_unmodulatedPhase = 0.5;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    nonagon->SetIndexArpInputs(input);
    DOCTEST_CHECK_FALSE(input.m_arpInput.m_clocks[5]);
    DOCTEST_CHECK_FALSE(input.m_arpInput.m_input[0].m_read);
    clock.m_unmodulatedPhase = 1.1;
    FillClock(nonagon->m_theoryOfTime, clock, 0.0);
    nonagon->m_theoryOfTime.RolloverMicroblockBuffer();
    nonagon->SetIndexArpInputs(input);
    nonagon->SetLameJuisInput(input);
    DOCTEST_CHECK(input.m_lameJuisInput.m_inputBitInput[5].m_value);
    DOCTEST_CHECK(input.m_arpInput.m_clocks[5]);
    DOCTEST_CHECK(input.m_arpInput.m_input[0].m_read);
    DOCTEST_CHECK(input.m_arpInput.m_totalIndex[0] == 1);
    input.m_arpInput.m_resetSelect[0] = 5;
    nonagon->SetIndexArpInputs(input);
    DOCTEST_CHECK(input.m_arpInput.m_clocks[5]);
    DOCTEST_CHECK(input.m_arpInput.m_totalIndex[0] == 0);
}
