#include "doctest.h"
#include "TheoryOfTime.hpp"

#include <cmath>

namespace
{
struct TimeWarpLFORig
{
    TheoryOfTime m_time;
    TheoryOfTime::Input m_input;

    TimeWarpLFORig()
    {
        m_input.m_phaseModLFOInput.m_center = 1.0f;
        m_input.m_phaseModLFOInput.m_slope = 0.0f;
        m_input.m_phaseModLFOInput.m_attackFrac = 0.5f;
        m_input.m_phaseModLFOInput.m_shape = 1.0f;
        m_input.m_lfoMult.m_expParam = 1.0f;
    }

    float Evaluate(float phase)
    {
        TheoryOfTimeBase::Sample& sample = m_time.m_samples[0];
        sample.m_unmodulatedPhase = phase;
        TheoryOfTimeBase::SetLoopPeriods(sample);
        m_time.ProcessPhaseModLFO(1, m_input);
        return m_time.m_phaseModLFO.m_valuesPostQuantize[TheoryOfTimeBase::x_globalLoop];
    }
};
}

DOCTEST_TEST_CASE("TimeWarpLFO: Shape continuously blends sine into triangle")
{
    TimeWarpLFORig rig;
    constexpr float x_phase = 0.125f;
    constexpr float x_triangle = 0.25f;
    const float sine = (1.0f - std::cos(static_cast<float>(M_PI) / 4.0f)) / 2.0f;

    rig.m_input.m_phaseModLFOInput.m_shape = 1.0f;
    DOCTEST_CHECK(rig.Evaluate(x_phase) == doctest::Approx(x_triangle));

    rig.m_input.m_phaseModLFOInput.m_shape = 0.0f;
    DOCTEST_CHECK(rig.Evaluate(x_phase) == doctest::Approx(sine));

    rig.m_input.m_phaseModLFOInput.m_shape = 0.5f;
    DOCTEST_CHECK(rig.Evaluate(x_phase) == doctest::Approx((sine + x_triangle) / 2.0f));
}

DOCTEST_TEST_CASE("TimeWarpLFO: Skew endpoints map to ten and ninety percent")
{
    TimeWarpLFORig rig;

    rig.m_input.m_phaseModLFOInput.m_attackFrac = 0.0f;
    DOCTEST_CHECK(rig.Evaluate(0.05f) == doctest::Approx(0.5f));
    DOCTEST_CHECK(rig.m_input.m_phaseModLFOInput.m_attackFrac == doctest::Approx(0.0f));
    DOCTEST_CHECK(rig.Evaluate(0.05f) == doctest::Approx(0.5f));
    DOCTEST_CHECK(rig.m_input.m_phaseModLFOInput.m_attackFrac == doctest::Approx(0.0f));

    rig.m_input.m_phaseModLFOInput.m_attackFrac = 1.0f;
    DOCTEST_CHECK(rig.Evaluate(0.45f) == doctest::Approx(0.5f));
    DOCTEST_CHECK(rig.m_input.m_phaseModLFOInput.m_attackFrac == doctest::Approx(1.0f));
}

DOCTEST_TEST_CASE("TimeWarpLFO: high multipliers retain continuous steps")
{
    TimeWarpLFORig rig;
    rig.m_input.m_lfoMult.m_expParam = 16.0f;

    float previous = rig.Evaluate(0.0f);
    for (size_t i = 1; i <= 4096; ++i)
    {
        float phase = static_cast<float>(i) / 4096.0f;
        float current = rig.Evaluate(phase);
        DOCTEST_CHECK(std::abs(current - previous) < 0.02f);
        previous = current;
    }
}

DOCTEST_TEST_CASE("TimeWarpLFO: fractional Mult preserves the partial lobe")
{
    TimeWarpLFORig rig;
    rig.m_input.m_lfoMult.m_expParam = 2.5f;
    DOCTEST_CHECK(rig.Evaluate(0.9f) == doctest::Approx(0.5f));

    rig.m_input.m_lfoMult.m_expParam = 1.0f;
    DOCTEST_CHECK(rig.Evaluate(0.75f) == doctest::Approx(0.5f));
}

DOCTEST_TEST_CASE("PolyXFader: default Shape mode keeps stepped quantization")
{
    TheoryOfTimeBase time;
    time.m_samples[0].m_unmodulatedPhase = 0.125;
    TheoryOfTimeBase::SetLoopPeriods(time.m_samples[0]);

    PolyXFaderInternal fader;
    PolyXFaderInternal::Input input;
    input.m_theoryOfTime = &time;
    input.m_phaseDomain = PhaseDomain::Unmodulated;
    input.m_size = 1;
    input.m_attackFrac = 0.5f;
    input.m_shape = 1.0f;
    input.m_mult = 1.0f;
    input.m_phaseShift = -0.75f;
    fader.Process(input);

    DOCTEST_CHECK(fader.m_valuesPostQuantize[0] == doctest::Approx(0.0f));
}
