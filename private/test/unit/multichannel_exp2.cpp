#include "doctest.h"

#include <algorithm>
#include <cmath>

#include "EncoderBankBank.hpp"

DOCTEST_TEST_CASE("MultiChannelNumber: Exp2 matches std::exp2 to a few ulp")
{
    double maxRelativeError = 0;
    for (int i = -4000; i <= 4000; ++i)
    {
        float x = static_cast<float>(i) / 500.0f;
        QuadFloat result = QuadFloat(x, -x, x * 0.5f, -x * 0.5f).Exp2();
        float inputs[4] = {x, -x, x * 0.5f, -x * 0.5f};
        for (size_t lane = 0; lane < QuadFloat::x_numChannels; ++lane)
        {
            double expected = std::exp2(static_cast<double>(inputs[lane]));
            double relativeError = std::fabs(static_cast<double>(result[lane]) - expected) / expected;
            maxRelativeError = std::max(maxRelativeError, relativeError);
        }
    }

    DOCTEST_CHECK(maxRelativeError < 4.0e-7);
    DOCTEST_CHECK(QuadFloat::Splat(0.0f).Exp2()[0] == 1.0f);
    DOCTEST_CHECK(QuadFloat::Splat(3.0f).Exp2()[0] == 8.0f);
    DOCTEST_CHECK(QuadFloat::Splat(-2.0f).Exp2()[0] == 0.25f);
}

DOCTEST_TEST_CASE("BankedEncoderCell: vector modulation depth tracks the pow curve")
{
    using Cell = SmartGrid::BankedEncoderCell;
    double maxError = 0;
    for (int i = -1200; i <= 1200; ++i)
    {
        float value = static_cast<float>(i) / 1000.0f;
        float bipolar = std::fmin(1.0f, std::fmax(-1.0f, value));
        double expected = std::copysign((std::pow(9.0, std::fabs(static_cast<double>(bipolar))) - 1.0) / 8.0, bipolar);
        maxError = std::max(maxError, std::fabs(static_cast<double>(Cell::ModulationDepthFromValue(value)) - expected));
    }

    DOCTEST_CHECK(maxError < 1.0e-6);
    DOCTEST_CHECK(Cell::ModulationDepthFromValue(0.0f) == 0.0f);
    DOCTEST_CHECK(Cell::ModulationDepthFromValue(1.0f) == doctest::Approx(1.0f).epsilon(1.0e-6));
    DOCTEST_CHECK(Cell::ModulationDepthFromValue(-1.0f) == doctest::Approx(-1.0f).epsilon(1.0e-6));
}
