#include "doctest.h"
#include "ASD.hpp"

namespace
{

struct ASDRig
{
    TheoryOfTimeBase m_time;
    TheoryOfTimeBase::Input m_clock;
    ASD m_envelope;
    ASD::Input m_input;
    ASD::ASDControl m_control;

    ASDRig()
    {
        m_time.m_globalPeriodSamples = 64.0;
        m_clock.m_running = true;
        m_clock.m_unmodulatedPhase = 100.0;
        m_time.Process(m_clock);
        m_input.m_theoryOfTime = &m_time;
        m_input.m_envelopePeriodSamples = 64.0;
        m_input.m_attackIncrement = 0.125f;
        m_input.m_decayIncrement = 0.0625f;
    }

    float Step(double deltaPhase)
    {
        m_clock.m_unmodulatedPhase += deltaPhase;
        m_time.Process(m_clock);
        return m_envelope.Process(m_input);
    }

    void SetGate(bool gate)
    {
        m_control.m_gate = gate;
        m_input.Set(m_control);
    }
};

}

DOCTEST_TEST_CASE("ASD: gate alone selects ramp direction through reverse and stationary time")
{
    ASDRig rig;
    rig.SetGate(true);
    DOCTEST_CHECK(rig.Step(2.0 / 64.0) == 0.25f);
    DOCTEST_CHECK(rig.Step(-2.0 / 64.0) == 0.5f);
    DOCTEST_CHECK(rig.m_input.m_gate);
    rig.m_input.m_attackIncrement = 0.25f;
    DOCTEST_CHECK(rig.Step(0.0) == 0.5f);
    DOCTEST_CHECK(rig.Step(-1.0 / 64.0) == 0.75f);
    rig.SetGate(false);
    DOCTEST_CHECK(rig.Step(-2.0 / 64.0) == 0.625f);
    DOCTEST_CHECK_FALSE(rig.m_input.m_gate);
    rig.m_input.m_decayIncrement = 0.125f;
    DOCTEST_CHECK(rig.Step(2.0 / 64.0) == 0.375f);
    rig.SetGate(true);
    DOCTEST_CHECK(rig.Step(1.0) == 1.0f);
    DOCTEST_CHECK(rig.Step(-1.0) == 1.0f);
    rig.SetGate(false);
    DOCTEST_CHECK(rig.Step(-1.0) == 0.0f);
    DOCTEST_CHECK(rig.Step(1.0) == 0.0f);
}

DOCTEST_TEST_CASE("ASD: stopped tails follow basic tempo without counting phase resets")
{
    ASDRig rig;
    rig.SetGate(true);
    DOCTEST_REQUIRE(rig.Step(4.0 / 64.0) == 0.5f);
    rig.SetGate(false);
    rig.m_clock.m_running = false;
    rig.m_clock.m_phaseOffset = -1000.0;
    DOCTEST_CHECK(rig.Step(0.0) == 0.4375f);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 0.0);
    DOCTEST_CHECK(rig.m_time.GetPhase(TheoryOfTimeBase::x_globalLoop, PhaseDomain::Modulated) == 0.0);
    rig.m_time.m_globalPeriodSamples = 32.0;
    DOCTEST_CHECK(rig.Step(0.0) == 0.3125f);
    rig.m_input.m_decayIncrement = 0.125f;
    DOCTEST_CHECK(rig.Step(0.0) == 0.0625f);
    DOCTEST_CHECK(rig.Step(0.0) == 0.0f);

    rig.SetGate(true);
    rig.m_clock.m_running = true;
    DOCTEST_CHECK(rig.Step(0.0) == 0.25f);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 1.0 / 32.0);
}

DOCTEST_TEST_CASE("ASD: lanes have independent gates rates and phase scaling")
{
    ASDRig rig;
    ASDGeneric<3> trio;
    ASDGeneric<3>::Input input;
    const double x_ratios[] = {1.0, 2.0, 3.0};
    const float x_attack[] = {0.125f, 0.25f, 0.0625f};
    const float x_decay[] = {0.125f, 0.125f, 0.25f};
    for (size_t i = 0; i < 3; ++i)
    {
        input.m_channels[i] = rig.m_input;
        input.m_channels[i].m_phaseRatio = x_ratios[i];
        input.m_channels[i].m_attackIncrement = x_attack[i];
        input.m_channels[i].m_decayIncrement = x_decay[i];
        ASD::ASDControl control;
        control.m_gate = i != 1;
        input.Set(i, control);
    }

    trio.m_channels[1].m_rawOutput = 0.75;
    rig.Step(-1.0 / 64.0);
    auto output = trio.Process(input);
    DOCTEST_CHECK(output[0] == 0.125f);
    DOCTEST_CHECK(output[1] == 0.5f);
    DOCTEST_CHECK(output[2] == 0.1875f);
    rig.Step(1.0 / 64.0);
    output = trio.Process(input);
    DOCTEST_CHECK(output[0] == 0.25f);
    DOCTEST_CHECK(output[1] == 0.25f);
    DOCTEST_CHECK(output[2] == 0.375f);
}

DOCTEST_TEST_CASE("ASD: small increments retain progress near a large output")
{
    ASDRig rig;
    rig.m_clock.m_running = false;
    rig.m_time.Process(rig.m_clock);
    rig.m_envelope.m_rawOutput = 0.75;
    rig.m_input.m_attackIncrement = 1e-9f;
    rig.m_input.m_decayIncrement = 1e-9f;
    rig.SetGate(true);
    for (size_t i = 0; i < 1000; ++i)
    {
        rig.m_envelope.Process(rig.m_input);
    }

    DOCTEST_CHECK(std::abs(rig.m_envelope.m_rawOutput - 0.750001) < 1e-12);
    rig.SetGate(false);
    for (size_t i = 0; i < 1000; ++i)
    {
        rig.m_envelope.Process(rig.m_input);
    }

    DOCTEST_CHECK(std::abs(rig.m_envelope.m_rawOutput - 0.75) < 1e-12);
}

DOCTEST_TEST_CASE("AbsoluteTime: modulated increments follow motion and exclude transport resets")
{
    ASDRig rig;
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 1.0 / 64.0);
    rig.Step(0.25);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 0.25);
    rig.m_clock.m_phaseOffset = -0.5;
    rig.Step(0.0);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == -0.5);
    rig.Step(0.0);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 0.0);
    rig.m_clock.m_running = false;
    rig.m_clock.m_input[0].m_parentMult = 7;
    rig.Step(0.0);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 0.0);
    rig.m_clock.m_running = true;
    rig.Step(100.0);
    DOCTEST_CHECK(rig.m_time.GetModulatedGlobalPhaseIncrement() == 1.0 / 64.0);
}
