#include "doctest.h"
#include "PhaseClock.hpp"
#include "ScopeWriter.hpp"
#include "TheoryOfTimeBase.hpp"
#include "PolyXFader.hpp"
#include "TimeWarpInverter.hpp"
#include <array>
#include <cmath>

DOCTEST_TEST_CASE("Phase clock: restart rebases once, tempo changes travel, and rewind stays signed")
{
    PhaseClock clock;
    clock.Update(1000.0, true, 0.01);
    DOCTEST_CHECK(clock.m_phase == 0.0);
    clock.Update(1000.25, true, 0.01);
    DOCTEST_CHECK(clock.m_phase == 0.25);
    double offset = clock.m_offset;
    clock.Update(1000.375, true, 0.125);
    DOCTEST_CHECK(clock.m_phase == 0.375);
    DOCTEST_CHECK(clock.m_offset == offset);
    clock.Update(1000.125, true, 0.125);
    DOCTEST_CHECK(clock.m_increment == -0.25);
    clock.Update(0.0, false, 0.0625);
    DOCTEST_CHECK(clock.m_phase == 0.1875);
    clock.Update(0.0, false, 0.125);
    DOCTEST_CHECK(clock.m_phase == 0.3125);
    clock.Update(-70.0, true, 0.125);
    DOCTEST_CHECK(clock.m_phase == 0.3125);
    DOCTEST_CHECK(clock.m_increment == 0.0);
    clock.Update(-69.75, true, 0.125);
    DOCTEST_CHECK(clock.m_phase == 0.5625);
    DOCTEST_CHECK(clock.m_phase == clock.m_musicalPhase + clock.m_offset);
}

DOCTEST_TEST_CASE("Phase inverse: tempo changes preserve history and forward revisits replace it")
{
    TimeWarpInverter<128> inverse(16.0);
    for (size_t sample = 0; sample <= 32; ++sample)
    {
        double phase = sample <= 16 ? sample / 16.0 : 1.0 + (sample - 16) / 32.0;
        inverse.Write(phase, 100.0 + sample);
    }

    TimeWarpView published{&inverse, 132.0, 1.5, inverse.m_oldestWarpedTime};
    DOCTEST_CHECK(published.GetRealTime(0.5) == 108.0);
    DOCTEST_CHECK(published.GetRealTime(1.25) == 124.0);
    inverse.Write(0.25, 133.0);
    TimeWarpView reversed{&inverse, 133.0, 0.25, inverse.m_oldestWarpedTime};
    DOCTEST_CHECK(reversed.GetRealTime(0.125) == 102.0);
    DOCTEST_CHECK(std::isnan(reversed.GetRealTime(0.5)));
    inverse.Write(0.75, 134.0);
    DOCTEST_CHECK(inverse.GetRealTime(0.5) == 133.5);
    DOCTEST_CHECK(std::isnan(published.GetRealTime(0.5)));
    DOCTEST_CHECK(inverse.GetRealTime(1.25) == 124.0);
}

DOCTEST_TEST_CASE("Phase scopes: explicit startup extent and origin survive tempo changes and rewind")
{
    TimeWarpInverter<8192> inverse(1024.0);
    ScopeWriterGeneric<1, 1> writer(1, 8192);
    writer.SetTimeWarp(0, &inverse);
    writer.SetPeriodic(0, 0, {-0.25, 0.5});
    double phase = -0.25;
    for (size_t sample = 0; sample < 1024; ++sample)
    {
        phase += sample < 512 ? 1.0 / 4096.0 : 1.0 / 2048.0;
        inverse.Write(phase, 1000.0 + sample);
        writer.Write(0, static_cast<float>(std::sin(4 * M_PI * (phase + 0.25))));
        writer.AdvanceIndex();
    }

    writer.Publish();
    writer.AcceptSamples();
    ScopeReader reader(&writer, 0, 0, 256);
    DOCTEST_REQUIRE_FALSE(reader.m_empty);
    DOCTEST_CHECK(writer.GetAcceptedMarkers(0, 0).m_startIndexIndex == 0);
    DOCTEST_CHECK(reader.m_transferXSample == 192.0);
    DOCTEST_CHECK(reader.Coordinate(64) == -0.125);
    DOCTEST_CHECK(reader.Get(64) == doctest::Approx(1.0).epsilon(1e-4));
    DOCTEST_CHECK(reader.Coordinate(224) == -0.3125);
    DOCTEST_CHECK(std::isnan(reader.RealIndex(reader.Coordinate(224))));
    inverse.Write(-0.0625, 2024.0);
    writer.Write(0, 0.0f);
    writer.AdvanceIndex();
    writer.Publish();
    writer.AcceptSamples();
    ScopeReader rewound(&writer, 0, 0, 256);
    DOCTEST_CHECK(rewound.m_transferXSample == 96.0);
    DOCTEST_CHECK(rewound.Get(64) == doctest::Approx(1.0).epsilon(1e-4));
    DOCTEST_CHECK(rewound.m_periodic.m_period == 0.5);
}

DOCTEST_TEST_CASE("Phase axes: negative origins and boundary queries select the latest period")
{
    for (double origin : {-19.25, 0.0, 17.0})
    {
        for (double period : {0.125, 0.5, 1.0})
        {
            PeriodicPhaseAxis axis{origin, period};
            for (double now : {-32.125, -0.25, 0.0, 0.5, 40.25})
            {
                for (size_t point = 0; point < 128; ++point)
                {
                    double position = point / 128.0;
                    double q = axis.Coordinate(now, position);
                    DOCTEST_CHECK(q <= now);
                    DOCTEST_CHECK(now - period < q);
                    double displacement = (q - origin) / period - position;
                    DOCTEST_CHECK(displacement == doctest::Approx(std::round(displacement)));
                }
            }
        }
    }
}

DOCTEST_TEST_CASE("Phase slew: uses signed-clock magnitude, holds at zero and preserves constant-input subdivision")
{
    TheoryOfTimeBase time;
    time.m_sample.m_running = true;
    time.m_sample.m_modulatedPhase = 0.25;
    time.m_sample.m_modulatedPhaseIncrement = 1.0 / 4096.0;
    time.m_globalPeriodSamples = 8192;
    PolyXFaderInternal fader;
    PolyXFaderInternal::Input input;
    input.m_theoryOfTime = &time;
    input.m_size = 1;
    input.m_mult = 1.0f;
    input.m_attackFrac = 0.5f;
    input.m_phaseShift = -0.75f;
    fader.Process(input);
    double alpha = fader.m_mixSlew.m_alpha[0];
    DOCTEST_CHECK(alpha == doctest::Approx(-std::expm1(-2 * M_PI * 8.0 / 4096.0)).epsilon(1e-6));
    time.m_sample.m_modulatedPhaseIncrement *= -1;
    fader.Process(input);
    DOCTEST_CHECK(fader.m_mixSlew.m_alpha[0] == alpha);
    time.m_sample.m_modulatedPhaseIncrement = 0;
    double held = fader.m_mixSlew.m_output[0];
    fader.Process(input);
    DOCTEST_CHECK(fader.m_mixSlew.m_output[0] == held);
    input.m_phaseDomain = PhaseDomain::Unmodulated;
    fader.Process(input);
    DOCTEST_CHECK(fader.m_mixSlew.m_alpha[0] > 0.0);
    DOCTEST_CHECK(fader.m_mixSlew.m_alpha[0] < alpha);
    using Filter = OPLowPassFilterGeneric<double, 1>;
    for (double distance : {1e-8, 0.01, 0.5, 2.0})
    {
        double full = Filter::AlphaFromPhaseTravel(distance)[0];
        double half = Filter::AlphaFromPhaseTravel(distance / 2)[0];
        DOCTEST_CHECK(full == doctest::Approx(1 - (1 - half) * (1 - half)).epsilon(3e-6));
        DOCTEST_CHECK(full == doctest::Approx(-std::expm1(-2 * M_PI * distance)).epsilon(3e-6));
    }
}

DOCTEST_TEST_CASE("Phase slew: forward warped LFO reconstructs the same logical curve")
{
    constexpr size_t x_samples = 32768;
    constexpr size_t x_points = 128;
    std::array<float, x_points> reference{};
    for (double depth : {0.0, 0.75})
    {
        TheoryOfTimeBase time;
        time.m_sample.m_running = true;
        time.m_globalPeriodSamples = x_samples / 4;
        PolyXFaderInternal fader;
        PolyXFaderInternal::Input input;
        input.m_theoryOfTime = &time;
        input.m_size = 1;
        input.m_mult = 2.5f;
        input.m_attackFrac = 0.3f;
        input.m_phaseShift = -0.75f;
        input.m_shape = 0.4f;
        TimeWarpInverter<32768> inverse(4096.0);
        ScopeWriterGeneric<1, 1> writer(1, x_samples + 1);
        writer.SetTimeWarp(0, &inverse);
        writer.SetPeriodic(0, 0, {0.0, 1.0});
        double previous = 0;
        for (size_t sample = 0; sample <= x_samples; ++sample)
        {
            double t = 4.0 * sample / x_samples;
            double phase = t + depth / (2 * M_PI) * std::sin(2 * M_PI * t);
            time.m_sample.m_modulatedPhase = phase;
            time.m_sample.m_modulatedPhaseIncrement = phase - previous;
            previous = phase;
            fader.Process(input);
            inverse.Write(phase, sample);
            writer.Write(0, fader.m_output);
            writer.AdvanceIndex();
        }

        writer.Publish();
        writer.AcceptSamples();
        ScopeReader reader(&writer, 0, 0, x_points);
        double maxError = 0;
        for (size_t point = 0; point < x_points; ++point)
        {
            float value = reader.ReadCoordinate(2.0 + static_cast<double>(point) / x_points);
            if (depth == 0)
            {
                reference[point] = value;
            }
            else
            {
                maxError = std::max(maxError, std::abs(static_cast<double>(value - reference[point])));
            }
        }

        DOCTEST_CHECK(maxError < 0.001);
    }
}
