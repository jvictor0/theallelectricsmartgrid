#include "doctest.h"

#include <cmath>
#include <complex>

#include "Math.hpp"

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
