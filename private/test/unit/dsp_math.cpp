#include "doctest.h"

#include <cmath>
#include <complex>

#include "Math.hpp"
#include "MultiChannelNumber.hpp"

DOCTEST_TEST_CASE_TEMPLATE("Math sine preserves small positive and negative phases", MathType, Math, Math4096)
{
    for (float phase : {0.0f, 1e-12f, 1e-9f, 1e-6f, 1e-4f, 0.125f, 0.25f, 0.5f, 0.75f})
    {
        using Vector = MultiChannelNumber<float, 3>;
        const Vector input(phase, -phase, phase + 1);
        const Vector actual = MathType::Sin2pi(input);
        for (size_t lane = 0; lane < 3; ++lane)
        {
            const double expected = std::sin(2 * M_PI * input[lane]);
            DOCTEST_CAPTURE(input[lane]);
            if (std::abs(expected) > 1e-12)
            {
                DOCTEST_CHECK(actual[lane] == doctest::Approx(expected).epsilon(1e-5));
            }
            else
            {
                DOCTEST_CHECK(std::abs(actual[lane]) < 1e-12);
            }

            DOCTEST_CHECK(actual[lane] == doctest::Approx(MathType::Sin2pi(input[lane])).epsilon(2e-7));
        }
    }

    DOCTEST_CHECK(std::signbit(MathType::Sin2pi(-0.0f)));
    DOCTEST_CHECK(MathType::Sin2pi(0.25f) == 1);
    DOCTEST_CHECK(MathType::Sin2pi(0.75f) == -1);
}

DOCTEST_TEST_CASE_TEMPLATE("Math Polar2pi preserves fractional phase after millions of cycles", MathType, Math, Math4096)
{
    for (double cycles : {0.0, 1000000.0, -1000000.0, 7200000.0, -7200000.0})
    {
        for (double fraction : {27.0 / 2048.0, -253.0 / 2048.0, 1023.0 / 2048.0, 2047.0 / 2048.0})
        {
            for (float magnitude : {0.0f, 0.25f, 1.0f})
            {
                DOCTEST_CAPTURE(cycles);
                DOCTEST_CAPTURE(fraction);
                DOCTEST_CAPTURE(magnitude);
                const double angle = 2.0 * M_PI * fraction;
                const std::complex<double> expected(magnitude * std::cos(angle), magnitude * std::sin(angle));
                const std::complex<double> actual = MathType::Polar2pi(magnitude, cycles + fraction);
                DOCTEST_CHECK(std::abs(actual - expected) < 1e-5);
            }
        }
    }
}

DOCTEST_TEST_CASE_TEMPLATE("Math Polar preserves fractional phase after millions of cycles", MathType, Math, Math4096)
{
    for (double cycles : {0.0, 1000000.0, -1000000.0, 7200000.0, -7200000.0})
    {
        for (double fraction : {27.0 / 2048.0, -253.0 / 2048.0, 1023.0 / 2048.0, 2047.0 / 2048.0})
        {
            for (float magnitude : {0.0f, 0.25f, 1.0f})
            {
                DOCTEST_CAPTURE(cycles);
                DOCTEST_CAPTURE(fraction);
                DOCTEST_CAPTURE(magnitude);
                const double angle = 2.0 * M_PI * fraction;
                const std::complex<double> expected(magnitude * std::cos(angle), magnitude * std::sin(angle));
                const std::complex<double> actual = MathType::Polar(magnitude, 2.0 * M_PI * (cycles + fraction));
                DOCTEST_CHECK(std::abs(actual - expected) < 1e-5);
            }
        }
    }
}
