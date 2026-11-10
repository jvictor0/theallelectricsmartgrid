#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "SquiggleBoyTrack.hpp"

// The quad PolyXFader LFOs run as one four-lane instance. Each lane must match
// a mono instance fed that lane's inputs.
//
namespace
{

TheoryOfTimeBase::Input LoopClock(double phase)
{
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    clock.m_unmodulatedPhase = phase;
    for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
    {
        clock.m_input[i].m_parentIndex = TheoryOfTimeBase::x_globalLoop;
        clock.m_input[i].m_parentMult = static_cast<int>(i + 1);
    }

    return clock;
}

float LaneValue(size_t lane, size_t sample, float base, float step)
{
    float value = base + step * static_cast<float>(lane) + (sample < 2400 ? 0.0f : 0.17f);
    return value - std::floor(value);
}

}

DOCTEST_TEST_CASE("Quad SquiggleLFO: four-lane instance matches mono lanes with independent triggers and phase offsets")
{
    GlobalEnv::ResetPerTest();
    using Quad = SquiggleLFOGeneric<4>;
    using Mono = SquiggleLFOGeneric<1>;
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock = LoopClock(0.1);
    time.Process(clock);
    time.m_globalPeriodSamples = 4800.0;

    Quad quad;
    Quad::Input quadInput;
    Mono mono[4];
    Mono::Input monoInput[4];
    quadInput.m_polyXFaderInput.m_theoryOfTime = &time;
    quadInput.m_polyXFaderInput.m_phaseDomain = PhaseDomain::Modulated;
    for (size_t lane = 0; lane < 4; ++lane)
    {
        monoInput[lane].m_polyXFaderInput.m_theoryOfTime = &time;
        monoInput[lane].m_polyXFaderInput.m_phaseDomain = PhaseDomain::Modulated;
    }

    for (size_t sample = 0; sample < 4800; ++sample)
    {
        QuadFloat mult;
        for (size_t lane = 0; lane < 4; ++lane)
        {
            Quad::PolyXFader::Input& q = quadInput.m_polyXFaderInput;
            Mono::PolyXFader::Input& m = monoInput[lane].m_polyXFaderInput;
            float attack = LaneValue(lane, sample, 0.2f, 0.15f);
            float shape = LaneValue(lane, sample, 0.1f, 0.27f);
            float amplitude = LaneValue(lane, sample, 0.5f, 0.11f);
            float center = LaneValue(lane, sample, 0.05f, 0.23f);
            float slope = LaneValue(lane, sample, 0.3f, 0.19f);
            float phaseShift = LaneValue(lane, sample, 0.4f, 0.31f) * 0.25f * static_cast<float>(lane);
            float shFade = LaneValue(lane, sample, 0.0f, 0.3f);
            bool trig = sample % (300 + 97 * lane) == 0;
            mult[lane] = LaneValue(lane, sample, 0.1f, 0.21f);
            q.m_attackFrac[lane] = attack;
            q.m_shape[lane] = shape;
            q.m_amplitude[lane] = amplitude;
            q.m_center[lane] = center;
            q.m_slope[lane] = slope;
            q.m_phaseShift[lane] = phaseShift;
            q.m_shFade[lane] = shFade;
            q.m_trig[lane] = trig;
            m.m_attackFrac = attack;
            m.m_shape = shape;
            m.m_amplitude = amplitude;
            m.m_center = center;
            m.m_slope = slope;
            m.m_phaseShift = phaseShift;
            m.m_shFade = shFade;
            m.m_trig[0] = trig;
            monoInput[lane].m_mult.Update(mult[lane]);
        }

        quadInput.m_mult.Update(mult);
        quad.Process(quadInput);
        for (size_t lane = 0; lane < 4; ++lane)
        {
            DOCTEST_CAPTURE(sample);
            DOCTEST_CAPTURE(lane);
            mono[lane].Process(monoInput[lane]);
            DOCTEST_CHECK(quad.m_output[lane] == doctest::Approx(mono[lane].m_output[0]).epsilon(0.00001).scale(1.0));
            DOCTEST_CHECK(quad.m_polyXFader.m_output[lane] == doctest::Approx(mono[lane].m_polyXFader.m_output[0]).epsilon(0.00001).scale(1.0));
            DOCTEST_CHECK(quad.m_polyXFader.m_amplitude[lane] == mono[lane].m_polyXFader.m_amplitude[0]);
        }

        clock.m_unmodulatedPhase += 1.0 / 4800.0;
        time.Process(clock);
    }

}

DOCTEST_TEST_CASE("Quad SquiggleLFO: published periods cover each lane's participating loops")
{
    GlobalEnv::ResetPerTest();
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock = LoopClock(0.25);
    clock.m_input[0].m_parentMult = 6;
    clock.m_input[1].m_parentMult = 10;
    time.Process(clock);
    time.m_globalPeriodSamples = 4800.0;

    SquiggleLFOGeneric<4> lfo;
    SquiggleLFOGeneric<4>::Input input;
    auto& mix = input.m_polyXFaderInput;
    mix.m_theoryOfTime = &time;
    mix.m_size = 2;
    mix.m_center = QuadFloat(0.0f, 0.5f, 0.25f, 0.25f);
    mix.m_slope = QuadFloat(0.0f, 0.0f, 0.5f, 0.5f);
    ScopeWriterGeneric<4, 1> writer(1, 128);
    lfo.m_scopeWriter = {&writer, 0};

    for (size_t step = 0; step < 2; ++step)
    {
        lfo.Process(input);
        writer.AdvanceIndex();
        lfo.PopulateScopeState(time);
        writer.Publish();
        writer.AcceptSamples();
        double expected[] = {step == 0 ? 1.0 / 6 : 1.0 / 9, 0.1, step == 0 ? 0.5 : 1.0, step == 0 ? 0.5 : 1.0};
        for (size_t lane = 0; lane < 4; ++lane)
        {
            const auto& markers = writer.GetAcceptedMarkers(0, lane);
            DOCTEST_CHECK(markers.m_periodic.m_period == doctest::Approx(expected[lane]));
            DOCTEST_CHECK(markers.m_periodic.m_origin == time.m_phaseClock.m_offset);
            DOCTEST_CHECK(markers.m_startIndexIndex == 0);
        }

        clock.m_input[0].m_parentMult = 9;
        clock.m_unmodulatedPhase += 1.0;
        time.Process(clock);
    }

    mix.m_size = 0;
    lfo.Process(input);
    DOCTEST_CHECK(lfo.m_polyXFader.ComputeLogicalPeriods(time) == QuadDouble::Splat(1.0));
}
