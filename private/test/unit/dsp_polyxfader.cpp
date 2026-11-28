#include "doctest.h"
#include "TheoryOfTime.hpp"

#include <cmath>

namespace
{
struct PolyXFaderSlewRig
{
    TheoryOfTimeBase m_time;
    PolyXFaderInternal m_fader;
    PolyXFaderInternal::Input m_input = PolyXFaderInternal::MakeInput();
    PhaseDomain m_phaseDomain = PhaseDomain::Unmodulated;

    PolyXFaderSlewRig()
    {
        m_time.m_globalPeriodSamples = 48000.0;
        m_time.m_unmodulatedPhaseClock.m_phase = 0.25;
        m_time.m_sample.m_loops[0].m_input.m_parentMult = 3;
        TheoryOfTimeBase::SetLoopPeriods(m_time.m_sample);
        m_input.m_phaseShift = -0.75f;
        m_input.m_size = 2;
        m_input.m_center = 0.25f;
        m_input.m_slope = 0.5f;
        m_input.m_mult = 2.0f;
        m_input.m_attackFrac = 0.5f;
        m_input.m_shape = 0.0f;
    }

    void Process(size_t samples)
    {
        for (size_t i = 0; i < samples; ++i)
        {
            m_fader.Process(m_input, m_time, m_phaseDomain);
        }
    }

    // Decay of a slew whose cutoff is in cycles per 48000-sample global period.
    //
    static double Decay(double cyclesPerPeriod, size_t samples)
    {
        return std::exp(-2.0 * M_PI * cyclesPerPeriod * static_cast<double>(samples) / 48000.0);
    }

    // Decay of a slew with a fixed cutoff in Hz at the engine rate.
    //
    static double DecayHz(double cutoffHz, size_t samples)
    {
        return std::exp(-2.0 * M_PI * SampleTimer::HzToCycPerSamp(cutoffHz) * static_cast<double>(samples));
    }
};
}

DOCTEST_TEST_CASE("PolyXFader: shaped mix slews at the fastest active frequency before capture")
{
    PolyXFaderSlewRig rig;

    // Isolate mixed-waveform slew and capture from the S&H slew.
    //
    rig.m_fader.m_dsp.m_holdAlpha = 1.0f;
    rig.m_time.m_unmodulatedPhaseClock.m_phase = 0.0625;
    rig.m_input.m_shape = 0.0f;
    rig.m_input.m_externalWeights[0] = 0.5f;
    rig.m_input.m_externalWeights[1] = 1.5f;
    rig.m_input.m_trig[0] = true;
    rig.m_input.m_shFade = 1.0f;
    rig.Process(128);

    double mixed = (0.5 * 0.75 + 1.5 * 0.25) / 2.0;
    double expected = mixed * (1.0 - PolyXFaderSlewRig::Decay(48.0, 128));
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_heldValue[0] == doctest::Approx(expected).epsilon(0.0001));
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 0.75f);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[1][0] == 0.25f);

    rig.m_input.m_trig[0] = false;
    rig.Process(128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: zero-weight components are skipped and excluded from the mix cutoff")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_slope = 0.0f;
    rig.m_input.m_center = 1.0f;
    rig.Process(128);
    DOCTEST_REQUIRE(rig.m_fader.m_dsp.m_weights[0][0] == 0.0f);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 0.0f);
    double expected = 1.0 - PolyXFaderSlewRig::Decay(16.0, 128);
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_center = 0.0f;
    rig.Process(1);
    expected = 1.0 + (expected - 1.0) * PolyXFaderSlewRig::Decay(48.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 1.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_time.m_unmodulatedPhaseClock.m_phase = 0.0;
    rig.Process(1);
    expected *= PolyXFaderSlewRig::Decay(48.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 0.0f);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[1][0] == 1.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: a later component with a small nonzero weight determines the maximum cutoff")
{
    PolyXFaderSlewRig rig;
    rig.m_time.m_sample.m_loops[0].m_input.m_parentIndex = TheoryOfTimeBase::x_globalLoop;
    rig.m_time.m_sample.m_loops[0].m_input.m_parentMult = 1;
    rig.m_time.m_sample.m_loops[1].m_input.m_parentMult = 3;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_sample);
    rig.m_input.m_center = 0.13f;
    rig.Process(128);

    DOCTEST_REQUIRE(rig.m_fader.m_dsp.m_weights[1][0] > 0.0f);
    DOCTEST_REQUIRE(rig.m_fader.m_dsp.m_weights[1][0] < rig.m_fader.m_dsp.m_weights[0][0]);
    double expected = 1.0 - PolyXFaderSlewRig::Decay(48.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: multiplier and tempo changes retune slew without resetting history")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_input.m_mult = 1.0f;
    rig.m_time.m_sample.m_loops[0].m_input.m_parentMult = 1;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_sample);
    rig.m_time.m_unmodulatedPhaseClock.m_phase = 0.5;
    rig.Process(128);
    double expected = 1.0 - PolyXFaderSlewRig::Decay(8.0, 128);
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_mult = 2.0f;
    rig.Process(128);
    expected *= PolyXFaderSlewRig::Decay(16.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 0.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_time.m_globalPeriodSamples = 24000.0;
    rig.Process(128);
    expected *= PolyXFaderSlewRig::Decay(32.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_mult = 2.5f;
    rig.Process(128);
    expected = 0.5 + (expected - 0.5) * PolyXFaderSlewRig::Decay(40.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 0.5f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: skew changes are smoothed after waveform shaping")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_input.m_mult = 1.0f;
    rig.m_input.m_shape = 0.0f;
    rig.m_time.m_unmodulatedPhaseClock.m_phase = 0.125;
    rig.m_time.m_sample.m_loops[0].m_input.m_parentMult = 1;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_sample);
    rig.Process(128);
    double expected = 0.25 * (1.0 - PolyXFaderSlewRig::Decay(8.0, 128));
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_attackFrac = 0.125f;
    rig.Process(1);
    expected = 1.0 + (expected - 1.0) * PolyXFaderSlewRig::Decay(8.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_dsp.m_values[0][0] == 1.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: cutoff uses the waveform interval topology and nominal clock rate")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_time.m_sample.m_modulatedPhase = 10.0;
    rig.Process(128);

    double expected = 1.0 - PolyXFaderSlewRig::Decay(48.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_time.m_sample.m_loops[0].m_input.m_parentMult = 9;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_sample);
    rig.m_time.m_sample.m_modulatedPhase = -10.0;
    rig.Process(1);
    expected = 1.0 + (expected - 1.0) * PolyXFaderSlewRig::Decay(144.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: time-warp mix cutoff follows the global clock frequency")
{
    TheoryOfTime time;
    TheoryOfTime::Input input;
    input.m_running = true;
    input.m_freq = 1.0 / 48000.0;
    input.m_lfoMult.m_expParam = 2.5f;
    input.m_phaseModLFOInput.m_center = 1.0f;
    input.m_phaseModLFOInput.m_shape = 0.0f;
    input.m_phaseModLFOInput.m_attackFrac = 0.5f;
    time.m_unmodulatedPhaseClock.m_phase = 0.1;
    time.Process(input);

    double expected = 0.5 * (1.0 - PolyXFaderSlewRig::Decay(20.0, 1));
    DOCTEST_CHECK(time.m_phaseModLFO.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: zero frequency holds state and high cutoffs stay finite")
{
    PolyXFaderSlewRig rig;
    rig.m_time.m_sample.m_running = true;
    rig.m_time.m_phaseClock.m_phase = 0.25;
    rig.m_time.m_phaseClock.m_increment = 1.0;
    rig.m_phaseDomain = PhaseDomain::Modulated;
    rig.Process(128);
    DOCTEST_REQUIRE(std::isfinite(rig.m_fader.m_rawOutput));
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(1.0f));

    rig.m_input.m_mult = 0.0f;
    rig.Process(128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(1.0f));

    rig.m_input.m_mult = 1.0f;
    rig.m_phaseDomain = PhaseDomain::Unmodulated;
    rig.m_time.m_unmodulatedPhaseClock.m_phase = 0.0;
    rig.Process(128);
    double expected = PolyXFaderSlewRig::Decay(24.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: each loop limits its own waveform multiplier lane by lane")
{
    using Fader = PolyXFaderGeneric<4>;
    TheoryOfTimeBase time;
    time.m_globalPeriodSamples = SampleTimer::x_sampleRate / Tuning::x_maxTempoHz;
    time.m_sample.m_running = true;
    for (size_t loop = 0; loop < TheoryOfTimeBase::x_globalLoop; ++loop)
    {
        time.m_sample.m_loops[loop].m_input.m_parentMult = TheoryOfTimeBase::x_maxTopologyMultiplier;
    }

    TheoryOfTimeBase::SetLoopPeriods(time.m_sample);
    const double fastestRatio = static_cast<double>(time.GetCycleRatio(0));
    time.m_phaseClock.m_phase = 0.25 / fastestRatio;
    time.m_phaseClock.m_increment = 1.0;
    auto input = Fader::MakeInput();
    input.m_size = TheoryOfTimeBase::x_numLoops;
    input.m_phaseShift = QuadFloat::Splat(-0.75f);
    input.m_attackFrac = QuadFloat::Splat(0.5f);
    input.m_mult = QuadFloat(0.5f, 1.0f, 4.0f, 16.0f);
    input.m_slope = QuadFloat::Splat(0.65f);
    Fader fader;
    fader.Process(input, time, PhaseDomain::Modulated);

    DOCTEST_CHECK(input.m_waveMultLimits[0] == 1.0f);
    DOCTEST_CHECK(input.m_waveMultLimits[1] == 5.0f);
    const auto expected = QuadFloat(0.25f, 0.5f, 0.5f, 0.5f);
    DOCTEST_CHECK(fader.m_dsp.m_values[0] == expected);
    DOCTEST_CHECK(Fader::ComputePhase(input, time, PhaseDomain::Modulated, 0) == expected);
    DOCTEST_CHECK(input.m_mult[3] == 16.0f);
    const auto slower = fader.m_dsp.m_values[1];
    DOCTEST_CHECK(slower == Fader::ComputePhase(input, time, PhaseDomain::Modulated, 1));
    DOCTEST_CHECK(slower[2] == doctest::Approx(0.4f));
    DOCTEST_CHECK(slower[3] == doctest::Approx(0.5f));

    time.m_globalPeriodSamples *= 0.5;
    fader.Process(input, time, PhaseDomain::Modulated);
    DOCTEST_CHECK(input.m_waveMultLimits[0] == 0.5f);
    DOCTEST_CHECK(fader.m_dsp.m_values[0] == QuadFloat::Splat(0.25f));

    time.m_globalPeriodSamples *= 4;
    fader.Process(input, time, PhaseDomain::Modulated);
    DOCTEST_CHECK(input.m_waveMultLimits[0] == 2.0f);
    DOCTEST_CHECK(fader.m_dsp.m_values[0] == QuadFloat(0.25f, 0.5f, 1.0f, 1.0f));

    time.m_globalPeriodSamples *= 2.0 / 3.0;
    fader.Process(input, time, PhaseDomain::Modulated);
    DOCTEST_CHECK(input.m_waveMultLimits[0] == std::nextafter(4.0f / 3.0f, 0.0f));
    DOCTEST_CHECK(static_cast<double>(input.m_waveMultLimits[0]) * fastestRatio <=
        Fader::x_maxWaveCycles * time.m_globalPeriodSamples);

    time.m_globalPeriodSamples = SampleTimer::x_sampleRate / Tuning::x_maxTempoHz;
    time.m_sample.m_loops[0].m_input.m_parentMult = 2;
    TheoryOfTimeBase::SetLoopPeriods(time.m_sample);
    fader.Process(input, time, PhaseDomain::Modulated);
    DOCTEST_CHECK(input.m_waveMultLimits[0] == 2.5f);
    DOCTEST_CHECK(fader.m_dsp.m_values[1] == slower);

    time.m_globalPeriodSamples *= 8;
    time.m_phaseClock.m_phase = (1.0 / 64.0) / time.GetCycleRatio(0);
    input.m_mult = QuadFloat::Splat(Fader::x_maxMappedMult);
    const auto clock = Fader::MakeClockInput(time, PhaseDomain::Modulated);
    DOCTEST_CHECK(clock.m_minWaveMultLimit == 20.0f);
    DOCTEST_CHECK(clock.m_waveMultLimits[0] == 20.0f);
    fader.Process(input, clock);
    DOCTEST_CHECK(fader.m_dsp.m_values[0] == QuadFloat::Splat(0.5f));

    time.m_globalPeriodSamples *= 0.8;
    const auto endpointClock = Fader::MakeClockInput(time, PhaseDomain::Modulated);
    DOCTEST_REQUIRE(endpointClock.m_minWaveMultLimit == Fader::x_maxMappedMult);
    input.m_mult[3] = std::nextafter(Fader::x_maxMappedMult, 17.0f);
    fader.Process(input, endpointClock);
    DOCTEST_CHECK(fader.m_dsp.m_values[0] == QuadFloat::Splat(0.5f));
}

DOCTEST_TEST_CASE("PolyXFader: slew uses the fastest contributing loop with its own limited multiplier")
{
    using Fader = PolyXFaderInternal;
    TheoryOfTimeBase time;
    time.m_globalPeriodSamples = SampleTimer::x_sampleRate / Tuning::x_maxTempoHz;
    time.m_sample.m_running = true;
    for (size_t loop = 0; loop < TheoryOfTimeBase::x_globalLoop; ++loop)
    {
        time.m_sample.m_loops[loop].m_input.m_parentMult = TheoryOfTimeBase::x_maxTopologyMultiplier;
    }

    TheoryOfTimeBase::SetLoopPeriods(time.m_sample);
    constexpr double x_travel = 1.0 / (1 << 20);
    time.m_phaseClock.m_phase = 0.125 / 625.0;
    time.m_phaseClock.m_increment = x_travel;
    auto input = Fader::MakeInput();
    input.m_size = 2;
    input.m_phaseShift = -0.75f;
    input.m_attackFrac = 0.5f;
    input.m_mult = 4.0f;
    input.m_center = 1.0f;
    Fader fader;
    fader.Process(input, time, PhaseDomain::Modulated);

    DOCTEST_REQUIRE(fader.m_dsp.m_weights[0][0] == 0.0f);
    DOCTEST_REQUIRE(fader.m_dsp.m_values[1][0] == 1.0f);
    const double slowAlpha = -std::expm1(-2 * M_PI * 8 * 625 * 4 * x_travel);
    DOCTEST_CHECK(fader.m_dsp.m_mixOutput[0] == doctest::Approx(slowAlpha).epsilon(1e-7));

    input.m_center = 0.25f;
    input.m_slope = 0.5f;
    fader.m_dsp.m_mixOutput[0] = 0.0;
    fader.Process(input, time, PhaseDomain::Modulated);
    DOCTEST_REQUIRE(fader.m_dsp.m_weights[0][0] > 0.0f);
    DOCTEST_REQUIRE(fader.m_dsp.m_weights[1][0] > 0.0f);
    const float mixed = (fader.m_dsp.m_values[0][0] * fader.m_dsp.m_weights[0][0] +
        fader.m_dsp.m_values[1][0] * fader.m_dsp.m_weights[1][0]) / fader.m_dsp.m_totalWeight[0];
    const double fastAlpha = -std::expm1(-2 * M_PI * 8 * 3125 * x_travel);
    DOCTEST_CHECK(fader.m_dsp.m_mixOutput[0] == doctest::Approx(fastAlpha * mixed).epsilon(1e-7));
}

DOCTEST_TEST_CASE("PolyXFader: tiny approximate coefficients still advance double slew state")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_input.m_mult = 1.0f;
    rig.m_time.m_globalPeriodSamples = 1e12;
    rig.m_time.m_sample.m_loops[0].m_input.m_parentMult = 1;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_sample);
    rig.m_time.m_unmodulatedPhaseClock.m_phase = 0.5;
    rig.m_fader.m_dsp.m_mixOutput[0] = 0.25;
    rig.Process(1000);

    double expectedAdvance = 0.75 * -std::expm1(-2.0 * M_PI * 8.0 / 1e12 * 1000.0);
    double actualAdvance = rig.m_fader.m_dsp.m_mixOutput[0] - 0.25;
    DOCTEST_CHECK(actualAdvance > 0);
    DOCTEST_CHECK(actualAdvance == doctest::Approx(expectedAdvance).epsilon(0.00001));
}

DOCTEST_TEST_CASE("PolyXFader: 500 Hz slew applies only to S&H before blending and stays warm while dry")
{
    for (float fade : {0.0f, 0.35f, 1.0f})
    {
        PolyXFaderSlewRig rig;
        rig.Process(128);
        double continuous = 1.0 - PolyXFaderSlewRig::Decay(48.0, 128);
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(continuous).epsilon(0.0001));

        rig.m_input.m_shFade = fade;
        rig.m_input.m_trig[0] = true;
        rig.Process(1);
        double captured = 1.0 - PolyXFaderSlewRig::Decay(48.0, 129);
        double held = captured * (1.0 - PolyXFaderSlewRig::DecayHz(500.0, 1));
        DOCTEST_CHECK(rig.m_fader.m_dsp.m_heldValue[0] == doctest::Approx(captured).epsilon(0.0001));
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(fade * held + (1.0 - fade) * captured).epsilon(0.0001));

        rig.m_input.m_trig[0] = false;
        rig.Process(7);
        continuous = 1.0 - PolyXFaderSlewRig::Decay(48.0, 136);
        held = captured * (1.0 - PolyXFaderSlewRig::DecayHz(500.0, 8));
        DOCTEST_CHECK(rig.m_fader.m_dsp.m_heldValue[0] == doctest::Approx(captured).epsilon(0.0001));
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(fade * held + (1.0 - fade) * continuous).epsilon(0.0001));

        rig.m_input.m_shFade = 1.0f;
        rig.Process(1);
        held = captured * (1.0 - PolyXFaderSlewRig::DecayHz(500.0, 9));
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(held).epsilon(0.0001));
    }
}
