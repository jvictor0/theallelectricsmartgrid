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
        m_time.m_globalPeriodSamples = 48000.0;
        m_input.m_phaseModLFOInput.m_center = 1.0f;
        m_input.m_phaseModLFOInput.m_slope = 0.0f;
        m_input.m_phaseModLFOInput.m_attackFrac = 0.5f;
        m_input.m_phaseModLFOInput.m_shape = 0.0f;
        m_input.m_lfoMult.m_expParam = 1.0f;
    }

    float Evaluate(float phase)
    {
        TheoryOfTimeBase::Sample& sample = m_time.m_sample;
        m_time.m_unmodulatedPhaseClock.m_phase = phase;
        TheoryOfTimeBase::SetLoopPeriods(sample);
        m_time.ProcessPhaseModLFO(m_input);
        return m_time.m_phaseModLFO.m_dsp.m_values[TheoryOfTimeBase::x_globalLoop][0];
    }
};
}

DOCTEST_TEST_CASE("TimeWarpLFO: Shape blends triangle into sine then progressively saturates")
{
    TimeWarpLFORig rig;
    constexpr float x_phase = 0.125f;
    constexpr float x_triangle = 0.25f;
    const float sine = (1.0f - std::cos(static_cast<float>(M_PI) / 4.0f)) / 2.0f;

    rig.m_input.m_phaseModLFOInput.m_shape = 0.0f;
    DOCTEST_CHECK(rig.Evaluate(x_phase) == doctest::Approx(x_triangle));

    rig.m_input.m_phaseModLFOInput.m_shape = 0.5f;
    DOCTEST_CHECK(rig.Evaluate(x_phase) == doctest::Approx(sine));

    rig.m_input.m_phaseModLFOInput.m_shape = 0.25f;
    DOCTEST_CHECK(rig.Evaluate(x_phase) == doctest::Approx((sine + x_triangle) / 2.0f));

    rig.m_input.m_phaseModLFOInput.m_shape = 0.75f;
    float saturated = rig.Evaluate(0.22f);
    rig.m_input.m_phaseModLFOInput.m_shape = 1.0f;
    DOCTEST_CHECK(rig.Evaluate(0.22f) < saturated);
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

DOCTEST_TEST_CASE("TimeWarpLFO: the fast loop's cap leaves the global loop multiplier alone")
{
    TimeWarpLFORig rig;
    rig.m_time.m_globalPeriodSamples = SampleTimer::x_sampleRate / Tuning::x_maxTempoHz;
    for (size_t loop = 0; loop < TheoryOfTimeBase::x_globalLoop; ++loop)
    {
        rig.m_time.m_sample.m_loops[loop].m_input.m_parentMult = TheoryOfTimeBase::x_maxTopologyMultiplier;
    }

    rig.m_input.m_lfoMult.m_expParam = 16.0f;
    DOCTEST_CHECK(rig.Evaluate(1.0f / 64.0f) == doctest::Approx(0.5f));
    rig.m_input.m_phaseModLFOInput.m_center = 0;
    rig.Evaluate(0.25f / 3125.0f);
    DOCTEST_CHECK(rig.m_time.m_phaseModLFO.m_dsp.m_values[0][0] == doctest::Approx(0.5f));
}

DOCTEST_TEST_CASE("TimeWarpLFO: reads the previous continuous phase while stopped")
{
    TimeWarpLFORig rig;
    constexpr double x_freq = 1.0 / 1024.0;
    rig.m_input.m_freq = x_freq;
    rig.m_input.m_running = true;
    rig.m_time.Process(rig.m_input);
    rig.m_time.Process(rig.m_input);

    rig.m_input.m_running = false;
    rig.m_time.Process(rig.m_input);
    const auto& values = rig.m_time.m_phaseModLFO.m_dsp.m_values;
    DOCTEST_CHECK(values[TheoryOfTimeBase::x_globalLoop][0] == 2.0 * x_freq);
    rig.m_time.Process(rig.m_input);
    DOCTEST_CHECK(values[TheoryOfTimeBase::x_globalLoop][0] == 4.0 * x_freq);
    DOCTEST_CHECK(rig.m_time.m_sample.m_unmodulatedPhase == 0.0);
}

DOCTEST_TEST_CASE("TimeWarpLFO: Center keeps the global source at its blend boundary")
{
    const float boundary = 23.0f / 24.0f;
    const float centers[] = {std::nextafter(boundary, 0.0f), boundary, std::nextafter(boundary, 1.0f)};
    for (float center : centers)
    {
        TimeWarpLFORig rig;
        rig.m_input.m_phaseModLFOInput.m_center = center;
        DOCTEST_CHECK(rig.Evaluate(0.125f) == doctest::Approx(0.25f));
        DOCTEST_CHECK(rig.m_time.m_phaseModLFO.m_dsp.m_totalWeight[0] > 0);
    }
}

DOCTEST_TEST_CASE("TimeWarpLFO: filtered Center motion cannot interrupt the global clock source")
{
    TheoryOfTime moving;
    TheoryOfTime::Input input;
    input.m_running = true;
    input.m_freq = SampleTimer::HzToCycPerSamp(0.25);
    input.m_lfoMult.m_expParam = 16;
    input.m_modIndex.m_expParam = 1;
    input.m_phaseModLFOInput.m_shape = 0;
    input.m_phaseModLFOInput.m_attackFrac = 0.5f;
    input.m_phaseModLFOInput.m_slope = 0;
    OPLowPassFilter encoder;
    encoder.SetAlphaFromNatFreq(static_cast<float>(SampleTimer::HzToCycPerSamp(500.0)));
    double maxSpeedDifference = 0;
    float minWeight = 1;
    bool reachedBoundary = false;
    const int samples = 56000 * static_cast<int>(SampleTimer::x_oversample);
    TheoryOfTime reference;
    for (int n = 1; n <= samples; ++n)
    {
        const float encoderValue = encoder.Process(0.0418f);
        input.m_phaseModLFOInput.m_center = static_cast<float>(1.0 - input.m_lfoCenterFilter.Process(encoderValue)[0]);
        reachedBoundary = reachedBoundary || input.m_phaseModLFOInput.m_center == 23.0f / 24.0f;

        // TheoryOfTime owns its inverter and cannot be copied; mirror the clock
        // state this step depends on instead.
        //
        static_cast<TheoryOfTimeBase&>(reference) = moving;
        reference.m_phaseModLFO = moving.m_phaseModLFO;
        TheoryOfTime::Input referenceInput = input;
        referenceInput.m_phaseModLFOInput.m_center = 1;
        double previousPhase = moving.m_sample.m_modulatedPhase;
        moving.Process(input);
        reference.Process(referenceInput);
        double speed = (moving.m_sample.m_modulatedPhase - previousPhase) / input.m_freq;
        double referenceSpeed = (reference.m_sample.m_modulatedPhase - previousPhase) / input.m_freq;
        maxSpeedDifference = std::max(maxSpeedDifference, std::abs(speed - referenceSpeed));
        minWeight = std::min(minWeight, moving.m_phaseModLFO.m_dsp.m_totalWeight[0]);
    }

    DOCTEST_REQUIRE(reachedBoundary);
    DOCTEST_CHECK(minWeight > 0);
    DOCTEST_CHECK(maxSpeedDifference < 0.1);
}

DOCTEST_TEST_CASE("PolyXFader: square-like Shape responds continuously without quantizer history")
{
    TheoryOfTimeBase time;
    time.m_globalPeriodSamples = 48000.0;
    time.m_unmodulatedPhaseClock.m_phase = 0.22;
    TheoryOfTimeBase::SetLoopPeriods(time.m_sample);

    PolyXFaderInternal fader;
    auto input = PolyXFaderInternal::MakeInput();
    input.m_size = 1;
    input.m_attackFrac = 0.5f;
    input.m_shape = 1.0f;
    input.m_mult = 1.0f;
    input.m_phaseShift = -0.75f;
    fader.Process(input, time, PhaseDomain::Unmodulated);

    float before = fader.m_dsp.m_values[0][0];
    DOCTEST_CHECK(before > 0.0f);
    DOCTEST_CHECK(before < 0.1f);

    time.m_unmodulatedPhaseClock.m_phase += 0.0001;
    fader.Process(input, time, PhaseDomain::Unmodulated);
    DOCTEST_CHECK(fader.m_dsp.m_values[0][0] > before);
    DOCTEST_CHECK(fader.m_dsp.m_values[0][0] - before < 0.01f);

    PolyXFaderInternal fresh;
    fresh.Process(input, time, PhaseDomain::Unmodulated);
    DOCTEST_CHECK(fader.m_dsp.m_values[0][0] == fresh.m_dsp.m_values[0][0]);
}

DOCTEST_TEST_CASE("PolyXFader: Shape morphs fixed linear, sine and saturated sine curves")
{
    for (size_t i = 0; i <= 4096; ++i)
    {
        float position = static_cast<float>(i) / 4096.0f;
        double sine = (1.0 - std::cos(M_PI * position)) * 0.5;
        double t = 8.0 * (2.0 * sine - 1.0);
        double hard = (1.0 + std::clamp(t * (27.0 + t * t) / (27.0 + 9.0 * t * t), -1.0, 1.0)) * 0.5;
        for (float shape : {0.0f, 0.25f, 0.5f, 0.625f, 0.75f, 1.0f})
        {
            double expected = shape <= 0.5f
                ? position + (2.0 * shape) * (sine - position)
                : sine + (2.0 * shape - 1.0) * (hard - sine);
            double actual = PolyXFaderInternal::Shape(shape, position)[0];
            DOCTEST_CHECK(std::abs(actual - expected) < 0.00003);
        }

        float atSine = PolyXFaderInternal::Shape(0.5f, position)[0];
        float aboveSine = PolyXFaderInternal::Shape(std::nextafter(0.5f, 1.0f), position)[0];
        DOCTEST_CHECK(std::abs(aboveSine - atSine) < 0.000001f);
    }
}

DOCTEST_TEST_CASE("PolyXFader: Shape lanes match mono evaluation")
{
    using Quad = PolyXFaderGeneric<4>;
    Quad::Vector shape;
    Quad::Vector position;
    float shapes[4] = {0.2f, 0.5f, 0.6f, 1.0f};
    float positions[4] = {0.15f, 0.4f, 0.65f, 0.9f};
    for (size_t lane = 0; lane < 4; ++lane)
    {
        shape[lane] = shapes[lane];
        position[lane] = positions[lane];
    }

    Quad::Vector quad = Quad::Shape(shape, position);
    for (size_t lane = 0; lane < 4; ++lane)
    {
        DOCTEST_CHECK(quad[lane] == doctest::Approx(PolyXFaderInternal::Shape(shapes[lane], positions[lane])[0]).epsilon(0.000001));
    }
}

DOCTEST_TEST_CASE("PolyXFader: Shape endpoints and sine boundary stay normalized")
{
    auto input = PolyXFaderInternal::MakeInput();
    DOCTEST_CHECK(input.m_shape == 0.0f);

    for (float shape : {0.0f, 0.25f, std::nextafter(0.5f, 0.0f), 0.5f, std::nextafter(0.5f, 1.0f), 0.5625f, 0.75f, 1.0f})
    {
        DOCTEST_CHECK(PolyXFaderInternal::Shape(shape, 0.0f) == doctest::Approx(0.0f).epsilon(0.000001));
        DOCTEST_CHECK(PolyXFaderInternal::Shape(shape, 0.5f) == doctest::Approx(0.5f).epsilon(0.000001));
        DOCTEST_CHECK(PolyXFaderInternal::Shape(shape, 1.0f) == doctest::Approx(1.0f).epsilon(0.000001));

        float previous = 0.0f;
        for (size_t i = 0; i <= 1024; ++i)
        {
            float position = static_cast<float>(i) / 1024.0f;
            float value = PolyXFaderInternal::Shape(shape, position);
            DOCTEST_CHECK(std::isfinite(value));
            DOCTEST_CHECK(value >= 0.0f);
            DOCTEST_CHECK(value <= 1.0f);
            DOCTEST_CHECK(value >= previous);
            DOCTEST_CHECK(value - previous < 0.02f);
            DOCTEST_CHECK(value + PolyXFaderInternal::Shape(shape, 1.0f - position) == doctest::Approx(1.0f).epsilon(0.00001));
            previous = value;
        }
    }

    float sine = PolyXFaderInternal::Shape(0.5f, 0.3f);
    DOCTEST_CHECK(PolyXFaderInternal::Shape(std::nextafter(0.5f, 0.0f), 0.3f) == doctest::Approx(sine).epsilon(0.000001));
}

DOCTEST_TEST_CASE("PolyXFader: increasing Shape above sine progressively sharpens the waveform")
{
    float previous = PolyXFaderInternal::Shape(0.5f, 0.45f);
    for (size_t i = 1; i <= 100; ++i)
    {
        float shape = 0.5f + 0.5f * static_cast<float>(i) / 100.0f;
        float value = PolyXFaderInternal::Shape(shape, 0.45f);
        DOCTEST_CHECK(value < previous);
        previous = value;
    }

    DOCTEST_CHECK(previous < 0.1f);
}
