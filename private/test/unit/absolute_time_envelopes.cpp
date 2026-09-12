#include "doctest.h"

#include "AHD.hpp"
#include "MultiPhasorGate.hpp"

namespace
{

void SetGlobalPhase(TheoryOfTimeBase& time, double phase)
{
    for (TheoryOfTimeBase::Sample& sample : time.m_samples)
    {
        sample.m_unmodulatedPhase = phase;
        sample.m_modulatedPhase = phase;
    }
}

void ConfigureEnvelope(AHD::Input& input, TheoryOfTimeBase& time)
{
    input.m_theoryOfTime = &time;
    input.m_samplePosition = 0.0f;
    input.m_attackIncrement = 0.001f;
    input.m_holdLoops = 0.25;
    input.m_decayIncrement = 0.001f;
    input.m_amplitude = 1.0f;
    input.m_amplitudePolarity = true;
}

AHD::AHDControl TriggerControl(double phaseRatio, double envelopePeriodSamples)
{
    AHD::AHDControl control;
    control.m_phaseRatio = phaseRatio;
    control.m_envelopePeriodSamples = envelopePeriodSamples;
    control.m_trig = true;
    return control;
}

MultiPhasorGateInternal::Input GateInput(
    TheoryOfTimeBase& time,
    bool trig,
    int64_t voiceCycleRatio,
    double globalPeriodSamples,
    double phaseRatio)
{
    MultiPhasorGateInternal::Input input;
    input.m_theoryOfTime = &time;
    input.m_globalPeriodSamples = globalPeriodSamples;
    input.m_numTrigs = 1;
    input.m_trigs[0] = trig;
    input.m_newTrigCanStart[0] = true;
    input.m_voiceCycleRatio[0] = voiceCycleRatio;
    input.m_phaseRatio = phaseRatio;
    return input;
}

}

DOCTEST_TEST_CASE("AHD keeps trigger timing after an accepted topology edit")
{
    const double origins[] = {1.75, 1.45, 1.25};
    const float outputsAtEdit[] = {0.5f, 1.0f, 0.75f};
    for (size_t stage = 0; stage < 3; ++stage)
    {
        TheoryOfTimeBase referenceTime;
        TheoryOfTimeBase editedTime;
        TheoryOfTimeBase::Input referenceClock;
        referenceClock.m_running = true;
        for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
        {
            referenceClock.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
            referenceClock.m_input[i].m_parentMult = 1;
        }

        referenceClock.m_input[4].m_parentMult = 2;
        referenceClock.m_input[0].m_parentIndex = 4;
        TheoryOfTimeBase::Input editedClock = referenceClock;
        auto advance = [](TheoryOfTimeBase& time, TheoryOfTimeBase::Input& clock, double phase)
        {
            clock.m_unmodulatedPhase = phase;
            for (size_t j = 1; j <= TheoryOfTimeBase::x_microBlockSize; ++j)
            {
                time.Process(j, clock);
            }

            time.RolloverMicroblockBuffer();
        };

        advance(referenceTime, referenceClock, origins[stage]);
        advance(editedTime, editedClock, origins[stage]);
        AHD reference;
        AHD edited;
        AHD::Input referenceInput;
        AHD::Input editedInput;
        ConfigureEnvelope(referenceInput, referenceTime);
        ConfigureEnvelope(editedInput, editedTime);
        AHD::AHDControl referenceControl = TriggerControl(referenceTime.GetCycleRatio(0, 0), 1000.0);
        AHD::AHDControl editedControl = TriggerControl(editedTime.GetCycleRatio(0, 0), 1000.0);
        referenceInput.Set(referenceControl);
        editedInput.Set(editedControl);
        reference.Process(referenceInput);
        edited.Process(editedInput);

        editedClock.m_input[0].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
        editedClock.m_input[0].m_parentMult = 6;
        referenceControl.m_trig = false;
        editedControl.m_trig = false;
        editedControl.m_envelopePeriodSamples = 2000.0;
        const double phases[] = {1.99, 2.0, 2.05};
        for (double phase : phases)
        {
            advance(referenceTime, referenceClock, phase);
            advance(editedTime, editedClock, phase);
            editedControl.m_phaseRatio = editedTime.GetCycleRatio(0, 0);
            referenceInput.Set(referenceControl);
            editedInput.Set(editedControl);
            reference.Process(referenceInput);
            edited.Process(editedInput);
            DOCTEST_CHECK(edited.m_rawOutput == doctest::Approx(reference.m_rawOutput));
            if (phase < 2.0)
            {
                DOCTEST_REQUIRE(editedTime.GetLoop(0, 0).m_input.m_parentIndex == 4);
                DOCTEST_REQUIRE(editedTime.GetCycleRatio(0, 0) == 2);
            }
            else
            {
                DOCTEST_REQUIRE(editedTime.GetLoop(0, 0).m_input.m_parentIndex == 5);
                DOCTEST_REQUIRE(editedTime.GetCycleRatio(0, 0) == 6);
            }

            if (phase == 2.0)
            {
                DOCTEST_CHECK(edited.m_rawOutput == doctest::Approx(outputsAtEdit[stage]));
            }
        }
    }
}

DOCTEST_TEST_CASE("AHD numeric progress uses captured values until retrigger")
{
    TheoryOfTimeBase time;
    SetGlobalPhase(time, 10.0);

    AHD oldNote;
    AHD::Input oldInput;
    ConfigureEnvelope(oldInput, time);
    oldInput.m_attackIncrement = 0.0001f;
    oldInput.m_holdLoops = 0.0;

    AHD::AHDControl control = TriggerControl(4.0, 1000.0);
    oldInput.Set(control);
    oldNote.Process(oldInput);

    control.m_trig = false;
    control.m_phaseRatio = 6.0;
    control.m_envelopePeriodSamples = 2000.0;
    oldInput.Set(control);
    SetGlobalPhase(time, 10.125);
    DOCTEST_CHECK(oldNote.Process(oldInput) == doctest::Approx(0.05f));

    AHD newNote;
    AHD::Input newInput;
    ConfigureEnvelope(newInput, time);
    newInput.m_attackIncrement = 0.0001f;
    newInput.m_holdLoops = 0.0;
    control.m_trig = true;
    newInput.Set(control);
    newNote.Process(newInput);
    newInput.m_trig = false;
    oldInput.Set(control);
    DOCTEST_CHECK(oldNote.Process(oldInput) == doctest::Approx(0.05f));
    oldInput.m_trig = false;

    SetGlobalPhase(time, 10.25);
    DOCTEST_CHECK(newNote.Process(newInput) == doctest::Approx(0.15f));
    DOCTEST_CHECK(oldNote.Process(oldInput) == doctest::Approx(0.20f));
}

DOCTEST_TEST_CASE("AHD uses absolute displacement when global phase reverses")
{
    TheoryOfTimeBase time;
    SetGlobalPhase(time, 5.0);

    AHD envelope;
    AHD::Input input;
    ConfigureEnvelope(input, time);
    input.m_attackIncrement = 0.0001f;
    input.m_holdLoops = 0.0;
    AHD::AHDControl control = TriggerControl(2.0, 1000.0);
    input.Set(control);
    envelope.Process(input);
    input.m_trig = false;

    SetGlobalPhase(time, 4.75);
    DOCTEST_CHECK(envelope.Process(input) == doctest::Approx(0.05f));
}

DOCTEST_TEST_CASE("MultiPhasorGate captures global origin voice ratio and period")
{
    TheoryOfTimeBase time;
    SetGlobalPhase(time, 10.0);
    MultiPhasorGateInternal gate;

    MultiPhasorGateInternal::Input input = GateInput(time, true, 4, 1000.0, 8.0);
    gate.Process(input);
    DOCTEST_REQUIRE(gate.m_gate[0]);
    DOCTEST_CHECK(gate.m_ahdControl[0].m_phaseRatio == doctest::Approx(8.0));
    DOCTEST_CHECK(gate.m_ahdControl[0].m_envelopePeriodSamples == doctest::Approx(250.0));

    input.m_trigs[0] = false;
    input.m_voiceCycleRatio[0] = 8;
    input.m_globalPeriodSamples = 4000.0;
    input.m_phaseRatio = 3.0;
    SetGlobalPhase(time, 10.0625);
    gate.Process(input);
    DOCTEST_CHECK(gate.m_gate[0]);
    DOCTEST_CHECK(gate.m_ahdControl[0].m_envelopePeriodSamples == doctest::Approx(250.0));

    SetGlobalPhase(time, 10.125);
    gate.Process(input);
    DOCTEST_CHECK_FALSE(gate.m_gate[0]);
    DOCTEST_CHECK_FALSE(gate.m_ahdControl[0].m_release);

    input.m_trigs[0] = true;
    gate.Process(input);
    DOCTEST_CHECK(gate.m_ahdControl[0].m_envelopePeriodSamples == doctest::Approx(500.0));
    DOCTEST_CHECK(gate.m_ahdControl[0].m_phaseRatio == doctest::Approx(3.0));

    input.m_trigs[0] = false;
    input.m_newTrigCanStart[0] = false;
    SetGlobalPhase(time, 10.1875);
    gate.Process(input);
    DOCTEST_CHECK(gate.m_ahdControl[0].m_release);
    DOCTEST_CHECK_FALSE(gate.m_set[0]);
}

DOCTEST_TEST_CASE("MultiPhasorGate applies the same threshold in reverse")
{
    TheoryOfTimeBase time;
    SetGlobalPhase(time, 3.0);
    MultiPhasorGateInternal gate;
    MultiPhasorGateInternal::Input input = GateInput(time, true, 4, 1000.0, 2.0);
    gate.Process(input);
    DOCTEST_REQUIRE(gate.m_gate[0]);

    input.m_trigs[0] = false;
    SetGlobalPhase(time, 2.875);
    gate.Process(input);
    DOCTEST_CHECK_FALSE(gate.m_gate[0]);
}

DOCTEST_TEST_CASE("AHD live hold control uses the captured period")
{
    TheoryOfTimeBase time;
    SetGlobalPhase(time, 1.0);
    AHD envelope;
    AHD::Input input;
    ConfigureEnvelope(input, time);
    AHD::AHDControl control = TriggerControl(2.0, 1000.0);
    input.Set(control);
    envelope.Process(input);
    control.m_trig = false;
    control.m_envelopePeriodSamples = 2000.0;
    input.Set(control);
    SetGlobalPhase(time, 1.75);
    DOCTEST_CHECK(envelope.Process(input) == doctest::Approx(0.75f));
    input.m_holdLoops = 0.5;
    DOCTEST_CHECK(envelope.Process(input) == doctest::Approx(1.0f));
    input.m_holdLoops = 0.0;
    DOCTEST_CHECK(envelope.Process(input) == doctest::Approx(0.5f));
}
