#include "doctest.h"

#include <cmath>

#include "Filter.hpp"
#include "Lissajous.hpp"
#include "PartialMachine.hpp"

namespace
{

float SaturateAxis(float radius, float value)
{
    TanhSaturator<false> saturator(radius);
    return saturator.Process(value);
}

}

DOCTEST_TEST_CASE("Lissajous radius is zeroed exp from 0 to 5 with 1 at center")
{
    LissajousLFOInternal::Input input;
    DOCTEST_CHECK(input.m_radius.m_expParam == doctest::Approx(1.0f).epsilon(1e-5));

    input.m_radius.Update(0.0f);
    DOCTEST_CHECK(input.m_radius.m_expParam == doctest::Approx(0.0f).epsilon(1e-5));

    input.m_radius.Update(1.0f);
    DOCTEST_CHECK(input.m_radius.m_expParam == doctest::Approx(5.0f).epsilon(1e-5));
}

DOCTEST_TEST_CASE("Lissajous radius 1 traces an unnormalized tanh circle")
{
    LissajousLFOInternal::Input input;
    std::pair<float, float> xy = input.Compute(0.0f);
    DOCTEST_CHECK(xy.first == doctest::Approx(SaturateAxis(1.0f, 1.0f)).epsilon(1e-5));
    DOCTEST_CHECK(xy.second == doctest::Approx(0.0f).epsilon(1e-5));
}

DOCTEST_TEST_CASE("Lissajous radius 0 collapses to the center")
{
    LissajousLFOInternal::Input input;
    input.m_radius.Update(0.0f);
    input.m_centerX = 0.5f;
    input.m_centerY = -0.5f;
    std::pair<float, float> xy = input.Compute(0.3f);
    DOCTEST_CHECK(xy.first == doctest::Approx(SaturateAxis(1.0f, 0.5f)).epsilon(1e-5));
    DOCTEST_CHECK(xy.second == doctest::Approx(SaturateAxis(1.0f, -0.5f)).epsilon(1e-5));
}

DOCTEST_TEST_CASE("Lissajous high radius squishes the circle toward a square")
{
    LissajousLFOInternal::Input input;
    std::pair<float, float> circleXY = input.Compute(0.125f);

    input.m_radius.Update(1.0f);
    std::pair<float, float> squareXY = input.Compute(0.125f);

    DOCTEST_CHECK(circleXY.first == doctest::Approx(SaturateAxis(1.0f, Math::Sin2pi(0.375f))).epsilon(1e-5));
    DOCTEST_CHECK(circleXY.second == doctest::Approx(SaturateAxis(1.0f, Math::Sin2pi(0.125f))).epsilon(1e-5));
    DOCTEST_CHECK(squareXY.first == doctest::Approx(SaturateAxis(5.0f, Math::Sin2pi(0.375f))).epsilon(1e-5));
    DOCTEST_CHECK(squareXY.second == doctest::Approx(SaturateAxis(5.0f, Math::Sin2pi(0.125f))).epsilon(1e-5));
    DOCTEST_CHECK(std::fabs(squareXY.first) > std::fabs(circleXY.first));
    DOCTEST_CHECK(std::fabs(squareXY.second) > std::fabs(circleXY.second));
}

DOCTEST_TEST_CASE("PartialMachine pan uses the same unnormalized tanh")
{
    std::pair<float, float> center = PartialMachine::SynthesisContext::GetPanCoordinates(0.0f, 0.0f);
    DOCTEST_CHECK(center.first == doctest::Approx(0.5f).epsilon(1e-5));
    DOCTEST_CHECK(center.second == doctest::Approx(0.5f).epsilon(1e-5));

    std::pair<float, float> circle = PartialMachine::SynthesisContext::GetPanCoordinates(0.0f, 1.0f);
    DOCTEST_CHECK(circle.first == doctest::Approx(0.5f + 0.5f * SaturateAxis(2.0f, 1.0f)).epsilon(1e-5));
    DOCTEST_CHECK(circle.second == doctest::Approx(0.5f).epsilon(1e-5));

    QuadFloat distribution = PartialMachine::SynthesisContext::Pan(0.0f, 1.0f);
    DOCTEST_CHECK(distribution[0] == doctest::Approx(QuadFloat::Pan(circle.first, circle.second, 1.0f)[0]).epsilon(1e-5));
    DOCTEST_CHECK(distribution[1] == doctest::Approx(QuadFloat::Pan(circle.first, circle.second, 1.0f)[1]).epsilon(1e-5));
}
