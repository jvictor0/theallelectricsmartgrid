#include "doctest.h"
#include "TheoryOfTime.hpp"

#include <cmath>

namespace
{
struct PolyXFaderSlewRig
{
    TheoryOfTimeBase m_time;
    PolyXFaderInternal m_fader;
    PolyXFaderInternal::Input m_input;

    PolyXFaderSlewRig()
    {
        m_time.m_globalPeriodSamples = 48000.0;
        m_time.m_samples[0].m_unmodulatedPhase = 0.25;
        m_time.m_samples[0].m_loops[0].m_input.m_parentMult = 3;
        TheoryOfTimeBase::SetLoopPeriods(m_time.m_samples[0]);
        m_input.m_theoryOfTime = &m_time;
        m_input.m_phaseDomain = PhaseDomain::Unmodulated;
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
            m_fader.Process(m_input);
        }
    }

    static double Decay(double cutoffHz, size_t samples)
    {
        return std::exp(-2.0 * M_PI * cutoffHz * static_cast<double>(samples) / 48000.0);
    }
};
}

DOCTEST_TEST_CASE("PolyXFader: shaped mix slews at the fastest active frequency before capture")
{
    PolyXFaderSlewRig rig;

    // Isolate mixed-waveform slew and capture from the S&H slew.
    //
    rig.m_fader.m_shSlew.m_alpha = 1.0f;
    rig.m_time.m_samples[0].m_unmodulatedPhase = 0.0625;
    rig.m_input.m_shape = 0.0f;
    rig.m_input.m_externalWeights[0] = 0.5f;
    rig.m_input.m_externalWeights[1] = 1.5f;
    rig.m_input.m_trig = true;
    rig.m_input.m_shFade = 1.0f;
    rig.Process(128);

    double mixed = (0.5 * 0.75 + 1.5 * 0.25) / 2.0;
    double expected = mixed * (1.0 - PolyXFaderSlewRig::Decay(48.0, 128));
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
    DOCTEST_CHECK(rig.m_fader.m_shValue == doctest::Approx(expected).epsilon(0.0001));
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 0.75f);
    DOCTEST_CHECK(rig.m_fader.m_values[1] == 0.25f);

    rig.m_input.m_trig = false;
    rig.Process(128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: zero-weight components are skipped and excluded from the mix cutoff")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_slope = 0.0f;
    rig.m_input.m_center = 1.0f;
    rig.Process(128);
    DOCTEST_REQUIRE(rig.m_fader.m_weights[0] == 0.0f);
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 0.0f);
    double expected = 1.0 - PolyXFaderSlewRig::Decay(16.0, 128);
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_center = 0.0f;
    rig.Process(1);
    expected = 1.0 + (expected - 1.0) * PolyXFaderSlewRig::Decay(48.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 1.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_time.m_samples[0].m_unmodulatedPhase = 0.0;
    rig.Process(1);
    expected *= PolyXFaderSlewRig::Decay(48.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 0.0f);
    DOCTEST_CHECK(rig.m_fader.m_values[1] == 1.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: a later component with a small nonzero weight determines the maximum cutoff")
{
    PolyXFaderSlewRig rig;
    rig.m_time.m_samples[0].m_loops[0].m_input.m_parentIndex = TheoryOfTimeBase::x_globalLoop;
    rig.m_time.m_samples[0].m_loops[0].m_input.m_parentMult = 1;
    rig.m_time.m_samples[0].m_loops[1].m_input.m_parentMult = 3;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_samples[0]);
    rig.m_input.m_center = 0.13f;
    rig.Process(128);

    DOCTEST_REQUIRE(rig.m_fader.m_weights[1] > 0.0f);
    DOCTEST_REQUIRE(rig.m_fader.m_weights[1] < rig.m_fader.m_weights[0]);
    double expected = 1.0 - PolyXFaderSlewRig::Decay(48.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: multiplier and tempo changes retune slew without resetting history")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_input.m_mult = 1.0f;
    rig.m_time.m_samples[0].m_loops[0].m_input.m_parentMult = 1;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_samples[0]);
    rig.m_time.m_samples[0].m_unmodulatedPhase = 0.5;
    rig.Process(128);
    double expected = 1.0 - PolyXFaderSlewRig::Decay(8.0, 128);
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_mult = 2.0f;
    rig.Process(128);
    expected *= PolyXFaderSlewRig::Decay(16.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 0.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_time.m_globalPeriodSamples = 24000.0;
    rig.Process(128);
    expected *= PolyXFaderSlewRig::Decay(32.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_mult = 2.5f;
    rig.Process(128);
    expected = 0.5 + (expected - 0.5) * PolyXFaderSlewRig::Decay(40.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 0.5f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: skew changes are smoothed after waveform shaping")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_input.m_mult = 1.0f;
    rig.m_input.m_shape = 0.0f;
    rig.m_time.m_samples[0].m_unmodulatedPhase = 0.125;
    rig.m_time.m_samples[0].m_loops[0].m_input.m_parentMult = 1;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_samples[0]);
    rig.Process(128);
    double expected = 0.25 * (1.0 - PolyXFaderSlewRig::Decay(8.0, 128));
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_attackFrac = 0.125f;
    rig.Process(1);
    expected = 1.0 + (expected - 1.0) * PolyXFaderSlewRig::Decay(8.0, 1);
    DOCTEST_CHECK(rig.m_fader.m_values[0] == 1.0f);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: cutoff uses the waveform interval topology and nominal clock rate")
{
    PolyXFaderSlewRig rig;
    rig.m_input.m_size = 1;
    rig.m_input.m_samplePosition = 1.75f;
    rig.m_time.m_samples[1] = rig.m_time.m_samples[0];
    rig.m_time.m_samples[2] = rig.m_time.m_samples[0];
    rig.m_time.m_samples[2].m_loops[0].m_input.m_parentMult = 9;
    TheoryOfTimeBase::SetLoopPeriods(rig.m_time.m_samples[2]);
    rig.m_time.m_samples[1].m_modulatedPhase = 10.0;
    rig.m_time.m_samples[2].m_modulatedPhase = -10.0;
    rig.Process(128);

    double expected = 1.0 - PolyXFaderSlewRig::Decay(48.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));

    rig.m_input.m_samplePosition = 2.0f;
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
    time.m_samples[0].m_unmodulatedPhase = 0.1;
    time.Process(1, input);

    double expected = 0.5 * (1.0 - PolyXFaderSlewRig::Decay(20.0, 1));
    DOCTEST_CHECK(time.m_phaseModLFO.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
}

DOCTEST_TEST_CASE("PolyXFader: zero frequency holds state and high cutoffs stay finite")
{
    PolyXFaderSlewRig rig;
    rig.m_time.m_globalPeriodSamples = 1.0;
    rig.Process(128);
    DOCTEST_REQUIRE(std::isfinite(rig.m_fader.m_rawOutput));
    DOCTEST_REQUIRE(rig.m_fader.m_rawOutput == doctest::Approx(1.0f));

    rig.m_input.m_mult = 0.0f;
    rig.Process(128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(1.0f));

    rig.m_input.m_mult = 1.0f;
    rig.m_time.m_globalPeriodSamples = 48000.0;
    rig.m_time.m_samples[0].m_unmodulatedPhase = 0.0;
    rig.Process(128);
    double expected = PolyXFaderSlewRig::Decay(24.0, 128);
    DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(expected).epsilon(0.0001));
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
        rig.m_input.m_trig = true;
        rig.Process(1);
        double captured = 1.0 - PolyXFaderSlewRig::Decay(48.0, 129);
        double held = captured * (1.0 - PolyXFaderSlewRig::Decay(500.0, 1));
        DOCTEST_CHECK(rig.m_fader.m_shValue == doctest::Approx(captured).epsilon(0.0001));
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(fade * held + (1.0 - fade) * captured).epsilon(0.0001));

        rig.m_input.m_trig = false;
        rig.Process(7);
        continuous = 1.0 - PolyXFaderSlewRig::Decay(48.0, 136);
        held = captured * (1.0 - PolyXFaderSlewRig::Decay(500.0, 8));
        DOCTEST_CHECK(rig.m_fader.m_shValue == doctest::Approx(captured).epsilon(0.0001));
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(fade * held + (1.0 - fade) * continuous).epsilon(0.0001));

        rig.m_input.m_shFade = 1.0f;
        rig.Process(1);
        held = captured * (1.0 - PolyXFaderSlewRig::Decay(500.0, 9));
        DOCTEST_CHECK(rig.m_fader.m_rawOutput == doctest::Approx(held).epsilon(0.0001));
    }
}
